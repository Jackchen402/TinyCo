#include "coroutine/coroutine.h"
#include "epoller/epoller.h"
#include "hook/socket_f.h"

#include <sys/time.h>
#include <arpa/inet.h>
#include <stdlib.h>

#define MAX_CLIENT_NUM			100
#define TIME_SUB_MS(tv1, tv2)  ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)


void read_cb(void *arg) {
	int fd = *(int *)arg;
	int ret = 0;

 
	struct pollfd fds;
	fds.fd = fd;
	fds.events = POLLIN;

	while (1) {
		
		char buf[1024] = {0};
		ret = recv(fd, buf, 1024, 0);
		if (ret > 0) {
			if(fd > MAX_CLIENT_NUM) 
			printf("read from server: %.*s\n", ret, buf);

			ret = send(fd, buf, strlen(buf), 0);
			if (ret == -1) {
				close(fd);
				break;
			}
		} else if (ret == 0) {	
			close(fd);
			break;
		}

	}
}


void server(void *arg) {

	unsigned short port = *(unsigned short *)arg;
	free(arg);

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) return ;

	struct sockaddr_in local, remote;
	local.sin_family = AF_INET;
	local.sin_port = htons(port);
	local.sin_addr.s_addr = INADDR_ANY;
	bind(fd, (struct sockaddr*)&local, sizeof(struct sockaddr_in));

	listen(fd, 20);
	printf("listen port : %d\n", port);

	
	struct timeval tv_begin;
	gettimeofday(&tv_begin, NULL);

	while (1) {
		socklen_t len = sizeof(struct sockaddr_in);
		
		int cli_fd = accept(fd, (struct sockaddr*)&remote, &len);
		
		if (cli_fd % 1000 == 999) {

			struct timeval tv_cur;
			memcpy(&tv_cur, &tv_begin, sizeof(struct timeval));
			
			gettimeofday(&tv_begin, NULL);
			int time_used = TIME_SUB_MS(tv_begin, tv_cur);
			
			printf("client fd : %d, time_used: %d\n", cli_fd, time_used);
		}

		coroutine_create(NULL, read_cb, &cli_fd);
	}
	
}


int main(int argc, char *argv[]) {
	Coroutine *co = NULL;

	int i = 0;
	unsigned short base_port = 8888;
	for (i = 0;i < 1;i ++) {
		unsigned short *port = (unsigned short *)calloc(1, sizeof(unsigned short));
		*port = base_port + i;
		coroutine_create(NULL, server, port); // not run
	}

    Schedule *sched = Schedule::get_schedule();
    sched->run();  // run
	schedule_free(sched);

	return 0;
}