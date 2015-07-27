//********************************************************
/**
 * @file  WebServer.cc 
 *
 * @brief HTTP multithreaded Server 
 *        rfc2616 compliant (HTTP1.1)
 *        rfc5280 X509 authentification
 *
 * @author T.Descombes (thierry.descombes@gmail.com)
 *
 * @version 1        
 * @date 19/02/15
 */
//********************************************************


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

#endif



#include <sys/stat.h>

#include <pthread.h>
#include <ctype.h>

#include <string.h>
#include <sys/types.h>
#include <errno.h> 
#include <stdlib.h>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <fcntl.h>

#include "libnavajo/WebServer.hh"
#if defined(LINUX) || defined(__darwin__)
#include "libnavajo/AuthPAM.hh"
#endif
#include "libnavajo/thread.h"
#include "libnavajo/htonll.h"
#include "libnavajo/WebSocket.hh"

#define DEFAULT_HTTP_PORT 8080
#define LOGHIST_EXPIRATION_DELAY 600
#define BUFSIZE 32768

const char WebServer::authStr[]="Authorization: Basic ";
const int WebServer::verify_depth=512;
char *WebServer::certpass=NULL;
string WebServer::webServerName;
pthread_mutex_t IpAddress::resolvIP_mutex = PTHREAD_MUTEX_INITIALIZER;
HttpSession::HttpSessionsContainerMap HttpSession::sessions;
pthread_mutex_t HttpSession::sessions_mutex=PTHREAD_MUTEX_INITIALIZER;
const string WebServer::base64_chars = 
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";
const string WebServer::webSocketMagicString="258EAFA5-E914-47DA-95CA-C5AB0DC85B11";


time_t HttpSession::lastExpirationSearchTime=0;
time_t HttpSession::sessionLifeTime=20*60;

/*********************************************************************/

WebServer::WebServer()
{
  sslCtx=NULL;
  s_server_session_id_context = 1;

  webServerName=std::string("Server: libNavajo/")+std::string(LIBNAVAJO_SOFTWARE_VERSION);
  exiting=false;
  exitedThread=0;
  httpdAuth=false;
  nbServerSock=0;
  
  disableIpV4=false;
  disableIpV6=false;
  tcpPort=DEFAULT_HTTP_PORT;
  threadsPoolSize=128;
  
  sslEnabled=false;
  authPeerSsl=false;
  authPam=false;

  pthread_mutex_init(&clientsQueue_mutex, NULL);
  pthread_cond_init(&clientsQueue_cond, NULL);

  pthread_mutex_init(&webSocketClientList_mutex, NULL);

  pthread_mutex_init(&peerDnHistory_mutex, NULL);
  pthread_mutex_init(&usersAuthHistory_mutex, NULL);
}

/*********************************************************************/

void WebServer::updatePeerIpHistory(IpAddress& ip)
{
  time_t t = time ( NULL );
  std::map<IpAddress, time_t>::iterator i = peerIpHistory.find (ip);

  bool dispPeer = false;
  if (i != peerIpHistory.end())
  {
        dispPeer = t - i->second > LOGHIST_EXPIRATION_DELAY;
        i->second = t;
  }
  else
  {
        peerIpHistory[ip]=t;
         dispPeer = true;
  }

  if (dispPeer)
     NVJ_LOG->append(NVJ_INFO,std::string ("WebServer: Connection from IP: ") + ip.str());
}

/*********************************************************************/

void WebServer::updatePeerDnHistory(std::string dn)
{

  pthread_mutex_lock( &peerDnHistory_mutex );
  time_t t = time ( NULL );
  std::map<std::string,time_t>::iterator i = peerDnHistory.find (dn);

  bool dispPeer = false;
  if (i != peerDnHistory.end())
  {
    dispPeer = t - i->second > LOGHIST_EXPIRATION_DELAY;
    i->second = t;
  }
  else
  {
    peerDnHistory[dn]=t;
    dispPeer = true;
  }

  if (dispPeer)
    NVJ_LOG->append(NVJ_INFO,"WebServer: Authorized DN: "+dn);

  pthread_mutex_unlock( &peerDnHistory_mutex );
}

/*********************************************************************/
/**
* Http login authentification
* @param name: the login/password string in base64 format  
* @param name: set to the decoded login name  
* @return true if user is allowed
*/ 
bool WebServer::isUserAllowed(const string &pwdb64, string& login)
{

  pthread_mutex_lock( &usersAuthHistory_mutex );
  time_t t = time ( NULL );

  bool isNewUser = true;
  std::map<std::string,time_t>::iterator i = usersAuthHistory.find (pwdb64);

  if (i != usersAuthHistory.end())
  {
    isNewUser = t - i->second > LOGHIST_EXPIRATION_DELAY;
    i->second = t;
  }

  if (!isNewUser)
  {
    pthread_mutex_unlock( &usersAuthHistory_mutex );
    return true;
  }

  // It's a new user !
  bool authOK=false;
  string loginPwd=base64_decode(pwdb64.c_str());
  printf ("%s => %s", loginPwd.c_str(), pwdb64.c_str());
  size_t loginPwdSep=loginPwd.find(':');
  if (loginPwdSep==string::npos)
  {
    pthread_mutex_unlock( &usersAuthHistory_mutex );
    return false;
  }

  login=loginPwd.substr(0,loginPwdSep);
  string pwd=loginPwd.substr(loginPwdSep+1);

  vector<string> httpAuthLoginPwd=authLoginPwdList;
  if (httpAuthLoginPwd.size())
  {
    string logPass=login+':'+pwd;
    for ( vector<string>::iterator it=httpAuthLoginPwd.begin(); it != httpAuthLoginPwd.end(); it++ )
      if ( logPass == *it )
        authOK=true;
  }
#if defined(LINUX) || defined(__darwin__)
  if (!authOK && isAuthPam())
  {
    if (authPamUsersList.size())
    {
      for ( vector<string>::iterator it=authPamUsersList.begin(); it != authPamUsersList.end(); it++ )
        if (login == *it)
          authOK=AuthPAM::authentificate(login.c_str(), pwd.c_str(), pamService.c_str());
    }
    else
      authOK=AuthPAM::authentificate(login.c_str(), pwd.c_str(), pamService.c_str() );
  }    
#endif
  if (authOK)
  {
    NVJ_LOG->append(NVJ_INFO,"WebServer: Authentification passed for user '"+login+"'");
    if (i == usersAuthHistory.end())
      usersAuthHistory[pwdb64]=t;
  }
  else
    NVJ_LOG->append(NVJ_INFO,"WebServer: Authentification failed for user '"+login+"'");

  pthread_mutex_unlock( &usersAuthHistory_mutex );
  return authOK;
}

/***********************************************************************
* recvLine:  Receive an ascii line from a socket
* @param c - the socket connected to the client
* \return always NULL
***********************************************************************/

size_t WebServer::recvLine(int client, char *bufLine, size_t nsize)
{
  size_t bufLineLen=0;
  char c;
  int n;
  do
  {
    n = recv(client, &c, 1, 0);

    if (n > 0)
      bufLine[bufLineLen++] = c;
  } 
  while ((bufLineLen + 1 < nsize ) && (c != '\n') && ( n > 0 ));
  bufLine[bufLineLen] = '\0';

  return bufLineLen;
}


/***********************************************************************
* accept_request:  Process a request
* @param c - the socket connected to the client
* \return true if the socket must to close
***********************************************************************/

