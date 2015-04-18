#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/select.h>

#define send send
#define bind bind
#define recv recv
#define select select

 ssize_t mySend(int sockfd, const void *buf, size_t len, int flags)
 {

   return(send(sockfd, buf, len, flags));
 }


ssize_t myRecv(int sockfd, void *buf, size_t len, int flags)
{
  return(recv(sockfd, buf, len, flags));

}

 int myBind(int sockfd, const struct sockaddr *addr,  socklen_t addrlen)
 {
   return(bind(sockfd, addr, addrlen));

 }


 int mySelect(int nfds, fd_set *readfds, fd_set *writefds,
                  fd_set *exceptfds, struct timeval *timeout)
 {
   return(select(nfds, readfds, writefds, exceptfds, timeout));

 }

