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

#include "navajo/WebServer.hh"
#if defined(LINUX) || defined(__darwin__)
#include "navajo/AuthPAM.hh"
#endif
#include "navajo/thread.h"
#include "gzip.cc"


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
time_t HttpSession::lastExpirationSearchTime=0;
time_t HttpSession::sessionLifeTime=20*60;

/*********************************************************************/

WebServer::WebServer()
{
  sslCtx=NULL;
  s_server_session_id_context = 1;

  webServerName=std::string("Server: Navajo/")+std::string(LIBNAVAJO_SOFTWARE_VERSION);
  exiting=false;
  exitedThread=0;
  httpdAuth=false;
  nbServerSock=0;
  
  disableIpV4=false;
  disableIpV6=false;
  tcpPort=DEFAULT_HTTP_PORT;
  threadsPoolSize=20;
  
  sslEnabled=false;
  authPeerSsl=false;
  authPam=false;

  pthread_mutex_init(&clientsSockLifo_mutex, NULL);
  pthread_cond_init(&clientsSockLifo_cond, NULL);

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

bool WebServer::isUserAllowed(const string &pwdb64)
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

  string login=loginPwd.substr(0,loginPwdSep);
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
* \return always NULL
***********************************************************************/

void WebServer::accept_request(int client, SSL *ssl)
{
  char bufLine[BUFSIZE];
  HttpRequestType requestType;
  size_t postContentLength=0;
  bool postContentTypeOk=false;
  char url[BUFSIZE];
  size_t nbFileKeepAlive=5;

  char requestParams[BUFSIZE], requestCookies[BUFSIZE];
  struct stat st;
  size_t bufLineLen=0;
  BIO *io = NULL,*ssl_bio = NULL;

  if (sslEnabled)
  {
    io=BIO_new(BIO_f_buffer());
    ssl_bio=BIO_new(BIO_f_ssl());
    BIO_set_ssl(ssl_bio,ssl,BIO_CLOSE);
    BIO_push(io,ssl_bio);
  }

  bool crlfEmptyLineFound=false;
  unsigned i=0, j=0;
  
  bool authOK = !isAuthPam() && authLoginPwdList.size() == 0;
  char httpVers[4]="";
  int keepAlive=-1;
  bool zipSupport=false;
  bool isQueryStr=false;
  
  do
  {
    requestType=UNKNOWN_METHOD;
    postContentLength=0;
    postContentTypeOk=false;
    *url='\0';
    *requestParams='\0';
    *requestCookies='\0';
    crlfEmptyLineFound=false;
    keepAlive=-1;
    zipSupport=false;
    isQueryStr=false;

    while (!crlfEmptyLineFound)
    {
      bufLineLen=0;
      *bufLine='\0';

      if (sslEnabled)
      {
        int r=BIO_gets(io, bufLine, BUFSIZE-1);

        switch(SSL_get_error(ssl,r))
        {
          case SSL_ERROR_NONE:
            bufLineLen=r;
            break;
          case SSL_ERROR_ZERO_RETURN:
            r=SSL_shutdown(ssl);
            if(!r)
            {
              shutdown(client,1);
              SSL_shutdown(ssl);
            }
            SSL_free(ssl);
            return ;      
        }
      }
      else
        bufLineLen=recvLine(client, bufLine, BUFSIZE);

      if (bufLineLen == 0 || exiting)
      {
        if (sslEnabled)
          { SSL_shutdown(ssl); SSL_free(ssl); }
        return ;
      }

      if ( bufLineLen <= 2) 
        crlfEmptyLineFound = (*bufLine=='\n') || (*bufLine=='\r' && *(bufLine+1)=='\n');        
      else
      {
        j = 0; while (isspace((int)(bufLine[j])) && j < (unsigned)bufLineLen) j++;

        if ( strncmp(bufLine+j, authStr, sizeof authStr - 1 ) == 0)
        {
          j+=sizeof authStr - 1;

          // decode login/passwd
          string pwdb64="";
          while ( j < (unsigned)bufLineLen && *(bufLine + j) != 0x0d && *(bufLine + j) != 0x0a)
            pwdb64+=*(bufLine + j++);;
          if (!authOK)
            authOK=isUserAllowed(pwdb64);
          continue;
        }
                 
        if (strncasecmp(bufLine+j, "Connection: ", 12) == 0)
        { 
          j+=12; 
          if (strstr(bufLine+j,"lose") != NULL) keepAlive=false;
          else if ((strstr(bufLine+j,"eep-") != NULL) && (strstr(bufLine+j+4,"live") != NULL)) keepAlive=true;
          continue;
        }

        if (strncasecmp(bufLine+j, "Accept-Encoding: ",17) == 0) { j+=17; if (strstr(bufLine+j,"gzip") != NULL) zipSupport=true; continue; }

        if (strncasecmp(bufLine+j, "Content-Type: application/x-www-form-urlencoded", 47) == 0) { postContentTypeOk=true; continue; }

        if (strncasecmp(bufLine+j, "Content-Length: ",16) == 0) { j+=16; postContentLength = atoi(bufLine+j); continue; }

        if (strncasecmp(bufLine+j, "Cookie: ",8) == 0) { j+=8; strcpy(requestCookies, bufLine+j); continue; }
        
        isQueryStr=false;
        if (strncmp(bufLine+j, "GET", 3) == 0)
          {  requestType=GET_METHOD; isQueryStr=true; j+=4; }
        
        if (strncmp(bufLine+j, "POST", 4) == 0)
          {  requestType=POST_METHOD; isQueryStr=true; j+=5; }

        if (isQueryStr)
        {
          while (isspace((int)(bufLine[j])) && j < bufLineLen) j++;

          i=0; while (!isspace((int)(bufLine[j])) && (i < BUFSIZE - 1) && (j < bufLineLen) && bufLine[j]!='?') url[i++] = bufLine[j++];
          url[i]='\0';

          if ((requestType == GET_METHOD) && (bufLine[j] == '?'))
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
      httpSend(client, (const void*) msg.c_str(), msg.length(), io);
      if (sslEnabled) { SSL_shutdown(ssl); SSL_free(ssl); }
      return ;
    }

    if ( requestType == UNKNOWN_METHOD )
    {
      std::string msg = getNotImplementedErrorMsg();
      httpSend(client, (const void*) msg.c_str(), msg.length(), io);
      if (sslEnabled) { SSL_shutdown(ssl); SSL_free(ssl); }
      return ;
    }

    if ( requestType == POST_METHOD )
      bufLineLen=recvLine(client, requestParams, BUFSIZE);

    char logBuffer[BUFSIZE];
    snprintf(logBuffer, BUFSIZE, "Request : url='%s'  reqType='%d'  param='%s'  requestCookies='%s'  (httpVers=%s keepAlive=%d zipSupport=%d)\n", url+1, requestType, requestParams, requestCookies, httpVers, keepAlive, zipSupport );
    NVJ_LOG->append(NVJ_DEBUG, logBuffer);

    // Process the query
    if (keepAlive==-1) 
      keepAlive = ( strncmp (httpVers,"1.1", 3) >= 0 );

    if ( (url[strlen(url) - 1] == '/') && (strlen(url)+12 < BUFSIZE) )
        strcpy (url + strlen(url) - 1, "/index.html");

    unsigned char *webpage = NULL;
    size_t webpageLen = 0;
    unsigned char *gzipWebPage=NULL;
    int sizeZip=0;
    bool fileFound=false, zippedFile=false;

#ifdef DEBUG_TRACES
    printf( "url: %s?%s\n", url+1, requestParams ); fflush(NULL);
#endif

    HttpRequest request(requestType, url+1, requestParams, requestCookies);
    const char *mime=get_mime_type(url+1); 
    string mimeStr; if (mime != NULL) mimeStr=mime;
    HttpResponse response(mimeStr);

    std::vector<WebRepository *>::const_iterator repo=webRepositories.begin();
    for( ; repo!=webRepositories.end() && !fileFound && !zippedFile;)
    {
      if (*repo == NULL) continue;

      fileFound = (*repo)->getFile(&request, &response);
      if (fileFound && response.getForwardedUrl() != "")
      {
        strncpy( url+1, response.getForwardedUrl().c_str(), BUFSIZE - 1);
        *(url + BUFSIZE - 1)='\0';
        response.forwardTo("");
        repo=webRepositories.begin(); fileFound=false;
      }
      else
         repo++;
    }

    if (!fileFound)
    {
      char bufLinestr[300]; snprintf(bufLinestr, 300, "Webserver: page not found %s",  url+1);
      NVJ_LOG->append(NVJ_WARNING,bufLinestr);

      std::string msg = getNotFoundErrorMsg();
      httpSend(client, (const void*) msg.c_str(), msg.length(), io);
      if (sslEnabled) { SSL_shutdown(ssl); SSL_free(ssl); }
      return ;
    }
    else
    {
      repo--;
      response.getContent(&webpage, &webpageLen, &zippedFile);
      
      if ( webpage == NULL || !webpageLen)
      {
        std::string msg = getNoContentErrorMsg();
        httpSend(client, (const void*) msg.c_str(), msg.length(), io);
        if (sslEnabled) { SSL_shutdown(ssl); SSL_free(ssl); }
        return;
      }
        
      if (zippedFile)
      {
        gzipWebPage = webpage;
        sizeZip = webpageLen;
      }
    }
    
    char bufLinestr[300]; snprintf(bufLinestr, 300, "Webserver: page found %s",  url+1);
    NVJ_LOG->append(NVJ_DEBUG,bufLinestr);

    if (!zipSupport && zippedFile)
    {
      // Need to uncompress
      if ((int)(webpageLen=gunzip( &webpage, gzipWebPage, sizeZip )) < 0)
      {
        NVJ_LOG->append(NVJ_ERROR, "Webserver: gunzip decompression failed !");
        std::string msg = getInternalServerErrorMsg();
        httpSend(client, (const void*) msg.c_str(), msg.length(), io);
        if (sslEnabled) { SSL_shutdown(ssl); SSL_free(ssl); }
        return ;
      }
    }

    // Need to compress
    if (!zippedFile && zipSupport && (webpageLen > 2048))
    {
      const char *mimetype=response.getMimeType().c_str();
      if (mimetype != NULL && (strncmp(mimetype,"application",11) == 0 || strncmp(mimetype,"text",4) == 0))
      {  
        if ((int)(sizeZip=gzip( &gzipWebPage, webpage, webpageLen )) < 0)
        {
          NVJ_LOG->append(NVJ_ERROR, "Webserver: gunzip compression failed !");
          std::string msg = getInternalServerErrorMsg();
          httpSend(client, (const void*) msg.c_str(), msg.length(), io);
          if (sslEnabled) { SSL_shutdown(ssl); SSL_free(ssl); }
          return ;
        }
        else
          if ((size_t)sizeZip>webpageLen)
          {
            sizeZip=0;
            free (gzipWebPage);
          }
      }
    }

    if (keepAlive && !(--nbFileKeepAlive)) keepAlive=false;

    if (sizeZip>0 && zipSupport)
    {  
      std::string header = getHttpHeader("200 OK", sizeZip, keepAlive, true, &response);
      httpSend(client, (const void*) header.c_str(), header.length(), io);
      httpSend(client, (const void*) gzipWebPage, sizeZip, io);
    }
    else
    {
      std::string header = getHttpHeader("200 OK", webpageLen, keepAlive, false, &response);
      httpSend(client, (const void*) header.c_str(), header.length(), io);
      httpSend(client, (const void*) webpage, webpageLen, io);
    }

    if (sizeZip>0 && !zippedFile) // cas compression = double desalloc
    {
      free (gzipWebPage);
      (*repo)->freeFile(webpage); 
      continue;
    }

    if (!zipSupport && zippedFile) // cas décompression = double desalloc
    {
      free (webpage);
      (*repo)->freeFile(gzipWebPage);
      continue;
    }

    (*repo)->freeFile(webpage); 

  }
  while (keepAlive && !exiting);

}

/***********************************************************************
* httpSend
***********************************************************************/

void WebServer::httpSend(int s, const void *buf, size_t len, BIO *io)
{
  if (sslEnabled)
  {
    while (BIO_write(io, buf, len) <= 0)
    {
      if(! BIO_should_retry(io))
      {
          NVJ_LOG->append(NVJ_WARNING, "WebServer: BIO_write failed !");
          return ;
      }
      // retry
    }

    BIO_flush(io);
  }
  else
    sendCompat (s, buf, len, 0);
}

/***********************************************************************
* fatalError:  Print out a system error and exit
* @param s - error message
***********************************************************************/

void WebServer::fatalError(const char *s)
{
  NVJ_LOG->append(NVJ_FATAL,s);
  perror(s);
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
  
  if (response != NULL)
  {
    std::vector<std::string>& cookies=response->getCookies();
    for (unsigned i=0; i < cookies.size(); i++)
      header+="Set-Cookie: " + cookies[i] + "\r\n";
  }
   
  if (strncmp(messageType, "401", 3) == 0)
    header+=std::string("WWW-Authenticate: Basic realm=\"Restricted area: please enter Login/Password\"\r\n");
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
      if ( listen(server_sock [ nbServerSock ], 5) >= 0 )
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
  pthread_mutex_lock( &clientsSockLifo_mutex );
  exiting=true;

  while (nbServerSock>0)
  {
    shutdown ( server_sock[ --nbServerSock ], 2 ) ;
    close (server_sock[ nbServerSock ]);
  }

  pthread_mutex_unlock( &clientsSockLifo_mutex );
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

/***********************************************************************
* startPoolThread: 
************************************************************************/
void *WebServer::startPoolThread(void *t)
{
  static_cast<WebServer *>(t)->poolThreadProcessing();
  pthread_exit(NULL);
  return NULL;
}  
  
void WebServer::poolThreadProcessing()
{
  pthread_mutex_lock( &clientsSockLifo_mutex );

  BIO *sbio;
  SSL *ssl=NULL;
  X509 *peer=NULL;
  bool authSSL=false;

  while( !clientsSockLifo.empty() || !exiting )
  {
    while( clientsSockLifo.empty() && !exiting)
      pthread_cond_wait( &clientsSockLifo_cond, &clientsSockLifo_mutex );

    if (exiting)  { pthread_mutex_unlock( &clientsSockLifo_mutex ); break; }

    // clientsSockLifo is not empty
    volatile int client=clientsSockLifo.top();
    clientsSockLifo.pop();

    pthread_mutex_unlock( &clientsSockLifo_mutex );
    
    if (sslEnabled)
    {
      sbio=BIO_new_socket(client, BIO_NOCLOSE);
      ssl=SSL_new(sslCtx);
      SSL_set_bio(ssl,sbio,sbio);

      if (SSL_accept(ssl) <= 0)
      { const char *sslmsg=ERR_reason_error_string(ERR_get_error());
        string msg="SSL accept error ";
        if (sslmsg != NULL) msg+=": "+string(sslmsg);
        NVJ_LOG->append(NVJ_DEBUG,msg);
      }

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
              updatePeerDnHistory(std::string(str));
            free (str);                           
            X509_free(peer);
          }
//        else printf ("SSL_get_verify_result(ssl) = %d\n", SSL_get_verify_result(ssl) );
        }
      }
      else
        authSSL=true;
    }

    if (authSSL)
      accept_request(client,ssl); 
    else
    {
      if (sslEnabled)
      { SSL_shutdown(ssl); SSL_free(ssl); }
      else 
        accept_request(client);
    }

    shutdown (client, SHUT_RDWR);      
    close(client);     
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
    create_thread( &newthread, WebServer::startPoolThread, static_cast<void *>(this) );
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
      status = poll( pfd, nbServerSock, 1000 );
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

      pthread_mutex_lock( &clientsSockLifo_mutex );

      updatePeerIpHistory(webClientAddr);
      if (client_sock == -1)
        NVJ_LOG->appendUniq(NVJ_ERROR, "WebServer : An error occurred when attempting to access the socket (accept == -1)");
      else
      {
        setSocketRcvTimeout(client_sock, 5);
        clientsSockLifo.push(client_sock);
      }

      pthread_cond_broadcast (& clientsSockLifo_cond);
      pthread_mutex_unlock( &clientsSockLifo_mutex );   
    }
  }


  while (exitedThread != threadsPoolSize)
  {
    pthread_cond_broadcast (& clientsSockLifo_cond);
    usleep(500);
  }

  // Exiting...
  free (pfd);

  if (sslEnabled)
    SSL_CTX_free(sslCtx);

  pthread_mutex_destroy(&clientsSockLifo_mutex);

#if defined(LINUX) || defined(__darwin__)
  if (isAuthPam())
    AuthPAM::stop();
#endif
 
}

/***********************************************************************
* base64_decode 
  thanks to  René Nyffenegger rene.nyffenegger@adp-gmbh.ch for his
  public implementation of this algorithm
*
************************************************************************/

std::string WebServer::base64_decode(const std::string& encoded_string)
{
  static const string base64_chars = 
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";
             
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