bool WebServer::accept_request(ClientSockData* client)
{
  char bufLine[BUFSIZE];
  HttpRequestMethod requestMethod;
  size_t postContentLength=0;
  bool urlencodedForm=false;
  char urlBuffer[BUFSIZE];
  size_t nbFileKeepAlive=5;

  char requestParams[BUFSIZE], requestCookies[BUFSIZE], requestOrigin[BUFSIZE], webSocketClientKey[BUFSIZE];
  bool websocket=false;
  int webSocketVersion=-1;
  string username;
  struct stat st;
  size_t bufLineLen=0;
  BIO *ssl_bio = NULL;

  if (sslEnabled)
  {
    client->bio=BIO_new(BIO_f_buffer());
    ssl_bio=BIO_new(BIO_f_ssl());
    BIO_set_ssl(ssl_bio,client->ssl,BIO_CLOSE);
    BIO_push(client->bio,ssl_bio);
  }

  bool crlfEmptyLineFound=false;
  unsigned i=0, j=0;
  
  bool authOK = !isAuthPam() && authLoginPwdList.size() == 0;
  char httpVers[4]="";
  int keepAlive=-1;
  bool isQueryStr=false;
  
  do
  {
    requestMethod=UNKNOWN_METHOD;
    postContentLength=0;
    urlencodedForm=false;
    *urlBuffer='\0';
    char *url=urlBuffer;
    *requestParams='\0';
    *requestCookies='\0';
    *requestOrigin='\0';
    websocket=false;
    *webSocketClientKey='\0';
    webSocketVersion=-1;
    username="";
    crlfEmptyLineFound=false;
    keepAlive=-1;
    isQueryStr=false;

    while (!crlfEmptyLineFound)
    {
      bufLineLen=0;
      *bufLine='\0';

      if (sslEnabled)
      {
        int r=BIO_gets(client->bio, bufLine, BUFSIZE-1);

        switch(SSL_get_error(client->ssl,r))
        {
          case SSL_ERROR_NONE:
            bufLineLen=r;
            break;
          case SSL_ERROR_ZERO_RETURN:
            return true;      
        }
      }
      else
        bufLineLen=recvLine(client->socketId, bufLine, BUFSIZE-1);

      if (bufLineLen == 0 || exiting)
        return true;

      if ( bufLineLen <= 2) 
        crlfEmptyLineFound = (*bufLine=='\n') || (*bufLine=='\r' && *(bufLine+1)=='\n');        
      else
      {
        if (bufLineLen == BUFSIZE-1) *(bufLine+bufLineLen)='\0';
        else *(bufLine+bufLineLen-2)='\0';
        j = 0; while (isspace((int)(bufLine[j])) && j < (unsigned)bufLineLen) j++;

        if ( strncmp(bufLine+j, authStr, sizeof authStr - 1 ) == 0)
        {
          j+=sizeof authStr - 1;

          // decode login/passwd
          string pwdb64="";
          while ( j < (unsigned)bufLineLen && *(bufLine + j) != 0x0d && *(bufLine + j) != 0x0a)
            pwdb64+=*(bufLine + j++);;
          if (!authOK)
            authOK=isUserAllowed(pwdb64, username);
          continue;
        }
                 
        if (strncasecmp(bufLine+j, "Connection: ", 12) == 0)
        { 
          j+=12; 
          if (strstr(bufLine+j,"pgrade") != NULL) websocket=true;
          if (strstr(bufLine+j,"lose") != NULL) keepAlive=false;
          else if ((strstr(bufLine+j,"eep-") != NULL) && (strstr(bufLine+j+4,"live") != NULL)) keepAlive=true;
          continue;
        }

        if (strncasecmp(bufLine+j, "Accept-Encoding: ",17) == 0) { j+=17; if (strstr(bufLine+j,"gzip") != NULL) client->compression=GZIP; continue; }
// Unused:
        if (strncasecmp(bufLine+j, "Content-Type: application/x-www-form-urlencoded", 47) == 0) { urlencodedForm=true; continue; }

        if (strncasecmp(bufLine+j, "Content-Length: ",16) == 0) { j+=16; postContentLength = atoi(bufLine+j); continue; }

        if (strncasecmp(bufLine+j, "Cookie: ",8) == 0) { j+=8; strcpy(requestCookies, bufLine+j); continue; }

        if (strncasecmp(bufLine+j, "Origin: ",8) == 0) { j+=8; strcpy(requestOrigin, bufLine+j); continue; }
        
        if (strncasecmp(bufLine+j, "Sec-WebSocket-Key: ", 19) == 0) { j+=19; strcpy(webSocketClientKey, bufLine+j); continue; }

// Not working:
//      if (strncasecmp(bufLine+j, "Sec-WebSocket-Extensions: ", 26) == 0) { j+=26; if (strstr(bufLine+j, "permessage-deflate")  != NULL) client->compression=ZLIB; continue; }
        
        if (strncasecmp(bufLine+j, "Sec-WebSocket-Version: ", 23) == 0) { j+=23; webSocketVersion = atoi(bufLine+j); continue; }

        isQueryStr=false;
        if (strncmp(bufLine+j, "GET", 3) == 0)
          {  requestMethod=GET_METHOD; isQueryStr=true; j+=4; }
        
        if (strncmp(bufLine+j, "POST", 4) == 0)
          {  requestMethod=POST_METHOD; isQueryStr=true; j+=5; }

        if (strncmp(bufLine+j, "PUT", 6) == 0)
          {  requestMethod=PUT_METHOD; isQueryStr=true; j+=7; }
          
        if (strncmp(bufLine+j, "DELETE", 6) == 0)
          {  requestMethod=DELETE_METHOD; isQueryStr=true; j+=7; }

        if (isQueryStr)
        {
          while (isspace((int)(bufLine[j])) && j < bufLineLen) j++;

          i=0; while (!isspace((int)(bufLine[j])) && (i < BUFSIZE - 1) && (j < bufLineLen) && bufLine[j]!='?') urlBuffer[i++] = bufLine[j++];
          urlBuffer[i]='\0';

          if ( !urlencodedForm && (bufLine[j] == '?') )
            { i=0; j++; while (!isspace((int)(bufLine[j])) && (i < BUFSIZE - 1) && (j < bufLineLen)) requestParams[i++] = bufLine[j++];
            requestParams[i]='\0'; }
          else requestParams[0]='\0';

          while (isspace((int)(bufLine[j])) && j < (unsigned)bufLineLen) j++;
          if (strncmp(bufLine+j, "HTTP/", 5) == 0)
          {
            strncpy (httpVers, bufLine+j+5, 3);
            *(httpVers+3)='\0';
            j+=8;
          }
        }
      }
    }

    if (!authOK)
    {
      std::string msg = getHttpHeader( "401 Authorization Required", 0, false);
      httpSend(client, (const void*) msg.c_str(), msg.length());
      return true;
    }

    if ( requestMethod == UNKNOWN_METHOD )
    {
      std::string msg = getNotImplementedErrorMsg();
      httpSend(client, (const void*) msg.c_str(), msg.length());
      return true;
    }

    if ( urlencodedForm )
    {
      if (sslEnabled)
      {
        int r=BIO_gets(client->bio, requestParams, BUFSIZE);

        switch(SSL_get_error(client->ssl,r))
        {
          case SSL_ERROR_NONE:
            bufLineLen=r;
            break;
          case SSL_ERROR_ZERO_RETURN:
            return true;      
        }
      }
      else
        bufLineLen=recvLine(client->socketId, requestParams, BUFSIZE);
    }

    if ( (url[strlen(url) - 1] == '/') && (strlen(url)+12 < BUFSIZE) )
        strcpy (url + strlen(url) - 1, "/index.html");
        
    do { url++; } while (strlen(url) && url[0]=='/');
    
    char logBuffer[BUFSIZE];
    snprintf(logBuffer, BUFSIZE, "Request : url='%s'  reqType='%d'  param='%s'  requestCookies='%s'  (httpVers=%s keepAlive=%d zipSupport=%d)\n", url, requestMethod, requestParams, requestCookies, httpVers, keepAlive, client->compression );
    NVJ_LOG->append(NVJ_DEBUG, logBuffer);

    // Process the query
    if (keepAlive==-1) 
      keepAlive = ( strncmp (httpVers,"1.1", 3) >= 0 );
      
    /* *************************
    /  * processing WebSockets *
    /  *************************/
    
    if (websocket)
    {
      //search endpoint
      std::map<std::string,WebSocket*>::iterator it;

      it = webSocketEndPoints.find(url);
      if (it != webSocketEndPoints.end()) // FOUND
      {
        WebSocket* webSocket=it->second;
        std::string header = getHttpWebSocketHeader("101 Switching Protocols", webSocketClientKey, client->compression == ZLIB);

        httpSend(client, (const void*) header.c_str(), header.length());
        HttpRequest* request=new HttpRequest(requestMethod, url, requestParams, requestCookies, requestOrigin, username, client);

        if (webSocket->onOpening(request))
          startWebSocketListener(webSocket, request);
        return false;
      }
      else
      {
        char bufLinestr[300]; snprintf(bufLinestr, 300, "Webserver: Websocket not found %s",  url);
        NVJ_LOG->append(NVJ_WARNING,bufLinestr);

        std::string msg = getNotFoundErrorMsg();
        httpSend(client, (const void*) msg.c_str(), msg.length());
        return true;
      }
    }
    
    /* ********************* */

    bool fileFound=false;
    unsigned char *webpage = NULL;
    size_t webpageLen = 0;
    unsigned char *gzipWebPage=NULL;
    int sizeZip=0;
    bool zippedFile=false;

#ifdef DEBUG_TRACES
    printf( "url: %s?%s\n", url, requestParams ); fflush(NULL);
#endif

    HttpRequest request(requestMethod, url, requestParams, requestCookies, requestOrigin, username, client);

    const char *mime=get_mime_type(url); 
    string mimeStr; if (mime != NULL) mimeStr=mime;
    HttpResponse response(mimeStr);

    std::vector<WebRepository *>::const_iterator repo=webRepositories.begin();
    for( ; repo!=webRepositories.end() && !fileFound && !zippedFile;)
    {
      if (*repo == NULL) continue;

      fileFound = (*repo)->getFile(&request, &response);
      if (fileFound && response.getForwardedUrl() != "")
      {
        strncpy( url, response.getForwardedUrl().c_str(), BUFSIZE - 1);
        *(urlBuffer + BUFSIZE - 1)='\0';
        response.forwardTo("");
        repo=webRepositories.begin(); fileFound=false;
      }
      else
         repo++;
    }
    
    if (!fileFound)
    {
      char bufLinestr[300]; snprintf(bufLinestr, 300, "Webserver: page not found %s",  url);
      NVJ_LOG->append(NVJ_WARNING,bufLinestr);

      std::string msg = getNotFoundErrorMsg();
      httpSend(client, (const void*) msg.c_str(), msg.length());
      return true;
    }
    else
    {
      repo--;
      response.getContent(&webpage, &webpageLen, &zippedFile);
      
      if ( webpage == NULL || !webpageLen)
      {
        std::string msg = getNoContentErrorMsg();
        httpSend(client, (const void*) msg.c_str(), msg.length());
        return true;
      }
        
      if (zippedFile)
      {
        gzipWebPage = webpage;
        sizeZip = webpageLen;
      }
    }
    
    char bufLinestr[300]; snprintf(bufLinestr, 300, "Webserver: page found %s",  url);
    NVJ_LOG->append(NVJ_DEBUG,bufLinestr);

    if ( (client->compression == NONE) && zippedFile )
    {
      // Need to uncompress
      try
      {
        if ((int)(webpageLen=nvj_gunzip( &webpage, gzipWebPage, sizeZip )) < 0)
        {
          NVJ_LOG->append(NVJ_ERROR, "Webserver: gunzip decompression failed !");
          std::string msg = getInternalServerErrorMsg();
          httpSend(client, (const void*) msg.c_str(), msg.length());
          return true;
        }
      }
      catch(...)
      {
          NVJ_LOG->append(NVJ_ERROR, "Webserver: nvj_gunzip raised an exception");
          std::string msg = getInternalServerErrorMsg();
          httpSend(client, (const void*) msg.c_str(), msg.length());
          return true;
      }
    }

    // Need to compress
    if ( !zippedFile && (client->compression == GZIP) && (webpageLen > 2048) )
    {
      const char *mimetype=response.getMimeType().c_str();
      if (mimetype != NULL && (strncmp(mimetype,"application",11) == 0 || strncmp(mimetype,"text",4) == 0))
      {
        try
        {
          if ((int)(sizeZip=nvj_gzip( &gzipWebPage, webpage, webpageLen )) < 0)
          {
            NVJ_LOG->append(NVJ_ERROR, "Webserver: gunzip compression failed !");
            std::string msg = getInternalServerErrorMsg();
            httpSend(client, (const void*) msg.c_str(), msg.length());
            return true;
          }
          else
            if ((size_t)sizeZip>webpageLen)
            {
              sizeZip=0;
              free (gzipWebPage);
            }
          }
          catch(...)
          {
              NVJ_LOG->append(NVJ_ERROR, "Webserver: nvj_gzip raised an exception");
              std::string msg = getInternalServerErrorMsg();
              httpSend(client, (const void*) msg.c_str(), msg.length());
              return true;
          }
      }
    }

    if (keepAlive && !(--nbFileKeepAlive)) keepAlive=false;

    if (sizeZip>0 && (client->compression == GZIP))
    {  
      std::string header = getHttpHeader("200 OK", sizeZip, keepAlive, true, &response);
      httpSend(client, (const void*) header.c_str(), header.length());
      httpSend(client, (const void*) gzipWebPage, sizeZip);
    }
    else
    {
      std::string header = getHttpHeader("200 OK", webpageLen, keepAlive, false, &response);
      httpSend(client, (const void*) header.c_str(), header.length());
      httpSend(client, (const void*) webpage, webpageLen);
    }

    if (sizeZip>0 && !zippedFile) // cas compression = double desalloc
    {
      free (gzipWebPage);
      (*repo)->freeFile(webpage); 
      continue;
    }

    if ((client->compression == NONE) && zippedFile) // cas décompression = double desalloc
    {
      free (webpage);
      (*repo)->freeFile(gzipWebPage);
      continue;
    }

    (*repo)->freeFile(webpage); 

  }
  while (keepAlive && !exiting);
  return true;
}

