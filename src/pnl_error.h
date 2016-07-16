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

}pnl_error_t;

#define PNL_OK 0
#define PNL_ERR -1

#define PNL_ERROR_MAP(XX)																				    						\
	XX(NOERR, "No errors occurred")										,															\
	XX(ECLOSE, "Failed to close the TCP socket file descriptor"),															\
	XX(ESOCKET, "Failed to get a socket file descriptor from the OS"),													\
	XX(EACCEPT, "Failed to accept a new socket"),																				\
	XX(EHANGUP, "A hang-up occured on the socket file descriptor"),													\
	XX(EPEERCLO, "The opposite peer closed the connection"),																\
	XX(EMALLOC, "Failed to allocate memory"),																					\
	XX(EEVENT, "An error occurred on the file descriptor"),							        								\
	XX(ETIMEOUT, "A timeout occurred"),																							\
	XX(EEVENTDEL, "Unable to delete the file descriptor from the internal epoll event loop"),				\
	XX(EEVENTADD, "Unable to addthe file descriptor to the internal epoll event loop"),						\
	XX(EINVAL, "Invalid function arguments"),																						\
	XX(ENONBLOCK, "Failed to enable nonblocking mode on a file descriptor"),									\
	XX(EADDRINFO, "Failed to perform an address look up"),																\
	XX(ECONNECT, "Failed to connect a socket file descriptor to the network"),									\
	XX(EBIND, "Failed to bind a socket file descriptor to the network"),													\
	XX(EGETSOCKOPT, "Failed to get socket options"),																		\
	XX(ESETSOCKOPT, "Failed to set socket options"),																		\
	XX(ERECV, "Failed to receive data from the socket descriptor"),														\
	XX(ESEND, "Failed to send from a socket descriptor"),																	\
	XX(EINACTIVE, "The connection passed to the function is inactive"),												\
	XX(EALREADY, "The connection passed to the function is already in the requested state"),				\
	XX(ELISTEN, "Failed to listen to a socket descriptor"),																	\
	XX(EWAIT,"Operation could not be finished and would block")

enum pnl_error{

#define XX(ec, desc) PNL_##ec
	PNL_ERROR_MAP(XX)
#undef XX

};

const char* pnl_strerror(int ec);
const char* pnl_strrerrorcode(int ec);

#endif /* SRC_PNL_ERROR_H_ */
