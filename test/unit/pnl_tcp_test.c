/*
 * pnl_tcp_connect_test.c
 *
 *  Created on: 19.07.2016
 *      Author: philgras
 */


#include "pnl_common.h"
#include "pnl_system.h"
#include "pnl_tcp.h"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <errno.h>

#include <sys/socket.h>

#include <cmocka.h>
#include "../../include/pnl_error.h"
#include <pnl_common.h>

#define VALID_FD 100


static const char *text = "abcdefghijklmnopqrstuvwxyz";


/**************************************
 *********HAPPY SYS CALL MOCKING********
 **************************************/

int pnl_getaddrinfo(const char *__restrict name, const char *__restrict service,
                    const struct addrinfo *__restrict req, struct addrinfo **__restrict pai) {

    return mock();
}


void pnl_freeaddrinfo(struct addrinfo *ai) {

}

int pnl_socket(int domain, int type, int protocol) {
    return mock();
}

int pnl_connect(int fd, const struct sockaddr *addr, socklen_t len) {
    return mock();
}

int pnl_bind(int fd, const struct sockaddr *addr, socklen_t len) {
    return mock();
}

int pnl_setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen) {
    return mock();
}

int pnl_getsockopt(int fd, int level, int optname, void *__restrict optval, socklen_t *__restrict optlen) {

    int mock_value = mock();

    assert_true(fd != PNL_INVALID_FD);
    assert_true(level = SOL_SOCKET);
    assert_true(optname = SOL_SOCKET);

    if(mock_value == 0){
        *((int*)optval) = 0;
    }else if(mock_value < 0){
        *((int*)optval) = -mock_value;
        mock_value = 0;
    }

    return mock_value;
}

int pnl_listen(int fd, int n) {
    return mock();
}

int pnl_accept4(int fd, struct sockaddr *addr, socklen_t *__restrict addr_len, int flags) {
    return mock();
}

ssize_t pnl_recv(int fd, void *buf, size_t n, int flags) {
    int mock_value = mock();
    size_t len = strlen(text);
    size_t offset = len - n;

    if (mock_value < 0) {

        errno = -mock_value;
        mock_value = -1;

    } else if (mock_value > 0) {
        assert_true(text + offset + mock_value <= text + len);
        memcpy(buf, text + offset, mock_value);
    }


    return mock_value;
}

ssize_t pnl_send(int fd, const void *buf, size_t n, int flags) {
    int mock_value = mock();

    if (mock_value < 0) {

        errno = -mock_value;
        mock_value = -1;
    }


    return mock_value;
}

int pnl_close(int fd) {
    int mock_value = mock();

    if (mock_value < 0) {

        errno = -mock_value;
        mock_value = -1;
    }
    return mock_value;
}


/*
 *****************************
 ************UNIT TESTS*******
 *****************************
 */

void connect_test(void **state) {
    will_return(pnl_socket, -1);
    assert_true(pnl_socket(1, 2, 3) == -1);
}


void connect_succeeded_test(void **state) {
    pnl_error_t error = {.pnl_ec = PNL_NOERR, .system_ec = 0};
    pnl_fd_t fd = VALID_FD;
    int rc;

    will_return(pnl_getsockopt,-EINPROGRESS);
    rc = pnl_tcp_connect_succeeded(&fd,&error);
    assert_true(rc == PNL_ERR);
    assert_true(error.system_ec == EINPROGRESS );
    assert_true(error.pnl_ec == PNL_EWAIT);

    will_return(pnl_getsockopt,0);
    rc = pnl_tcp_connect_succeeded(&fd,&error);
    assert_true(rc == PNL_OK);

}

void listen_test(void **state) {
    will_return(pnl_socket, -1);
    assert_true(pnl_socket(1, 2, 3) == -1);
}

void accept_test(void **state) {
    will_return(pnl_socket, -1);
    assert_true(pnl_socket(1, 2, 3) == -1);
}