/***********************************************************************
* httpSend
***********************************************************************/

void WebServer::httpSend(ClientSockData *client, const void *buf, size_t len)
{
  if (sslEnabled)
  {
    while (BIO_write(client->bio, buf, len) <= 0)
    {
      if(! BIO_should_retry(client->bio))
      {
          NVJ_LOG->append(NVJ_WARNING, "WebServer: BIO_write failed !");
          return ;
      }
      // retry
    }

    BIO_flush(client->bio);
  }
  else
    sendCompat (client->socketId, buf, len, 0);
}

/***********************************************************************
* fatalError:  Print out a system error and exit
* @param s - error message
***********************************************************************/

void WebServer::fatalError(const char *s)
{
  NVJ_LOG->append(NVJ_FATAL,string(s)+": "+string(strerror(errno)));
  ::exit(1);
}

/***********************************************************************
* setSocketRcvTimeout:  Place a timeout on the socket
* @param socket -  socket descriptor
* @param seconds - number of seconds
* \return result of setsockopt (successfull: 0, otherwise -1)
***********************************************************************/

int WebServer::setSocketRcvTimeout(int socket, int seconds)
{
  struct timeval tv;
  int set = 1;

  tv.tv_sec = seconds ;
  tv.tv_usec = 0  ;

  return setsockoptCompat (socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof tv) && setsockoptCompat(socket, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
}


/***********************************************************************
* get_mime_type: return valid mime_type using filename's extension 
* @param name - filename
* \return mime_type or NULL is no found
***********************************************************************/

const char* WebServer::get_mime_type(const char *name)
{
  char *ext = strrchr(const_cast<char*>(name), '.');
  if (!ext) return NULL;

  char extLowerCase[6]; unsigned i=0;
  for (; i<5 && i<strlen(ext); i++)
    { extLowerCase[i]=ext[i]; if((extLowerCase[i]>='A')&&(extLowerCase[i]<='Z')) extLowerCase[i]+= 'a'-'A'; }
  extLowerCase[i]='\0';

  if (strcmp(extLowerCase, ".html") == 0 || strcmp(extLowerCase, ".htm") == 0) return "text/html";
  if (strcmp(extLowerCase, ".js") == 0) return "application/javascript";
  if (strcmp(extLowerCase, ".json") == 0) return "application/json";
  if (strcmp(extLowerCase, ".xml") == 0) return "application/xml";
  if (strcmp(extLowerCase, ".jpg") == 0 || strcmp(extLowerCase, ".jpeg") == 0) return "image/jpeg";
  if (strcmp(extLowerCase, ".gif") == 0) return "image/gif";
  if (strcmp(extLowerCase, ".png") == 0) return "image/png";
  if (strcmp(extLowerCase, ".css") == 0) return "text/css";
  if (strcmp(extLowerCase, ".txt") == 0) return "text/plain";
  if (strcmp(extLowerCase, ".au") == 0) return "audio/basic";
  if (strcmp(extLowerCase, ".wav") == 0) return "audio/wav";
  if (strcmp(extLowerCase, ".avi") == 0) return "video/x-msvideo";
  if (strcmp(extLowerCase, ".mpeg") == 0 || strcmp(extLowerCase, ".mpg") == 0) return "video/mpeg";
  if (strcmp(extLowerCase, ".mp3") == 0) return "audio/mpeg";
  if (strcmp(extLowerCase, ".csv") == 0) return "text/csv";
  if (strcmp(extLowerCase, ".mp4") == 0) return "application/mp4";
  if (strcmp(extLowerCase, ".bin") == 0) return "application/octet-stream";
  if (strcmp(extLowerCase, ".doc") == 0 || strcmp(extLowerCase, ".docx") == 0) return "application/msword";
  if (strcmp(extLowerCase, ".pdf") == 0) return "application/pdf";
  if (strcmp(extLowerCase, ".ps") == 0 || strcmp(extLowerCase, ".eps") == 0 || strcmp(extLowerCase, ".ai") == 0) return "application/postscript";
  if (strcmp(extLowerCase, ".tar") == 0) return "application/x-tar";
  if (strcmp(extLowerCase, ".h264") == 0) return "video/h264";
  if (strcmp(extLowerCase, ".dv") == 0) return "video/dv";
  if (strcmp(extLowerCase, ".qt") == 0 || strcmp(extLowerCase, ".mov") == 0) return "video/quicktime";

  return NULL;
}

/***********************************************************************
* getHttpHeader: generate HTTP header
* @param messageType - client socket descriptor
* @param len - HTTP message type
* @param keepAlive 
* @param zipped - true is content will be compressed
* @param response - the HttpResponse
* \return result of send function (successfull: >=0, otherwise <0)
***********************************************************************/

std::string WebServer::getHttpHeader(const char *messageType, const size_t len, const bool keepAlive, const bool zipped, HttpResponse* response)
{
  char timeBuf[200];
  time_t rawtime;
  struct tm timeinfo;

  std::string header="HTTP/1.1 "+std::string(messageType)+std::string("\r\n");
  time ( &rawtime );
  gmtime_r ( &rawtime, &timeinfo );
  strftime (timeBuf,200,"Date: %a, %d %b %Y %H:%M:%S GMT", &timeinfo);
  header+=std::string(timeBuf)+"\r\n";

  header+=webServerName+"\r\n";

  if (strncmp(messageType, "401", 3) == 0)
    header+=std::string("WWW-Authenticate: Basic realm=\"Restricted area: please enter Login/Password\"\r\n");
  
  if (response != NULL)
  {
    if ( response->isCORS() )
    {
      header += "Access-Control-Allow-Origin: " + response->getCORSdomain() + "\r\nAccess-Control-Allow-Credentials: ";
      if ( response->isCORSwithCredentials() )
        header += "true\r\n";
      else header+="false\r\n";
    } 

    std::vector<std::string>& cookies=response->getCookies();
    for (unsigned i=0; i < cookies.size(); i++)
      header+="Set-Cookie: " + cookies[i] + "\r\n";
  }
   
  header+="Accept-Ranges: bytes\r\n";

  if (keepAlive)
    header+="Connection: Keep-Alive\r\n";
  else
    header+="Connection: close\r\n";

  string mimetype="text/html";
  if (response != NULL)
    mimetype=response->getMimeType();
  header+="Content-Type: "+ mimetype  + "\r\n";
  
  if (zipped)
    header+="Content-Encoding: gzip\r\n";
  
  if (len)
  {
    std::stringstream lenSS; lenSS << len;
    header+="Content-Length: "+lenSS.str()+ "\r\n";
  }
 
  header+= "\r\n";

  return header;
}


/**********************************************************************
* getNoContentErrorMsg: send a 204 No Content Message
* \return the http message to send
***********************************************************************/

std::string WebServer::getNoContentErrorMsg()
{
  std::string header=getHttpHeader( "204 No Content", 0, false );

  return header;

}

/**********************************************************************
* sendBadRequestError: send a 400 Bad Request Message
* \return the http message to send
***********************************************************************/

std::string  WebServer::getBadRequestErrorMsg()
{
  std::string errorMessage="<HTML><HEAD>\n<TITLE>400 Bad Request</TITLE>\n</HEAD><body>\n<h1>Bad Request</h1>\n \
                <p>Your browser sent a request that this server could not understand.<br />\n</p>\n</body></HTML>\n";

  std::string header=getHttpHeader( "400 Bad Request", errorMessage.length(), false);

  return header+errorMessage;
}

/***********************************************************************
* sendNotFoundError: send a 404 not found error message
* \return the http message to send
***********************************************************************/

std::string WebServer::getNotFoundErrorMsg()
{
  std::string errorMessage="<HTML><HEAD><TITLE>Object not found!</TITLE><body><h1>Object not found!</h1>\n" \
                "<p>\n\n\nThe requested URL was not found on this server.\n\n\n\n    If you entered the URL manually please check your spelling and try again.\n\n\n</p>\n" \
                "<h2>Error 404</h2></body></HTML>\n";
 
  std::string header=getHttpHeader( "404 Not Found", errorMessage.length(), false );

  return header+errorMessage;

}

/***********************************************************************
* sendInternalServerError: send a 500 Internal Server Error
* \return the http message to send
***********************************************************************/

std::string WebServer::getInternalServerErrorMsg()
{
  std::string errorMessage="<HTML><HEAD><TITLE>Internal Server Error!</TITLE><body><h1>Internal Server Error!</h1>\n" \
                "<p>\n\n\nSomething happens.\n\n\n\n    If you entered the URL manually please check your spelling and try again.\n\n\n</p>\n" \
                "<h2>Error 500</h2></body></HTML>\n";
  std::string header=getHttpHeader( "500 Internal Server Error", errorMessage.length(), false );

  return header+errorMessage;
}


/***********************************************************************
* sendInternalServerError: send a 501 Method Not Implemented
* \return the http message to send
***********************************************************************/

std::string WebServer::getNotImplementedErrorMsg()
{
  std::string errorMessage="<HTML><HEAD><TITLE>Cannot process request!</TITLE><body><h1>Cannot process request!</h1>\n" \
                "<p>\n\n\n   The server does not support the action requested by the browser.\n\n\n\n" \
                "If you entered the URL manually please check your spelling and try again.\n\n\n</p>\n" \
                "<h2>Error 501</h2></body></HTML>\n";

  std::string header=getHttpHeader( "501 Method Not Implemented", errorMessage.length(), false );

  return header+errorMessage;
}


/***********************************************************************
* init: Initialize server listening socket
* \return Port server used
***********************************************************************/

u_short WebServer::init()
{
  // Build SSL context
  if (sslEnabled)
    initialize_ctx(sslCertFile.c_str(), sslCaFile.c_str(), sslCertPwd.c_str());

  struct addrinfo  hints;
  struct addrinfo *result, *rp;

  threadWebServer=0;
  nbServerSock=0;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
  hints.ai_socktype = SOCK_STREAM; /* TCP socket */
  hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;

  char portStr[10];
  snprintf(portStr ,10, "%d", tcpPort);

  if (getaddrinfo(NULL, portStr, &hints, &result) != 0)
    fatalError("WebServer : getaddrinfo error ");

  for (rp = result; rp != NULL && nbServerSock < sizeof(server_sock)/sizeof(int) ; rp = rp->ai_next)
  {
    if ( (server_sock[ nbServerSock ] = socket( rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1 ) continue;

    int optval = 1;

    setsockoptCompat( server_sock [ nbServerSock ], SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);


    if (device.length())
    {
#ifndef LINUX
      NVJ_LOG->append(NVJ_WARNING, "WebServer: HttpdDevice parameter will be ignored on your system");
#else
      setsockopt( server_sock [ nbServerSock ], SOL_SOCKET, SO_BINDTODEVICE, device.c_str(), device.length());
#endif
    }

    if ( rp->ai_family == PF_INET && disableIpV4) continue;

    if ( rp->ai_family == PF_INET6 )
    {
      if (disableIpV6) continue;
#if defined( IPV6_V6ONLY )
      
      //Disable IPv4 mapped addresses.
      
      int v6Only = 1;

      setsockoptCompat( server_sock [ nbServerSock ], IPPROTO_IPV6, IPV6_V6ONLY, &v6Only, sizeof( v6Only ) ) ;
                
#else
      NVJ_LOG->append(NVJ_WARNING, "WebServer: Cannot set IPV6_V6ONLY socket option.  Closing IPv6 socket.");
      close(  server_sock[ nbServerSock ] );
      continue; 
#endif
    }
    if ( bind( server_sock[ nbServerSock ], rp->ai_addr, rp->ai_addrlen) == 0 )
      if ( listen(server_sock [ nbServerSock ], 128) >= 0 )
      {
        nbServerSock ++;                  /* Success */
        continue;
      }

    close( server_sock[ nbServerSock ] );
  }
  freeaddrinfo(result);           /* No longer needed */

  if (nbServerSock == 0)
    fatalError("WebServer : Init Failed ! (nbServerSock == 0)");

  return ( tcpPort );
}


/***********************************************************************
* exit: Stop http server
***********************************************************************/

void WebServer::exit()
{
  pthread_mutex_lock( &clientsQueue_mutex );
  exiting=true;

  while (nbServerSock>0)
  {
    shutdown ( server_sock[ --nbServerSock ], 2 ) ;
    close (server_sock[ nbServerSock ]);
  }
  pthread_mutex_unlock( &clientsQueue_mutex );

  pthread_mutex_lock(&webSocketClientList_mutex);
  for (std::list<int>::iterator it = webSocketClientList.begin(); it != webSocketClientList.end();)
  {
    shutdown ( *it, 2 ) ;
    close ( *it );
    it = webSocketClientList.erase(it);
  }
  pthread_mutex_unlock(&webSocketClientList_mutex);
}

/***********************************************************************
* password_cb
************************************************************************/

int WebServer::password_cb(char *buf, int num, int rwflag, void *userdata)
{
  if((size_t)num<strlen(certpass)+1)
    return(0);

  strcpy(buf,certpass);
  return(strlen(certpass));
}

/***********************************************************************
* verify_callback: 
************************************************************************/

int WebServer::verify_callback(int preverify_ok, X509_STORE_CTX *ctx)
{
  char    buf[256];
  X509   *err_cert;
  int     err, depth;

  err_cert = X509_STORE_CTX_get_current_cert(ctx);
  err = X509_STORE_CTX_get_error(ctx);
  depth = X509_STORE_CTX_get_error_depth(ctx);

  X509_NAME_oneline(X509_get_subject_name(err_cert), buf, 256);

  /* Catch a too long certificate chain */
  if (depth > verify_depth)
  {
     preverify_ok = 0;
     err = X509_V_ERR_CERT_CHAIN_TOO_LONG;
     X509_STORE_CTX_set_error(ctx, err);
  }
  if (!preverify_ok) 
  {
    char buftmp[300];
    snprintf(buftmp, 300, "X509_verify_cert error: num=%d:%s:depth=%d:%s", err,
              X509_verify_cert_error_string(err), depth, buf);
    NVJ_LOG->append(NVJ_INFO,buftmp);
  }

  /*
  * At this point, err contains the last verification error. We can use
  * it for something special
  */
  if (!preverify_ok && (err == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT))
  {
   X509_NAME_oneline(X509_get_issuer_name(ctx->current_cert), buf, 256);
   char buftmp[300]; snprintf(buftmp, 300, "X509_verify_cert error: issuer= %s", buf);
     NVJ_LOG->append(NVJ_INFO,buftmp);
  }

  return 1;
}


/***********************************************************************
* initialize_ctx: 
************************************************************************/

void WebServer::initialize_ctx(const char *certfile, const char *cafile, const char *password)
{
  /* Global system initialization*/
  SSL_library_init();
  SSL_load_error_strings();

  /* Create our context*/
  sslCtx=SSL_CTX_new(SSLv23_method());

  /* Load our keys and certificates*/
  if(!(SSL_CTX_use_certificate_chain_file(sslCtx, certfile)))
  {
    NVJ_LOG->append(NVJ_FATAL,"OpenSSL error: Can't read certificate file");
    ::exit(1);
  }

  certpass=(char*)password;
  SSL_CTX_set_default_passwd_cb(sslCtx, WebServer::password_cb);
  if(!(SSL_CTX_use_PrivateKey_file(sslCtx, certfile, SSL_FILETYPE_PEM)))
  {
    NVJ_LOG->append(NVJ_FATAL,"OpenSSL error: Can't read key file");
    ::exit(1);
  }

  SSL_CTX_set_session_id_context(sslCtx, (const unsigned char*)&s_server_session_id_context, sizeof s_server_session_id_context); 

  if ( authPeerSsl )
  {
      if(!(SSL_CTX_load_verify_locations(sslCtx, cafile,0)))
      {
        NVJ_LOG->append(NVJ_FATAL,"OpenSSL error: Can't read CA list");
        ::exit(1);
      }

      SSL_CTX_set_verify(sslCtx, SSL_VERIFY_PEER|SSL_VERIFY_CLIENT_ONCE,
                  verify_callback);

      SSL_CTX_set_verify_depth(sslCtx, verify_depth + 1);
  }
}
     
/**********************************************************************/

bool WebServer::isAuthorizedDN(const std::string str)
{
  bool res = false;
  for( std::vector<std::string>::const_iterator i=authDnList.begin(); i!=authDnList.end() && !res; i++)
    res = (*i == str);
  return res;
}

/**********************************************************************/
  
void WebServer::poolThreadProcessing()
{
  BIO *sbio;
  SSL *ssl=NULL;
  X509 *peer=NULL;
  bool authSSL=false;


  while( !clientsQueue.empty() || !exiting )
  {
    pthread_mutex_lock( &clientsQueue_mutex );

    while( clientsQueue.empty() && !exiting)
      pthread_cond_wait( &clientsQueue_cond, &clientsQueue_mutex );

    if (exiting)  { pthread_mutex_unlock( &clientsQueue_mutex ); break; }

    // clientsQueue is not empty
    ClientSockData* client = clientsQueue.front();
    clientsQueue.pop();

    pthread_mutex_unlock( &clientsQueue_mutex );
    
    if (sslEnabled)
    {
      sbio=BIO_new_socket(client->socketId, BIO_NOCLOSE);
      ssl=SSL_new(sslCtx);
      SSL_set_bio(ssl,sbio,sbio);

      if (SSL_accept(ssl) <= 0)
      { const char *sslmsg=ERR_reason_error_string(ERR_get_error());
        string msg="SSL accept error ";
        if (sslmsg != NULL) msg+=": "+string(sslmsg);
        NVJ_LOG->append(NVJ_DEBUG,msg);
      }
      
      client->ssl=ssl;
      client->bio=sbio;
      
      if ( authPeerSsl )
      {
        authSSL=false;        
        
        if ( (peer = SSL_get_peer_certificate(ssl)) != NULL )
        {
          if (SSL_get_verify_result(ssl) == X509_V_OK)
          {
            // The client sent a certificate which verified OK
            char *str = X509_NAME_oneline(X509_get_subject_name(peer), 0, 0);                  

            if ((authSSL=isAuthorizedDN(str)) == true)
            {
              client->peerDN = new std::string(str);
              updatePeerDnHistory(*(client->peerDN));
            }
            free (str);                           
            X509_free(peer);
          }
        }
      }
      else
        authSSL=true;
    }
    if (accept_request(client))
      freeClientSockData(client);
  }
  exitedThread++;

}


/***********************************************************************
* initPoolThreads: 
************************************************************************/

void WebServer::initPoolThreads()
{
  pthread_t newthread;
  for (unsigned i=0; i<threadsPoolSize; i++)
  {
    create_thread( &newthread, WebServer::startPoolThread, static_cast<void *>(this) );
    usleep(500);
  }
  exitedThread=0;
}


/***********************************************************************
* startThread: Launch http server
* @param p - port server to use. If port is 0, port value will be modified
*                 dynamically.
* \return NULL
************************************************************************/

void* WebServer::startThread(void* t)
{
  static_cast<WebServer *>(t)->threadProcessing();
  pthread_exit(NULL);
  return NULL;
}  


void WebServer::threadProcessing()
{
  int client_sock=0;

  exiting=false;
  exitedThread=0; 

  struct sockaddr_storage clientAddress;
  socklen_t clientAddressLength = sizeof(clientAddress);

  ushort port=init();

  initPoolThreads();
  httpdAuth = authLoginPwdList.size() || isAuthPam() ;

  if (isAuthPam())
  {
#if defined(LINUX) || defined(__darwin__)
          AuthPAM::start();
#else
          NVJ_LOG->appendUniq(NVJ_ERROR, "WebServer : WARNING authPAM will be ignored on your system");
#endif
  }

  char buf[300]; snprintf(buf, 300, "WebServer : Listen on port %d", port);
  NVJ_LOG->append(NVJ_INFO,buf);

  struct pollfd *pfd;
  if ( (pfd = (pollfd *)malloc( nbServerSock * sizeof( struct pollfd ) )) == NULL )
      fatalError("WebServer : malloc error ");

  unsigned idx;
  int status;

  for ( idx = 0; idx < nbServerSock; idx++ )
  {
      pfd[ idx ].fd = server_sock[ idx ];
      pfd[ idx ].events  = POLLIN;
      pfd[ idx ].revents = 0;
  }

  for (;!exiting;)
  {
    do
    {
    #ifdef __darwin__
      status = poll( pfd, nbServerSock, 500 );
    #else
      status = poll( pfd, nbServerSock, -1 );
    #endif
    }
    while ( ( status < 0 ) && ( errno == EINTR ) && !exiting );

    for ( idx = 0; idx < nbServerSock && !exiting; idx++ )
    {
      if ( !(pfd[idx].revents & POLLIN) )
              continue;
      client_sock = accept(pfd[idx].fd,
                       (struct sockaddr*)&clientAddress, &clientAddressLength);

      IpAddress webClientAddr;
              
      if ( clientAddress.ss_family == AF_INET )
      {
        webClientAddr.ipversion=4;
        webClientAddr.ip.v4=((struct sockaddr_in *)&clientAddress)->sin_addr.s_addr;
      }

      if ( clientAddress.ss_family == AF_INET6 )
      {
        webClientAddr.ipversion=6;
        webClientAddr.ip.v6=((struct sockaddr_in6 *)&clientAddress)->sin6_addr;
      }

      if (exiting) { close(pfd[idx].fd); break; };
    
      if ( hostsAllowed.size() 
        && !isIpBelongToIpNetwork(webClientAddr, hostsAllowed ) )
        {
          shutdown (client_sock, SHUT_RDWR);      
          close(client_sock);
          continue;
        }

      //

      updatePeerIpHistory(webClientAddr);
      if (client_sock == -1)
        NVJ_LOG->appendUniq(NVJ_ERROR, "WebServer : An error occurred when attempting to access the socket (accept == -1)");
      else
      {
        setSocketRcvTimeout(client_sock, 1);
        ClientSockData* client=(ClientSockData*)malloc(sizeof(ClientSockData));
        client->socketId=client_sock;
        client->ip=webClientAddr;
        client->ssl=NULL;
        client->bio=NULL;
        client->peerDN=NULL;
        pthread_mutex_lock( &clientsQueue_mutex );
        clientsQueue.push(client);
        pthread_mutex_unlock( &clientsQueue_mutex );
        pthread_cond_signal (& clientsQueue_cond);

      }

    }
  }

  while (exitedThread != threadsPoolSize)
  {
    pthread_cond_broadcast (& clientsQueue_cond);
    usleep(500);
  }

  // Exiting...
  free (pfd);

  if (sslEnabled)
    SSL_CTX_free(sslCtx);

  pthread_mutex_destroy(&clientsQueue_mutex);

#if defined(LINUX) || defined(__darwin__)
  if (isAuthPam())
    AuthPAM::stop();
#endif
 
}

/***********************************************************************/

void WebServer::closeSocket(ClientSockData* client)
{
  if (client->ssl != NULL)
  { 
    int n=SSL_shutdown(client->ssl);
    if(!n)
    {
      shutdown(client->socketId, 1);
      SSL_shutdown(client->ssl);
    }
    SSL_free(client->ssl);    
  }
  shutdown (client->socketId, SHUT_RDWR);      
  close(client->socketId);
}

/***********************************************************************
* base64_decode & base64_encode
  thanks to  René Nyffenegger rene.nyffenegger@adp-gmbh.ch for his
  public implementation of this algorithm
*
************************************************************************/      
std::string WebServer::base64_decode(const std::string& encoded_string)
{
             
  int in_len = encoded_string.size();
  int i = 0;
  int j = 0;
  int in_ = 0;
  unsigned char char_array_4[4], char_array_3[3];
  std::string ret;

  while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_]))
  {
    char_array_4[i++] = encoded_string[in_]; in_++;
    if (i ==4) {
      for (i = 0; i <4; i++)
        char_array_4[i] = base64_chars.find(char_array_4[i]);

      char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

      for (i = 0; (i < 3); i++)
        ret += char_array_3[i];
      i = 0;
    }
  }

  if (i)
  {
    for (j = i; j <4; j++)
      char_array_4[j] = 0;

    for (j = 0; j <4; j++)
      char_array_4[j] = base64_chars.find(char_array_4[j]);

    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
    char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

    for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
  }

  return ret;
}
             
