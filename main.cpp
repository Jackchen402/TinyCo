#include "coroutine/coroutine.h"
#include "epoller/epoller.h"
#include "hook/socket_f.h"

#include <arpa/inet.h>
#include <stdlib.h>

#define MAX_CLIENT_NUM			100


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

	steady_clock::time_point t_begin = steady_clock::now();
	
	while (1) {
		socklen_t len = sizeof(struct sockaddr_in);
		
		int client_fd = accept(fd, (struct sockaddr*)&remote, &len);
		
		if (client_fd % 1000 == 999) {
			
			steady_clock::time_point t_now = steady_clock::now();
			milliseconds time_used = duration_cast<milliseconds>(t_now.time_since_epoch() - t_begin.time_since_epoch());
			
			printf("client fd : %d, time_used: %ld\n", client_fd, time_used.count());
		}

		coroutine_create(NULL, read_cb, &client_fd);
	}
	
}


int main(int argc, char *argv[]) {
	Coroutine *co = NULL;

	int i = 0;
	unsigned short base_port = 8888;
	for (i = 0;i < MAX_CLIENT_NUM;i ++) {
		unsigned short *port = (unsigned short *)calloc(1, sizeof(unsigned short));
		*port = base_port + i;
		coroutine_create(NULL, server, port); // not run
	}

	Schedule *sched = Schedule::get_schedule();
	sched->run();  // run
	schedule_free(sched);

	return 0;
}