void write_test(void **state) {

    int rc;
    char text[] = "abcdefghijklmnopqrstuvwxyz";
    size_t size = strlen(text);
    pnl_fd_t fd = VALID_FD;
    pnl_error_t error = {.pnl_ec = PNL_NOERR, .system_ec = 0};
    pnl_buffer_t buffer;

    pnl_buffer_init(&buffer);
    pnl_buffer_set_output(&buffer, text, size);


    will_return(pnl_send, 5);
    will_return(pnl_send, -EINTR);
    will_return(pnl_send, 5);
    will_return(pnl_send, -EAGAIN);
    rc = pnl_tcp_write(&fd, &buffer, &error);

    assert_true(rc == PNL_ERR);
    assert_true(error.system_ec == EAGAIN);
    assert_true(error.pnl_ec == PNL_EWAIT);
    assert_true(buffer.position == text + 10);
    pnl_error_reset(&error);

    will_return(pnl_send, 5);
    will_return(pnl_send, -EISCONN);
    rc = pnl_tcp_write(&fd, &buffer, &error);

    assert_true(rc == PNL_ERR);
    assert_true(error.system_ec == EISCONN);
    assert_true(error.pnl_ec == PNL_ESEND);
    assert_true(buffer.position == text + 15);
    pnl_error_reset(&error);

    will_return(pnl_send, 11);
    rc = pnl_tcp_write(&fd, &buffer, &error);
    assert_true(rc == PNL_OK);
    assert_true(buffer.position == text + size);

}


void read_test(void **state) {
    int rc;

    size_t size = strlen(text);
    char data[size];

    pnl_fd_t fd = VALID_FD;
    pnl_error_t error = {.pnl_ec = PNL_NOERR, .system_ec = 0};
    pnl_buffer_t buffer;

    pnl_buffer_init(&buffer);
    pnl_buffer_set_input(&buffer, data, size);

    will_return(pnl_recv, 10);
    rc = pnl_tcp_read(&fd, &buffer, &error);
    assert_true(rc == PNL_OK);
    assert_memory_equal(data, text, 10);
    pnl_error_reset(&error);

    pnl_buffer_set_input(&buffer, data + 10, size - 10);
    will_return(pnl_recv, -EAGAIN);
    rc = pnl_tcp_read(&fd, &buffer, &error);
    assert_true(rc == PNL_ERR);
    assert_true(error.system_ec == EAGAIN);
    assert_true(error.pnl_ec == PNL_EWAIT);
    pnl_error_reset(&error);

    will_return(pnl_recv, -ECONNRESET);
    rc = pnl_tcp_read(&fd, &buffer, &error);
    assert_true(rc == PNL_ERR);
    assert_true(error.system_ec == ECONNRESET);
    assert_true(error.pnl_ec == PNL_ERECV);
    pnl_error_reset(&error);

    will_return(pnl_recv, 0);
    rc = pnl_tcp_read(&fd, &buffer, &error);
    assert_true(rc == PNL_ERR);
    assert_true(error.system_ec == 0);
    assert_true(error.pnl_ec == PNL_EPEERCLO);
    pnl_error_reset(&error);

    will_return(pnl_recv, 16);
    rc = pnl_tcp_read(&fd, &buffer, &error);
    assert_true(rc == PNL_OK);
    assert_memory_equal(data, text, 26);

}


void close_test(void **state) {

    pnl_error_t error = {.pnl_ec = PNL_NOERR, .system_ec = 0};
    pnl_fd_t fd = VALID_FD;
    int rc;

    will_return(pnl_close, 0);
    rc = pnl_tcp_close(&fd, &error);
    assert_true(rc == PNL_OK);
    assert_true(fd == PNL_INVALID_FD);
    assert_true(error.system_ec == 0);
    assert_true(error.pnl_ec == PNL_NOERR);

    fd = VALID_FD;
    will_return(pnl_close, -EINVAL);
    rc = pnl_tcp_close(&fd, &error);
    assert_true(rc == PNL_ERR);
    assert_true(fd == PNL_INVALID_FD);
    assert_true(error.system_ec == EINVAL);
    assert_true(error.pnl_ec == PNL_ECLOSE);

}

int main(int argc, char *argv[]) {

    const struct CMUnitTest tests[] = {

            cmocka_unit_test(connect_test),
            cmocka_unit_test(listen_test),
            cmocka_unit_test(write_test),
            cmocka_unit_test(read_test),
            cmocka_unit_test(accept_test),
            cmocka_unit_test(close_test),
            cmocka_unit_test(connect_succeeded_test)

    };

    return cmocka_run_group_tests(tests, NULL, NULL);

}