std::string WebServer::base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len)
{
  std::string ret;
  int i = 0;
  int j = 0;
  unsigned char char_array_3[3];
  unsigned char char_array_4[4];

  while (in_len--) {
    char_array_3[i++] = *(bytes_to_encode++);
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;

      for(i = 0; (i <4) ; i++)
        ret += base64_chars[char_array_4[i]];
      i = 0;
    }
  }

  if (i)
  {
    for(j = i; j < 3; j++)
      char_array_3[j] = '\0';

    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    char_array_4[3] = char_array_3[2] & 0x3f;

    for (j = 0; (j < i + 1); j++)
      ret += base64_chars[char_array_4[j]];

    while((i++ < 3))
      ret += '=';

  }
  return ret;
}

/***********************************************************************
* SHA1_encode: Generate the SHA1 encoding
* @param input - the string to encode
* \return the encoded string
************************************************************************/
string WebServer::SHA1_encode(const string& input)
{
    string hash;
    SHA_CTX context;
    SHA1_Init(&context);
    SHA1_Update(&context, &input[0], input.size());
    hash.resize(160/8);
    SHA1_Final((unsigned char*)&hash[0], &context);
    return hash;
}

/***********************************************************************
* generateWebSocketServerKey: Generate the websocket server key
* @param webSocketKey - the websocket client key.
* \return the websocket server key
************************************************************************/
string WebServer::generateWebSocketServerKey(string webSocketKey)
{
  string sha1Key=SHA1_encode(webSocketKey+webSocketMagicString);
  return base64_encode(reinterpret_cast<const unsigned char*>(sha1Key.c_str()), sha1Key.length());
}

