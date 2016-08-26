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

#include <string.h>

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
#include <netinet/tcp.h>
#include <unistd.h>
#include <strings.h>
#include <netdb.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define setsockoptCompat setsockopt
#define sendCompat send
#endif


/***********************************************************************
* setSocketIp6Only:  restricted socket able to send and receive IPv6 packets only
* @param socket   - socket descriptor
* @param v6only  - IP6 socket only : true by default
* \return true is successful, otherwise false
***********************************************************************/

inline bool setSocketIp6Only(int socket, bool v6only = true)
{
  int set = v6only ? 1 : 0;
  return setsockoptCompat( socket, IPPROTO_IPV6, IPV6_V6ONLY, (void *) &set, sizeof(int) ) == 0;
}

/***********************************************************************
* setSocketNoSigpipe:  Disable generation of SIGPIPE for the socket
* @param socket   - socket descriptor
* @param sigpipe  - sigpipe generation: true by default
* \return true is successful, otherwise false
***********************************************************************/

inline bool setSocketNoSigpipe(int socket, bool nosigpipe = true)
{
#if defined(SO_NOSIGPIPE)
  int set = nosigpipe ? 1 : 0;
  return setsockoptCompat( socket, SOL_SOCKET, SO_NOSIGPIPE, (void *) &set, sizeof(int) ) == 0;
#else
  return true;
#endif
}

/***********************************************************************
* setSocketReuseAddr:  Reuse the socket even if the port is busy
* @param socket   - socket descriptor
* @param reuse  - sigpipe generation: true by default
* \return true is successful, otherwise false
***********************************************************************/

inline bool setSocketReuseAddr(int socket, bool reuse = true)
{
  int optval = reuse ? 1 : 0;
  return setsockoptCompat( socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval ) == 0;
}

/***********************************************************************
* setSocketBindToDevice:  Bind socket to a device
* @param socket   - socket descriptor
* @param device  - the device to use
* \return true is successful, otherwise false
***********************************************************************/

inline bool setSocketBindToDevice(int socket, const char *device)
{
#if defined(SO_BINDTODEVICE)
  return setsockopt( socket, SOL_SOCKET, SO_BINDTODEVICE, device, strlen(device) ) == 0;
#else
  return true;
#endif
}

/***********************************************************************
* setSocketKeepAlive:  keepAlive mode for the socket
* @param socket    - socket descriptor
* @param keepAlive - use keepAlive mode ?
* \return true is successful, otherwise false
***********************************************************************/

inline bool setSocketKeepAlive(int socket, bool keepAlive)
{
  int set = keepAlive ? 1 : 0;
  return setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, (void *)&set, sizeof(int) ) == 0;
}

/***********************************************************************
* setSocketDoLinger:  DoLinger Socket Mode
* @param socket   - socket descriptor
* @param dolinger - use dolinger mode ?
* \return true is successful, otherwise false
***********************************************************************/

inline bool setSocketDoLinger(int socket, bool dolinger)
{
  linger value;
  value.l_onoff = dolinger != 0;
  value.l_linger = dolinger;
  return setsockopt( socket, SOL_SOCKET, SO_LINGER, (char*)&value, sizeof (linger) ) == 0;
}
/***********************************************************************
* setSocketSndRcvTimeout:  Place a timeout on the socket
* @param socket   - socket descriptor
* @param seconds  - number of seconds
* @param useconds - number of useconds
* \return true is successful, otherwise false
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
* \return true is successful, otherwise false
***********************************************************************/

inline bool setSocketTcpAckTimeout(int socket, int seconds, int milliseconds)
{
#if defined(SOL_TCP) && defined(TCP_USER_TIMEOUT)
  int timeout = 1000 * seconds + milliseconds;  // user timeout in milliseconds [ms]
  return setsockopt (socket, SOL_TCP, TCP_USER_TIMEOUT, (char*) &timeout, sizeof (timeout)) == 0;
#else
  return false;
#endif
}

/***********************************************************************
* setSocketNagleAlgo:
* @param socket  - socket descriptor
* @param enabled - use Nagle Algorithm
* \return true is successful, otherwise false
***********************************************************************/

inline bool setSocketNagleAlgo(int socket, bool naggle = false)
{
  int flag = naggle ? 1 : 0;
  return setsockopt(socket,IPPROTO_TCP,TCP_NODELAY,(char *)&flag,sizeof(flag)) == 0;
}

#endif

