
#include "epoller.h"
#include <sys/eventfd.h>

int Epoller::epoll_init() {
    // 1024 just a hint
    return epoll_create(1024);
}

int Epoller::epoll_ev_register() {
    Schedule *sched = Schedule::get_schedule();

    if (sched->eventfd == 0) {
        sched->eventfd = eventfd(0, EFD_NONBLOCK); //创建一个用于事件通知的 fd
        assert(sched->eventfd != -1);
    }

    epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = sched->eventfd;
	int ret = epoll_ctl(sched->poller_fd, EPOLL_CTL_ADD, sched->eventfd, &ev);

    
    return ret;
}

int Epoller::epoll_wait_for(uint64_t interval) {
    Schedule *sched = Schedule::get_schedule();
    return epoll_wait(sched->poller_fd, sched->evs, MAX_EVENTS, interval);
}