/***********************************************************************
* getHttpWebSocketHeader: generate HTTP header
* @param messageType - client socket descriptor
* \return the header
***********************************************************************/

std::string WebServer::getHttpWebSocketHeader(const char *messageType, const char* webSocketClientKey, const bool webSocketDeflate)
{
  char timeBuf[200];
  time_t rawtime;
  struct tm timeinfo;

  std::string header="HTTP/1.1 "+std::string(messageType)+std::string("\r\n");
  header+="Upgrade: websocket\r\n";
  header+="Connection: Upgrade\r\n";

  time ( &rawtime );
  gmtime_r ( &rawtime, &timeinfo );
  strftime (timeBuf,200,"Date: %a, %d %b %Y %H:%M:%S GMT", &timeinfo);
  header+=std::string(timeBuf)+"\r\n";

  header+=webServerName+"\r\n";

  header+="Sec-WebSocket-Accept: "+generateWebSocketServerKey(webSocketClientKey)+"\r\n";
  
  if (webSocketDeflate)
    header+="Sec-WebSocket-Extensions: permessage-deflate\r\n"; //x-webkit-deflate-frame
   
  header+= "\r\n";

  return header;
}

/***********************************************************************/

void WebServer::startWebSocketListener(WebSocket *websocket, HttpRequest* request)
{
  pthread_t newthread;
  WebSocketParams *p=(WebSocketParams *)malloc( sizeof(WebSocketParams) );
  p->webserver=this;
  p->websocket=websocket;
  p->request=request;
  create_thread( &newthread, WebServer::startThreadListenWebSocket, static_cast<void *>(p) );
}

