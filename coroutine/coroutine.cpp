#include "coroutine.h"

pthread_key_t global_sched_key; 

pthread_once_t sched_key_once = PTHREAD_ONCE_INIT; // 在线程中只执行一次

#define SCHEDULE_PAGE_SIZE (1024*4)


extern "C"
{   

int _switch(cpu_ctx *new_ctx, cpu_ctx *cur_ctx);
// 内嵌入 _switch的定义


#ifdef __x86_64__  
__asm__ (
"    .text                                  \n"
"       .p2align 4,,15                                   \n"
".globl _switch                                          \n"
"_switch:                                                \n"
"       movq %rsp, 0(%rsi)      # save stack_pointer     \n"
"       movq %rbp, 8(%rsi)      # save frame_pointer     \n"
"       movq (%rsp), %rax       # save insn_pointer      \n"
"       movq %rax, 16(%rsi)                              \n"
"       movq %rbx, 24(%rsi)     # save rbx,r12-r15       \n"
"       movq %r12, 32(%rsi)                              \n"
"       movq %r13, 40(%rsi)                              \n"
"       movq %r14, 48(%rsi)                              \n"
"       movq %r15, 56(%rsi)                              \n"
"       movq 56(%rdi), %r15                              \n"
"       movq 48(%rdi), %r14                              \n"
"       movq 40(%rdi), %r13     # restore rbx,r12-r15    \n"
"       movq 32(%rdi), %r12                              \n"
"       movq 24(%rdi), %rbx                              \n"
"       movq 8(%rdi), %rbp      # restore frame_pointer  \n"
"       movq 0(%rdi), %rsp      # restore stack_pointer  \n"
"       movq 16(%rdi), %rax     # restore insn_pointer   \n"
"       movq %rax, (%rsp)                                \n"
"       ret                                              \n"
);
}
#endif


void _exec(void *cor) {
    Coroutine *co = (Coroutine *)cor;
    co->task._func();
    co->status = co->status | (BIT(STATUS_EXIT));
    co->sche->exitQueue.push(co);
    co->yield();
}

void coroutine_init(Coroutine *co) {
    // 一级指针强转为二级指针，也就是将一级指针所指内存中的值强转为 void*
    void **stack = (void **)((char *)co->stack_addr + co->stack_size);

    // 从 stack[-1] 到 stack[-4] 是用来保存栈指针、某些数据、协程信息等的预留空间
    stack[-3] = NULL; 
	stack[-2] = (void *)co;  // 协程信息保存  stack[-2]

    co->ctx.rsp = (char*)stack - (4 * sizeof(char*));  // 栈顶指针保存 stack[-4]
	co->ctx.rbp = (char*)stack - (3 * sizeof(char*));  // 栈基指针保存 stack[-3]
	co->ctx.rip = (void*)_exec;  //下一条指令是协程的入口函数
    co->status &= CLEARBIT(STATUS_NEW);
    co->status |= BIT(STATUS_READY);
}



/*
Coroutine::Coroutine(Coroutine **new_co, coroutine_entry funcp, void *argp) {
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
    func = funcp;
    arg = argp;
    id = sched->spawned_coroutines++;

    fd = -1;
    events = EPOLLIN;

    if (new_co)
        *new_co = this; // 获取当前协程
    
    bzero(&ctx, sizeof(cpu_ctx));
    sche->ready.push(this);
}
*/
/* void coroutine_create(Coroutine **new_co, coroutine_entry func, void *arg) {
    new Coroutine(new_co, func, arg);
}
 */

int Coroutine::yield() {
    precedence = 0;
    _switch(&sche->ctx, &ctx);
}



int Coroutine::resume() {
    if (status & BIT(STATUS_NEW)) {
        coroutine_init(this);

    }
    Schedule *sched = Schedule::get_schedule();
    sched->cur_co = this;
    _switch(&ctx, &sched->ctx);
    // 由于切换了到了协程，因此调度器的当前协程为NULL

    sched->cur_co = NULL;

}

void Coroutine::sleep_for(uint64_t msec) {
    if (msec == 0) {
        sche->ready.push(this);
    } else {
        Schedule::get_schedule()->add_sleep(this, msec);
    }
}

void Coroutine::detach() {
    status |= STATUS_DETACH;
}

Coroutine::~Coroutine() {
    if (stack_addr) {
        free(stack_addr);
        stack_addr = NULL;
    }
}

void coroutine_free(Coroutine *co){
    delete co;
}





