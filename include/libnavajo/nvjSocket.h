//********************************************************
/**
 * @file  nvjSocket.h
 *
 * @brief posix socket's facilities
 *
 * @author T.Descombes (thierry.descombes@gmail.com)
 *
 * @version 1
 * @date 19/02/16
 */
//********************************************************

#ifndef NVJSOCKET_H_
#define NVJSOCKET_H_

#ifdef WIN32

#define usleep(n) Sleep(n/1000>=1 ? n/1000 : 1)

#define snprintf _snprintf_s
#define vsnprintf _vsnprintf
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define close closesocket
#define SHUT_RDWR SD_BOTH
#define ushort USHORT
#define poll(x,y,z) WSAPoll((x),(y),(z))
#define setsockoptCompat(a,b,c,d,e)        setsockopt(a,b,c,(const char *)d,(int)e)
#define sendCompat(f,b,l,o)         send((f),(const char *)(b),(int)(l),(o))


#else

#include <sys/socket.h>
#include <unistd.h>
#include <strings.h>
#include <netdb.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define setsockoptCompat setsockopt
#define sendCompat send
#ifdef LINUX
  #define SO_NOSIGPIPE    0x0800
#endif
#endif

/***********************************************************************
* setSocketNoSigpipe:  Place a timeout on the socket
* @param socket   - socket descriptor
* diabled generation of SIGPIPE for the socket
* \return result of setsockopt (successfull: 0, otherwise -1)
***********************************************************************/

inline bool setSocketNoSigpipe(int socket)
{
  int set = 1;
  return setsockoptCompat( socket, SOL_SOCKET, SO_NOSIGPIPE, (void *) &set, sizeof(int) ) == 0;
}

/***********************************************************************
* setSocketKeepAlive:  Place a timeout on the socket
* @param socket   - socket descriptor
* diabled generation of SIGPIPE for the socket
* \return result of setsockopt (successfull: 0, otherwise -1)
***********************************************************************/

inline int setSocketKeepAlive(int socket, bool keepAlive)
{
  int set = keepAlive?1:0;
  return setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, (void *)&set, sizeof(int) );
}

/***********************************************************************
* setSocketDoLinger:  Place a timeout on the socket
* @param socket   - socket descriptor
* @param seconds  - number of seconds
* @param useconds - number of useconds
* \return result of setsockopt (successfull: 0, otherwise -1)
***********************************************************************/

inline int setSocketDoLinger(int socket, bool dolinger)
{
  linger value;
  value.l_onoff = dolinger != 0;
  value.l_linger = dolinger;
  return setsockopt( socket, SOL_SOCKET, SO_LINGER, (char*)&value, sizeof (linger) );
}
/***********************************************************************
* setSocketSndRcvTimeout:  Place a timeout on the socket
* @param socket   - socket descriptor
* @param seconds  - number of seconds
* @param useconds - number of useconds
* \return result of setsockopt (successfull: 0, otherwise -1)
***********************************************************************/

inline bool setSocketSndRcvTimeout(int socket, time_t seconds, long int useconds=0)
{
#ifdef WIN32
  int nTimeout = seconds * 1000 + useconds;
  if (  (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,(const char*)&nTimeout, sizeof(int)) == 0 )
     && (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO,(const char*)&nTimeout, sizeof(int)) == 0 )
  return true;
#else

  struct timeval tv,tv2;
  tv.tv_sec = seconds ;
  tv.tv_usec = useconds ;
  tv2.tv_sec = seconds ;
  tv2.tv_usec = useconds ;

  if (  (setsockoptCompat (socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval)) == 0)
     && (setsockoptCompat (socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv2, sizeof(struct timeval)) == 0))
    return true;
#endif

  return false;
}

/***********************************************************************
* setSocketTcpAckTimeout:  Place a timeout on the tcp acknowledgement
* @param socket  - socket descriptor
* @param seconds - number of seconds
* @param milliseconds - number of milliseconds
* \return result of setsockopt (successfull: 0, otherwise -1)
***********************************************************************/

inline int setSocketTcpAckTimeout(int socket, int seconds, int milliseconds)
{
#if defined(SOL_TCP) && defined(TCP_USER_TIMEOUT)
  int timeout = 1000 * seconds + milliseconds;  // user timeout in milliseconds [ms]
    return setsockopt (fd, SOL_TCP, TCP_USER_TIMEOUT, (char*) &timeout, sizeof (timeout));
#else
  return false;
#endif
}

#endif