/***********************************************************************/

void WebServer::listenWebSocket(WebSocket *websocket, HttpRequest* request)
{
  char bufferRecv[BUFSIZE];
  volatile bool closing=false;
  u_int64_t msgLength=0, msgContentIt=0;
  bool msgMask=false;

  unsigned char msgKeys[4];
  unsigned char* msgContent = NULL;
  enum MsgDecodSteps { FIRSTBYTE, LENGTH, MASK, CONTENT };

  ClientSockData* client = request->getClientSockData();
  
  setSocketRcvTimeout(client->socketId,0); // Remove socket timeout
  pthread_mutex_lock(&webSocketClientList_mutex);
  webSocketClientList.push_back(client->socketId);
  pthread_mutex_unlock(&webSocketClientList_mutex);

  websocket->addNewClient(request);

  bool fin=false;
  unsigned char rsv=0, opcode=0;  
  u_int64_t readLength=1;
  MsgDecodSteps step=FIRSTBYTE;
  memset( msgKeys, 0, 4*sizeof(unsigned char) );

  for (;!closing;)
  {

    int n=0;
    size_t it=0;
    size_t length=readLength;
    
    if (length > BUFSIZE)
    {
      length=BUFSIZE;
      readLength-=BUFSIZE;
    }

    do
    {
      if (client->bio != NULL && client->ssl != NULL)
      {
          n=BIO_read(client->bio, bufferRecv+it, length-it);

          if (SSL_get_error(client->ssl,n) == SSL_ERROR_ZERO_RETURN)
            closing=true;
      }
      else
      {
        n=recv(client->socketId, bufferRecv+it, length-it, 0);
        if ( n <= 0 )
        {
          if ( errno==ENOTCONN || errno==EBADF || errno==ECONNRESET ) 
            closing=true;
          continue;
        }
      }
      
      it += n;      
    }
    while (it != length && !closing);
    
    if (closing) continue;
    
    // Decode message header
    switch(step)
    {
    
      case FIRSTBYTE:     
        fin=(bufferRecv[0] & 0x80) >> 7;
        rsv=(bufferRecv[0] & 0x70) >> 4;
        opcode=bufferRecv[0] & 0xf;

        step=LENGTH;
        readLength=1;
        break;
          
      case LENGTH:    
        if (!msgLength)
        {    
          msgMask = (bufferRecv[0]  & 0x80) >> 7;
          if (!msgMask)
          {
            closing=true;
            continue;      
          }
          
          msgLength =  bufferRecv[0] & 0x7f;
          if (!msgLength)
          {
            NVJ_LOG->append(NVJ_WARNING, "Websocket: Message length is null. Closing socket.");
            msgLength=0;
            msgContent=NULL;
            step=CONTENT;
            continue;
          }
          if (msgLength == 126) { readLength=2; break; }
          if (msgLength == 127) { readLength=8; break; }
        }
        else
        { 
          if (msgLength == 126)
            msgLength=ntohs(*(u_int16_t*)bufferRecv);

          if (msgLength == 127)
            msgLength=ntohll(*(u_int64_t*)bufferRecv);
        }
        
        if ( (msgLength > 0x7FFF)
         || ( (msgContent = (unsigned char*)malloc(msgLength*sizeof(unsigned char))) == NULL ))
        {
          char logBuffer[500];
          snprintf(logBuffer, 500, " Websocket: Message content allocation failed (length: %llu)", static_cast<unsigned long long>(msgLength));
          NVJ_LOG->append(NVJ_WARNING, logBuffer);
          msgContent=NULL;
        }
        msgContentIt=0;
        if (msgMask) { step=MASK; readLength=4; } else { step=CONTENT; readLength=msgLength; }
        break;

      case MASK:
        memcpy(msgKeys, bufferRecv, 4*sizeof(unsigned char));
        readLength=msgLength;
        step=CONTENT;
        break;
        
      case CONTENT:
        char buf[300]; snprintf(buf, 300, "WebSocket: new message received (len=%llu fin=%d rsv=%d opcode=%d mask=%d)",
                                              static_cast<unsigned long long>(msgLength), fin, rsv, opcode, msgMask);
        NVJ_LOG->append(NVJ_DEBUG,buf);
        if (msgContent != NULL)
        {
          for (size_t i=0; i<length; i++)
          {
            if (msgMask)
              msgContent[msgContentIt] = bufferRecv[i] ^ msgKeys[msgContentIt % 4];
            else msgContent[msgContentIt] = bufferRecv[i];
            
            msgContentIt++;
          }
        }

        if (msgContentIt == msgLength)
        {      
          if (msgContent != NULL && (client->compression == ZLIB) && (rsv & 4) )
          {
            try
            {
              unsigned char *msg = NULL;
              size_t msgLen=nvj_gunzip( &msg, msgContent, msgLength, true );
              free(msgContent);
              msgContent=msg;
              msgLength=msgLen;
              
              msgContent[msgLength]='\0';
            }
            catch (std::exception& e)
            {
              NVJ_LOG->append(NVJ_ERROR, string(" Websocket: nvj_gzip raised an exception: ") +  e.what());
              msgLength = 0;
            }
          }

          switch(opcode)
          {
            case 0x1:
              if (msgLength)
                websocket->onTextMessage(request, string((char*)msgContent, msgLength), fin);
              else websocket->onTextMessage(request, "", fin);
              break;
            case 0x2:
              websocket->onBinaryMessage(request, msgContent, msgLength, fin);
              break;
            case 0x8:
              if (websocket->onCloseCtrlFrame(request, msgContent, msgLength))
              {
                webSocketSendCloseCtrlFrame(request, msgContent, msgLength);
                closing=true;
              }
              break;
            case 0x9:
              if (websocket->onPingCtrlFrame(request, msgContent, msgLength))
                webSocketSendPongCtrlFrame(request, msgContent, msgLength);
              break;
            case 0xa:
              websocket->onPongCtrlFrame(request, msgContent, msgLength);
              break;
            default:
              char buf[300]; snprintf(buf, 300, "WebSocket: message received with unknown opcode (%d) has been ignored", opcode);
              NVJ_LOG->append(NVJ_INFO,buf);
              break;
          }

          // FINISHED
  /*        if (msgContent != NULL)
            for (u_int64_t i=0; i<msgLength; i++)
              printf("%c(%2x)", msgContent[i],msgContent[i]);
            printf("\n"); fflush(NULL);*/
           
          if ((client->compression == ZLIB) && (rsv & 4) && msgLength)
            free (msgContent);
          fin=false; rsv=0;
          opcode=0;
          msgLength=0;
          msgContentIt=0;
          msgMask=false;
          memset( msgKeys, 0, 4*sizeof(unsigned char) );
          msgContent=NULL;
          readLength=1;
          step=FIRSTBYTE;
        }
        break;
    }
  }

  websocket->onClosing(request);

  websocket->removeClient(request);

  delete request;
  freeClientSockData(client);
  webSocketClientList.push_back(client->socketId);
  pthread_mutex_lock(&webSocketClientList_mutex);
  std::list<int>::iterator it = std::find(webSocketClientList.begin(), webSocketClientList.end(), client->socketId);
  if (it != webSocketClientList.end()) webSocketClientList.erase(it);
  pthread_mutex_unlock(&webSocketClientList_mutex);
}

