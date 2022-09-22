#include "../coroutine/coroutine.h"
#include "../epoller/epoller.h"

void Schedule::schedule_init_key() {
    pthread_key_create(&global_sched_key, Schedule::sched_key_destructor);
    pthread_setspecific(global_sched_key, NULL);
}

Schedule::~Schedule() {
    schedule_free(this);
}

Schedule* Schedule::get_schedule(void) { // 取出线程特有的值
    return (Schedule *)pthread_getspecific(global_sched_key);
}

void Schedule::sched_key_destructor(void *data) {
    free(data);
}


Schedule::Schedule(int size) {
    eventfd = 0;
    int sched_stack_size = size > 0 ? size : MAX_STACKSIZE;
    pthread_setspecific(global_sched_key, this);
    poller_fd = Epoller::epoll_init();
	if (poller_fd == -1) {
		printf("Failed to initialize epoller\n");
		return ;
	}

    Epoller::epoll_ev_register();

    stack_size = sched_stack_size;
    pagesize = getpagesize();

    spawned_coroutines = 0;
	default_timeout = 3000000u;

    birth = getTimeNow();

    bzero(&ctx, sizeof(cpu_ctx));
}



void Schedule::del_sleep(Coroutine *co) {
    if (co->status & BIT(STATUS_SLEEPING)) {
        co->sche->sleepSet.erase(co);
        co->status &= CLEARBIT(STATUS_SLEEPING);
        co->status |= BIT(STATUS_READY);
    }
}



void Schedule::add_sleep(Coroutine *co, uint64_t msecs) {
    uint64_t usecs = msecs * 1000u;
    co->sleep_usecs = getTimeNow() - co->sche->birth + usecs;

    auto co_tmp = co->sche->sleepSet.find(co);
    if (co_tmp != co->sche->sleepSet.end()) { // 如果找到了，需要删除后重新设置插入新的协程
        co->sche->sleepSet.erase(co_tmp);
        co->sche->sleepSet.insert(co);
    } else { //如果没找到，设置状态，然后插入协程
        co->status |= BIT(STATUS_SLEEPING);
        co->sche->sleepSet.insert(co);
    }
}

Coroutine* Schedule::search_wait(int fd) {
    Coroutine target;
    target.fd = fd;
    Schedule *sched = get_schedule();
    auto it = sched->wait.find(&target);
    if (it == wait.end()) {
        return NULL;
    }
    return *it;
}


Coroutine* Schedule::del_wait(int fd) {
    Coroutine target;
    target.fd = fd;

    Schedule *sched = get_schedule();
    std::set<Coroutine*, WaitCmp>::iterator it = sched->wait.find(&target);
    del_sleep(*it);
    if (it != wait.end())
        sched->wait.erase(it);
    
    (*it)->status = BIT(STATUS_READY);
    return *it;
}

void Schedule::add_wait(Coroutine *co, int fd, unsigned short events, uint64_t timeout) {
    if (events & POLLIN) {
        co->status |= STATUS_WAIT_READ;
    } else if (events & POLLOUT) {
        co->status |= STATUS_WAIT_WRITE;
    } else {
        printf("events : %d\n", events);
		assert(0);
    }
    co->fd = fd;
    co->events = events;

    add_sleep(co, timeout);
    co->sche->wait.insert(co);
}

void Schedule::run() {
    while (!sleepSet.empty() || !ready.empty() || !wait.empty() || !exitQueue.empty()) {
        // sleep rbtree-- expired  
        while (!sleepSet.empty()) {
            Coroutine *expired_co = get_expired_coroutine(this);
            if (expired_co == NULL) {
                break;
            }
            expired_co->resume();// 让sleep超时的线程先执行
        }
        // ready queue
        while (!ready.empty()) {
            Coroutine *co = ready.front();
            ready.pop();

            co->resume();
        }   
        // wait rbtree
        schedule_epoll(this);
        while (num_new_events > 0) {
            int idx = --num_new_events;
            epoll_event *ev = evs + idx; //第i个事件

            int fd = ev->data.fd;
            int is_eof = ev->events & EPOLLHUP; 
            if (is_eof) errno = ECONNRESET;  // 如果对端关闭，设置errno


            Coroutine* co = search_wait(fd);
            if (co != NULL) {
                if (is_eof) { // 如果对端关闭，设置协程状态
                    co->status |= BIT(STATUS_FDEOF);
                }
                co->resume();
            }
        }
        // exit queue
        while (!exitQueue.empty()) {
            Coroutine *co = exitQueue.front();
            exitQueue.pop();
            coroutine_free(co);
        }           
    }
    
}

Schedule* schedule_create() {
    Schedule *sched = new Schedule(0);
    return sched;
}

void schedule_free(Schedule *sched) {
    if (sched->poller_fd > 0) {
        close(sched->poller_fd);
    }
    if (sched->eventfd > 0) {
        close(sched->eventfd);
    }

    free(sched);
    assert(pthread_setspecific(global_sched_key, NULL) == 0);
}

Coroutine* Schedule::get_expired_coroutine(Schedule *sched) {
    uint64_t diff_usecs = getTimeNow() - sched->birth;
    auto it = sched->sleepSet.begin(); // 获取最小的
    //printf("get_expired_coroutine avaiable: [co->sleep_usecs: (%ld)]\n", it->sleep_usecs);
    if (it == sched->sleepSet.end()) {
        return NULL;
    }
    
    if ((*it)->sleep_usecs <= diff_usecs) { //如果有超时的协程
        sched->sleepSet.erase(it);
        return *it;
    }
    //没有超时的协程
    return NULL;
}

uint64_t Schedule::get_min_timeout(Schedule *sched) {
    uint64_t diff_usecs = getTimeNow() - sched->birth;
    uint64_t min = sched->default_timeout;

    auto it = sched->sleepSet.begin();
    if (it == sched->sleepSet.end()) {  //如果没有找到，就是默认超时时间
        return min;
    }

    min = (*it)->sleep_usecs;
	if (min > diff_usecs) {
		return min - diff_usecs; // 如果协程的睡眠时间还没有超时，返回差值
	}
    return 0; //否则就判定为超时，返回0

}

// 在 ready queue 处理完后进行 schedule_epoll
int Schedule::schedule_epoll(Schedule *sched) {
    sched->num_new_events = 0;
    uint64_t usecs = get_min_timeout(sched);
    if (usecs <= 0 || !sched->ready.empty()) { // 如果超时时间大于0，并且就绪队列为空,就继续执行
        return 0;
    }

    int nready = 0;
    // 使用循环主要是因为可能会被信号打断而没有 epoll_wait成功
    while (1) {
		nready = Epoller::epoll_wait_for(usecs);
		if (nready == -1) {
			if (errno == EINTR) continue; 
			else assert(0);
		}
		break;
	}
    //sched->nevents = 0;
	sched->num_new_events = nready;
}

