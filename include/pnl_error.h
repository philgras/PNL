/*
 * pnl_error.h
 *
 *  Created on: 19.06.2016
 *      Author: philgras
 */

#ifndef SRC_PNL_ERROR_H_
#define SRC_PNL_ERROR_H_


/*
 * ERROR
 */

typedef struct {

    /*is set to 0 if no error occurred or to the respective PNL error code otherwise */
    int pnl_ec;

    /*
     * is set to 0 if no error occurred or to the respective system error code otherwise.
     *If system_ec is set to a certain error code then pnl_ec also contains an error code.
     */
    int system_ec;

} pnl_error_t;

#define PNL_ERROR_INIT {.pnl_ec=PNL_NOERR, .system_ec=0}

#define PNL_OK 0
#define PNL_ERR -1

#define PNL_ERROR_MAP(XX)                                                                       \
    XX(NOERR, "No errors occurred"),                                                            \
    XX(ECLOSE, "Failed to close the TCP socket file descriptor"),                               \
    XX(ESOCKET, "Failed to get a socket file descriptor from the OS"),                          \
    XX(EACCEPT, "Failed to accept a new socket"),                                               \
    XX(EHANGUP, "A hang-up occurred on the socket file descriptor"),                            \
    XX(EPEERCLO, "The opposite peer closed the connection"),                                    \
    XX(EMALLOC, "Failed to allocate memory"),                                                   \
    XX(EEVENT, "An error occurred while creating the epoll file descriptor"),                   \
    XX(ETIMEOUT, "A timeout occurred"),                                                         \
    XX(EEVENTDEL, "Unable to delete the file descriptor from the internal epoll event loop"),   \
    XX(EEVENTADD, "Unable to add the file descriptor to the internal epoll event loop"),        \
    XX(EINVAL, "Invalid function arguments"),                                                   \
    XX(ENONBLOCK, "Failed to enable non-blocking mode on a file descriptor"),                   \
    XX(EADDRINFO, "Failed to perform an address look up"),                                      \
    XX(ECONNECT, "Failed to connect a socket file descriptor to the network"),                  \
    XX(EBIND, "Failed to bind a socket file descriptor to the network"),                        \
    XX(EGETSOCKOPT, "Failed to get socket options"),                                            \
    XX(ESETSOCKOPT, "Failed to set socket options"),                                            \
    XX(ERECV, "Failed to receive data from the socket descriptor"),                             \
    XX(ESEND, "Failed to send from the socket descriptor"),                                     \
    XX(EINACTIVE, "The object passed to the function is inactive"),                             \
    XX(EALREADY, "The object passed to the function is already reading/writing"),               \
    XX(ELISTEN, "Failed to listen to a socket file descriptor"),                                \
    XX(EEVENTWAIT, "Failed to wait for epoll events"),                                          \
    XX(ENOTRUNNING, "The event loop is not running"),                                           \
    XX(EWAKEUPFD, "Failed to create the wake up file descriptor via eventfd()"),                \
    XX(ETHREAD, "Failed to create the deamon thread"),                                          \
    XX(ESTARTCB, "Error during user-defined start function"),                                   \
    XX(ENOTCONNECTED, "Failed because a connection has not been established yet"),              \
    XX(ENODAEMON, "Cannot wait for daemon because PNL was started as single thread"),           \
    XX(EINVALIDBUF, "Either the pointer to the buffer is null or the buffer size is 0"),        \
    XX(EWAIT,"Operation could not be finished and would block")

enum pnl_error {

#define XX(ec, desc) PNL_##ec
    PNL_ERROR_MAP(XX)
#undef XX

};

const char *pnl_str_pnl_error(const pnl_error_t* error);

const char* pnl_str_system_error(const pnl_error_t* error);

static inline
int pnl_error_is_error(const pnl_error_t* error){
    return error->pnl_ec != PNL_NOERR;
}

static inline
void pnl_error_set(pnl_error_t *error, int pnl_error, int system_error) {
    error->pnl_ec = pnl_error;
    error->system_ec = system_error;
}

static inline
void pnl_error_reset(pnl_error_t *err) {
    err->pnl_ec = PNL_NOERR;
    err->system_ec = 0;
}

static inline
void pnl_error_set_cleanup(pnl_error_t *error, int pnl_error, int system_error) {
    if (error->pnl_ec == PNL_NOERR) {
        error->pnl_ec = pnl_error;
        error->system_ec = system_error;
    }
}

#endif /* SRC_PNL_ERROR_H_ */