/***********************************************************************/

void WebServer::webSocketSend(HttpRequest* request, const u_int8_t opcode, const unsigned char* message, size_t length, bool fin)
{
  ClientSockData* client = request->getClientSockData();

  unsigned char headerBuffer[10]; // 10 is the max header size 
  size_t headerLen=2; // default header size
  unsigned char *msg = NULL;
  size_t msgLen=0;
  
  headerBuffer[0]= 0x80 | (opcode & 0xf) ; // FIN & OPCODE:0x1
  if (client->compression == ZLIB)
  {
    headerBuffer[0] |= 0x40; // Set RSV1
    try
    {
      msgLen=nvj_gzip( &msg, message, length, true );
    }
    catch(...)
    {
      NVJ_LOG->append(NVJ_ERROR, " Websocket: nvj_gzip raised an exception");
      return;
    }
  }
  else
  {
    msg=(unsigned char*)message;
    msgLen=length;
  }
  
  
  if (msgLen < 126)
    headerBuffer[1]=msgLen;
  else
  {
    if (msgLen < 0xFFFF)
    {
      headerBuffer[1]=126;
      *(u_int16_t*)(headerBuffer+2)=htons((u_int16_t)msgLen);
      headerLen+=2;
    }
    else
    {
      headerBuffer[1]=127;
      *(u_int64_t*)(headerBuffer+2)=htonll((u_int64_t)msgLen);
      headerLen+=8;
    }
  }

  if (client->bio != NULL && client->ssl != NULL)
  {
    if ( (BIO_write(client->bio, headerBuffer, headerLen) <=0 ) || (BIO_write(client->bio,  msg, msgLen) <=0 ) )
    {
      freeClientSockData(client);      
    }
  }
  else
  {
    if ( (send(client->socketId, headerBuffer, headerLen, 0) < 0) || ( msgLen && (send(client->socketId, msg, msgLen, 0) < 0) ) )
    {
      freeClientSockData(client);
    }
  }

  
  if (client->compression == ZLIB)
    free (msg);
}

