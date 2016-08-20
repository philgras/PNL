/*
 * pnl_tcp_connect_test.c
 *
 *  Created on: 19.07.2016
 *      Author: philgras
 */

#include <cmocka.h>

#include "pnl_common.h"
#include "pnl_tcp.h"
#include "pnl_sys_nio.h"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>


/**************************************
 *********HAPPY SYS CALL MOCKING********
 **************************************/

int pnl_getaddrinfo (const char *__restrict name,	  const char *__restrict service,
								const struct addrinfo *__restrict req,	  struct addrinfo **__restrict pai){




}


extern void pnl_freeaddrinfo (struct addrinfo *ai);

extern int pnl_socket(int domain, int type, int protocol);

extern int pnl_connect (int fd, const struct sockaddr* addr, socklen_t len);

extern int pnl_bind (int fd, const struct sockaddr*  addr, socklen_t len);

extern int pnl_close(int fd);

extern int pnl_setsockopt (int fd, int level, int optname,
		       	   	   	   	   	   	   	     const void *optval, socklen_t optlen);

extern int pnl_getsockopt (int fd, int level, int optname,
										     void *__restrict optval,
										     socklen_t *__restrict optlen);

extern int pnl_listen (int fd, int n);

extern int pnl_accept4 (int fd, struct sockaddr* addr,
		    							 socklen_t *__restrict addr_len, int flags);

extern ssize_t pnl_recv (int fd, void *buf, size_t n, int flags);

extern ssize_t pnl_send (int fd, const void *buf, size_t n, int flags);

void demo_test(void** state){

}


int main(int argc, char* argv[]){

	const struct CMUnitTest tests [] = {

			cmocka_unit_test(demo_test)

	};

	return cmocka_run_group_tests(tests,NULL,NULL);

}







