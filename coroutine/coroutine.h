#ifndef _COROUTINE_H_
#define _COROUTINE_H_
#include <inttypes.h>
#include <stddef.h>
#include <sys/epoll.h>
#include <sys/poll.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <functional>
#include <queue>
#include <set>
#include <chrono>
#include <assert.h>
using namespace std::chrono;


#define MAX_EVENTS 1024
#define MAX_STACKSIZE	(4*1024)  // 最大的栈大小

extern pthread_key_t global_sched_key;
extern pthread_once_t sched_key_once; // 在线程中只执行一次

#define BIT(x)	 				(1 << (x))
#define CLEARBIT(x)             ~(1 << (x))

struct cpu_ctx {
    void *rsp; // 栈顶指针
    void *rbp; // 被调用者保存，用作数据存储
    void *rip; // 下一条指令
    void *rbx; // 被调用者保存
    void *r12; // 被调用者保存
    void *r13; // 被调用者保存
    void *r14; // 被调用者保存
    void *r15; // 被调用者保存
};

enum CorStatus {
    STATUS_READY = 0,
    STATUS_WAIT_READ,
    STATUS_WAIT_WRITE,
    STATUS_SLEEPING,
    STATUS_EXIT,
    STATUS_DETACH,
    STATUS_NEW,
    STATUS_FDEOF

};


struct Schedule;
extern Schedule* schedule_create();


class Coroutine {
public:
    Coroutine() : stack_addr(NULL) {};

    struct TaskFunc
    {
        TaskFunc(std::function<void()> func = NULL) : _func(func)
        { }

        std::function<void()>   _func;
    };
    //Coroutine(Coroutine **new_co, coroutine_entry func, void *arg);
    template <typename Entry, typename ...Args>
    Coroutine(Coroutine **new_co, Entry &&f, Args &&... args);

    template <typename Entry, typename ...Args>
    friend void coroutine_create(Coroutine **new_co, Entry &&f, Args &&... args);

    ~Coroutine();

    int yield();
    int resume();
    void sleep_for(uint64_t msec);    
    void detach();

// 允许以下的函数和类对协程进行修改
    friend void coroutine_free(Coroutine *co);
    friend void _exec(void *cor);
    friend void coroutine_init(Coroutine *co);
    friend int poll_inner(pollfd *fds, nfds_t num_fd, int timeout);
    friend struct SleepCmp;
    friend struct WaitCmp;
    friend struct Schedule;


private:
    cpu_ctx ctx; // 用来 save 上下文
    TaskFunc task;
    void *stack_addr;
    size_t stack_size;
    size_t last_stack_size; //剩余栈空间
    u_int64_t id;

    // 一个 fd，对应一个事件，在一个协程中运行
    int fd;
    unsigned short events;
    int status;

    // 协程所属的调度器
    Schedule *sche;

    int precedence; // 运行优先级
    uint64_t sleep_usecs; //睡眠时间
    
};

struct SleepCmp {
    bool operator()(const Coroutine *co1, const Coroutine *co2) const {
        return co1->sleep_usecs < co2->sleep_usecs;
    }
};

struct WaitCmp {
    bool operator()(const Coroutine *co1, const Coroutine *co2) const {
        return co1->fd < co2->fd;
    }
};


// 调度器的定义
class Schedule {
public:
    Schedule(int stack_size);
    ~Schedule();

    static Schedule *get_schedule(void);
    static void sched_key_destructor(void *data);
    static int schedule_epoll(Schedule *sched);
    static void schedule_init_key();
    // 获取sleep 超时的 coroutine
    static Coroutine *get_expired_coroutine(Schedule *sched);
    // 获取 sleep 可能最近超时的时间间隔
    static uint64_t get_min_timeout(Schedule *sched);

    void del_sleep(Coroutine *co);
    void add_sleep(Coroutine *co, uint64_t msecs);

    Coroutine* search_wait(int fd);
    Coroutine* del_wait(int fd);
    void add_wait(Coroutine *co, int fd, unsigned short events, uint64_t timeout);

    void run();
    friend void _exec(void *cor);
    friend void schedule_free(Schedule *sched);
    friend Schedule* schedule_create();
    friend int poll_inner(pollfd *fds, nfds_t num_fd, int timeout);
    friend class Coroutine;
    friend class Epoller;


private:
    cpu_ctx ctx;

    uint64_t birth;
    size_t stack_size;
    uint64_t default_timeout;
    int spawned_coroutines;

    Coroutine *cur_co; //正在运行的协程
    int poller_fd; // epfd -- IO操作之前用来检测是否有事件
    int eventfd; //事件通知的 fd，用于和其他进程通信

    int num_new_events; // poll 检测的新的IO数量
    epoll_event evs[MAX_EVENTS];

    int pagesize; //页大小，用于内存优化

    std::queue<Coroutine*> ready;
    std::queue<Coroutine*> exitQueue;
    std::set<Coroutine*, SleepCmp> sleepSet;
    std::set<Coroutine*, WaitCmp> wait;
    
};



static uint64_t getTimeNow() {
    steady_clock::time_point t1 = steady_clock::now();
    microseconds t2 = duration_cast<microseconds> (t1.time_since_epoch());
    return t2.count();
}



template <typename Entry, typename ...Args>
void coroutine_create(Coroutine **new_co, Entry &&f, Args &&... args) {
        new Coroutine(new_co, f, args...);
}


template <typename Entry, typename ...Args>
Coroutine::Coroutine(Coroutine **new_co, Entry &&f, Args &&... args) {
    
    int ret = pthread_once(&sched_key_once, Schedule::schedule_init_key);
    if (ret != 0) {
        perror("pthread_once");
    }
    Schedule *sched = Schedule::get_schedule();
    if (sched == NULL) {
        sched = schedule_create();
        if (sched == NULL) {
            printf("Failed to create scheduler\n");
            return ;
        }
    }

    // 一个栈分配 4k 的整数倍，一般来说就是一页
    
    ret = posix_memalign(&stack_addr, getpagesize(), sched->stack_size);

    sche = sched;
    stack_size = sched->stack_size;
    status = BIT(STATUS_NEW);
    task._func = std::bind(std::forward<Entry>(f), std::forward<Args>(args)...);
    id = sched->spawned_coroutines++;

    fd = -1;
    events = EPOLLIN;

    if (new_co)
        *new_co = this; // 获取当前协程
    
    bzero(&ctx, sizeof(cpu_ctx));
    sche->ready.push(this);
}




#endif // !_COROUTINE_H_