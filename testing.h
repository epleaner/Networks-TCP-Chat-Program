
#ifndef TESTING_H
#define TESTING_H

ssize_t mySend(int sockfd, const void *buf, size_t len, int flags);
ssize_t myRecv(int sockfd, void *buf, size_t len, int flags);
int myBind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int mySelect(int nfds, fd_set *readfds, fd_set *writefds,
                  fd_set *exceptfds, struct timeval *timeout);

#define send mySend
#define recv myRecv
#define bind myBind
#define select mySelect




#endif