/***********************************************************************/

void WebServer::webSocketSendTextMessage(HttpRequest* request, const string &message, bool fin)
{
  webSocketSend(request, 0x1, (const unsigned char*)(message.c_str()), message.length(), fin);
}

/***********************************************************************/

void WebServer::webSocketSendBinaryMessage(HttpRequest* request, const unsigned char* message, size_t length, bool fin)
{
  webSocketSend(request, 0x2, message, length, fin);
}

/***********************************************************************/

void WebServer::webSocketSendPingCtrlFrame(HttpRequest* request, const unsigned char* message, size_t length)
{
  webSocketSend(request, 0x9, message, length, true);
}

void WebServer::webSocketSendPingCtrlFrame(HttpRequest* request, const string &message)
{
  webSocketSend(request, 0x9, (const unsigned char*)message.c_str(), message.length(), true);
}

/***********************************************************************/

void WebServer::webSocketSendPongCtrlFrame(HttpRequest* request, const unsigned char* message, size_t length)
{
  webSocketSend(request, 0xa, message, length, false);
}

void WebServer::webSocketSendPongCtrlFrame(HttpRequest* request, const string &message)
{
  webSocketSend(request, 0xa, (const unsigned char*)message.c_str(), message.length(), false);
}

/***********************************************************************/

void WebServer::webSocketSendCloseCtrlFrame(HttpRequest* request, const unsigned char* message, size_t length)
{
  webSocketSend(request, 0x8, message, length, true);
}

void WebServer::webSocketSendCloseCtrlFrame(HttpRequest* request, const string &message)
{
  webSocketSend(request, 0x8, (const unsigned char*)message.c_str(), message.length(), true);
}



