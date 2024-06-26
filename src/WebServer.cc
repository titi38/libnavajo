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


#include <sys/stat.h>

#include <pthread.h>
#include <ctype.h>
#include <signal.h>

#include <string.h>
#include <sys/types.h>
#include <errno.h> 
#include <stdlib.h>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>

#include <openssl/evp.h>
#include <openssl/sha.h>

#include <libnavajo/HttpRequest.hh>

#include "libnavajo/WebServer.hh"
#include "libnavajo/nvjSocket.h"
#include "libnavajo/nvjGzip.h"
#include "libnavajo/htonll.h"
#include "libnavajo/WebSocket.hh"

#include "MPFDParser/Parser.h"

#define DEFAULT_HTTP_SERVER_SOCKET_TIMEOUT 3
#define DEFAULT_HTTP_PORT 8080
#define LOGHIST_EXPIRATION_DELAY 600
#define BUFSIZE 32768
#define KEEPALIVE_MAX_NB_QUERY 25

const char WebServer::authStr[]="Authorization: Basic ";
const char WebServer::authBearerStr[]="Authorization: Bearer ";
const int WebServer::verify_depth=512;
char *WebServer::certpass=NULL;
std::string WebServer::webServerName;
pthread_mutex_t IpAddress::resolvIP_mutex = PTHREAD_MUTEX_INITIALIZER;
HttpSession::HttpSessionsContainerMap HttpSession::sessions;
pthread_mutex_t HttpSession::sessions_mutex=PTHREAD_MUTEX_INITIALIZER;
const std::string WebServer::base64_chars =
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";
const std::string WebServer::webSocketMagicString="258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

std::map<unsigned, const char*> HttpResponse::httpReturnCodes;
time_t HttpSession::lastExpirationSearchTime=0;
time_t HttpSession::sessionLifeTime=20*60;

#ifndef MSG_NOSIGNAL
  #define MSG_NOSIGNAL 0
#endif


/*********************************************************************/

WebServer::WebServer(): sslCtx(NULL), s_server_session_id_context(1),
                        tokDecodeCallback(NULL), authBearTokDecExpirationCb(NULL), authBearTokDecScopesCb(NULL),
                        authBearerEnabled(false),
                        httpdAuth(false), exiting(false), exitedThread(0),
                        nbServerSock(0), disableIpV4(false), disableIpV6(false),
                        socketTimeoutInSecond(DEFAULT_HTTP_SERVER_SOCKET_TIMEOUT), tcpPort(DEFAULT_HTTP_PORT),
                        threadsPoolSize(64), mutipartMaxCollectedDataLength( 20*1024 ),
                        sslEnabled(false), authPeerSsl(false)
{

  webServerName=std::string("Server: libNavajo/")+std::string(LIBNAVAJO_SOFTWARE_VERSION);
  mutipartTempDirForFileUpload="/tmp";

  pthread_mutex_init(&clientsQueue_mutex, NULL);
  pthread_cond_init(&clientsQueue_cond, NULL);

  pthread_mutex_init(&peerDnHistory_mutex, NULL);
  pthread_mutex_init(&usersAuthHistory_mutex, NULL);
  pthread_mutex_init(&tokensAuthHistory_mutex, NULL);
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
     NVJ_LOG->append(NVJ_DEBUG,std::string ("WebServer: Connection from IP: ") + ip.str());
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
    NVJ_LOG->append(NVJ_DEBUG,"WebServer: Authorized DN: "+dn);

  pthread_mutex_unlock( &peerDnHistory_mutex );
}

/*********************************************************************/
/**
* Http login authentification
* @param name: the login/password string in base64 format  
* @param name: set to the decoded login name  
* @return true if user is allowed
*/ 
bool WebServer::isUserAllowed(const std::string &pwdb64, std::string& login)
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
  std::string loginPwd=base64_decode(pwdb64.c_str());
  size_t loginPwdSep=loginPwd.find(':');
  if (loginPwdSep==std::string::npos)
  {
    pthread_mutex_unlock( &usersAuthHistory_mutex );
    return false;
  }

  login=loginPwd.substr(0,loginPwdSep);
  std::string pwd=loginPwd.substr(loginPwdSep+1);

  std::vector<std::string> httpAuthLoginPwd=authLoginPwdList;
  if (httpAuthLoginPwd.size())
  {
    std::string logPass=login+':'+pwd;
    for ( std::vector<std::string>::iterator it=httpAuthLoginPwd.begin(); it != httpAuthLoginPwd.end(); it++ )
      if ( logPass == *it )
        authOK=true;
  }
  
  if (authOK)
  {
    NVJ_LOG->append(NVJ_INFO,"WebServer: Authentification passed for user '"+login+"'");
    if (i == usersAuthHistory.end())
      usersAuthHistory[pwdb64]=t;
  }
  else
    NVJ_LOG->append(NVJ_DEBUG,"WebServer: Authentification failed for user '"+login+"'");

  pthread_mutex_unlock( &usersAuthHistory_mutex );
  return authOK;
}

/**
* Http Bearer token authentication
* @param tokb64: the token string in base64 format
* @param resourceUrl: the token string in base64 format
* @param respHeader: headers to add in HTTP response in case of failed authentication, on tail of WWW-Authenticate attribute
* @return true if token is allowed
*/
bool WebServer::isTokenAllowed(const std::string &tokb64,
                               const std::string &resourceUrl,
                               std::string &respHeader)
{
  std::string logAuth = "WebServer: Authentication passed for token '"+tokb64+"'";
  NvjLogSeverity logAuthLvl = NVJ_DEBUG;
  time_t t = time ( NULL );
  struct tm * timeinfo;
  bool isTokenExpired = true;
  bool authOK = false;
  time_t expiration = 0;

  timeinfo = localtime ( &t );
  t = mktime ( timeinfo );

  pthread_mutex_lock( &tokensAuthHistory_mutex );
  std::map<std::string,time_t>::iterator i = tokensAuthHistory.find (tokb64);

  if (i != tokensAuthHistory.end())
  {
    NVJ_LOG->append(NVJ_DEBUG, "WebServer: token already authenticated");

    /* get current timeinfo and compare to the one previously stored */
    isTokenExpired = t > i->second;

    if (isTokenExpired)
    {
      /* Remove token from the map to avoid infinite grow of it */
      tokensAuthHistory.erase(tokb64);
      NVJ_LOG->append(NVJ_DEBUG, "WebServer: removing outdated token from cache '"+tokb64+"'");
    }

    pthread_mutex_unlock( &tokensAuthHistory_mutex );
    return !isTokenExpired;
  }

  // It's a new token !

  std::string tokDecoded;

  // Use callback configured to decode token
  if (tokDecodeCallback(tokb64, tokDecodeSecret, tokDecoded))
  {
    logAuth = "WebServer: Authentication failed for token '"+tokb64+"'";
    respHeader = "realm=\"" + authBearerRealm;
    respHeader += "\",error=\"invalid_token\", error_description=\"invalid signature\"";
    goto end;
  }

  // retrieve expiration date
  expiration = authBearTokDecExpirationCb(tokDecoded);

  if (!expiration)
  {
    logAuth = "WebServer: Authentication failed, expiration date not found for token '"+tokb64+"'";
    respHeader = "realm=\"" + authBearerRealm;
    respHeader += "\",error=\"invalid_token\", error_description=\"no expiration in token\"";
    goto end;
  }

  if (expiration < t)
  {
    logAuth = "WebServer: Authentication failed, validity expired for token '"+tokb64+"'";
    respHeader = "realm=\"" + authBearerRealm;
    respHeader += "\",error=\"invalid_token\", error_description=\"token expired\"";
    goto end;
  }

  // check for extra attribute if any callback was set to that purpose
  if (authBearTokDecScopesCb)
  {
    std::string errDescr;

    if (authBearTokDecScopesCb(tokDecoded, resourceUrl, errDescr))
    {
      logAuth = "WebServer: Authentication failed, invalid scope for token '"+tokb64+"'";
      respHeader = "realm=\"" + authBearerRealm;
      respHeader += "\",error=\"insufficient_scope\",error_description=\"";
      respHeader += errDescr +"\"";
      goto end;
    }
  }

  // All checks passed successfully, store the token to speed up processing of next request
  // proposing same token
  authOK = true;
  logAuthLvl = NVJ_INFO;
  tokensAuthHistory[tokb64] = expiration;

end:
  pthread_mutex_unlock( &tokensAuthHistory_mutex );
  NVJ_LOG->append(logAuthLvl, logAuth);

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

/**********************************************************************/
/**
* trim from start, thanks to https://stackoverflow.com/a/217605
*/
static inline std::string &ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
            std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}

/**********************************************************************/
/**
* trim from end, thanks to https://stackoverflow.com/a/217605
*/
static inline std::string &rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
            std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}

/**********************************************************************/
/**
* trim from both ends, thanks to https://stackoverflow.com/a/217605
*/
static inline std::string &trim(std::string &s) {
    return ltrim(rtrim(s));
}

/**********************************************************************/
/**
* fill extra http headers Map
* @param c: raw string containing a header line made of "Header: Value"
*/
static void addExtraHeader(const char* l, HttpRequestHeadersMap& m)
{
  std::stringstream ss(l);
  std::string header;
  std::string val;
  if (std::getline(ss, header, ':') && std::getline(ss, val, ':'))
  {
    m[header] = trim(val);
  }
};

/***********************************************************************
* accept_request:  Process a request
* @param c - the socket connected to the client
* \return true if the socket must to close
***********************************************************************/

bool WebServer::accept_request(ClientSockData* client, bool /*authSSL*/)
{
  char bufLine[BUFSIZE];
  HttpRequestMethod requestMethod;
  size_t requestContentLength=0;
  bool urlencodedForm=false;

  std::vector<uint8_t> payload;
  char mimeType[64]="\0";

  char *urlBuffer=NULL;
  char *mutipartContent=NULL;
  size_t nbFileKeepAlive=KEEPALIVE_MAX_NB_QUERY;
  MPFD::Parser *mutipartContentParser=NULL;
  char *requestParams=NULL;
  char *requestCookies=NULL;
  char *requestOrigin=NULL;
  HttpRequestHeadersMap requestExtraHeaders;
  char *webSocketClientKey=NULL;
  bool websocket=false;
  int webSocketVersion=-1;
  std::string username;
  int bufLineLen=0;

  unsigned i=0, j=0;
  
  bool authOK = authLoginPwdList.size() == 0;
  char httpVers[4]="";
  bool keepAlive=false;
  bool closing=false;
  bool isQueryStr=false;
  std::string authRespHeader;

  if (authBearerEnabled)
  {
    authOK = false;
    authRespHeader = "realm=\"Restricted area: please provide valid token\"";
  }

  do
  {
    // Initialisation /////////
    requestMethod=UNKNOWN_METHOD;
    requestContentLength=0;
    urlencodedForm=false;
    username="";
    keepAlive=false;
    closing=false;
    isQueryStr=false;
    
    if (urlBuffer != NULL) { free (urlBuffer); urlBuffer=NULL; };
    if (requestParams != NULL) { free (requestParams);  requestParams=NULL; };
    if (requestCookies != NULL) { free (requestCookies); requestCookies=NULL; };
    if (requestOrigin != NULL) { free (requestOrigin); requestOrigin=NULL; };
    if (webSocketClientKey != NULL) { free (webSocketClientKey); webSocketClientKey=NULL; };
    if (mutipartContent != NULL) { free (mutipartContent); mutipartContent=NULL; };
    if (mutipartContentParser != NULL) { delete mutipartContentParser; mutipartContentParser=NULL; };
    
    websocket=false;
    webSocketVersion=-1;

    //////////////////////////

    while (true)
    {
      bufLineLen=0;
      *bufLine='\0';

      if (sslEnabled)
      {
        int r=BIO_gets(client->bio, bufLine, BUFSIZE-1);

        switch(SSL_get_error(client->ssl,r))
        {
          case SSL_ERROR_NONE:
            if ( (r==0) || (r==-1) )
              continue;
            bufLineLen=r;
            break;
          case SSL_ERROR_ZERO_RETURN:
            NVJ_LOG->append(NVJ_DEBUG, "WebServer::accept_request - BIO_gets() failed with SSL_ERROR_ZERO_RETURN - 1");
            goto FREE_RETURN_TRUE;     
        }
      }
      else
        bufLineLen=recvLine(client->socketId, bufLine, BUFSIZE-1);

      if (bufLineLen == 0 || exiting)
        goto FREE_RETURN_TRUE;

      if ( bufLineLen <= 2)
      {
        // only CRLF found (empty line) -> decoding is finished !
        if ((*bufLine=='\n') || (*bufLine=='\r' && *(bufLine+1)=='\n'))
          break;
      }
      else
      {
        if (bufLineLen == BUFSIZE-1) *(bufLine+bufLineLen)='\0';
        else *(bufLine+bufLineLen-2)='\0';
        j = 0; while (isspace((int)(bufLine[j])) && j < (unsigned)bufLineLen) j++;


        // decode login/passwd
        if ( strncmp(bufLine+j, authStr, sizeof authStr - 1 ) == 0)
        {
          j+=sizeof authStr - 1;

          std::string pwdb64="";
          while ( j < (unsigned)bufLineLen && *(bufLine + j) != 0x0d && *(bufLine + j) != 0x0a)
            pwdb64+=*(bufLine + j++);;
          if (!authOK)
            authOK=isUserAllowed(pwdb64, username);
          continue;
        }
        
        // decode HTTP headers      
        if (strncasecmp(bufLine+j, "Connection: ", 12) == 0)
        { 
          j+=12; 
          if (strstr(bufLine+j,"pgrade") != NULL)
            websocket=true;
          else
          {
            if ( strstr (bufLine + j, "lose") != NULL )
              closing = false;
            else if (( strstr (bufLine + j, "eep-") != NULL ) && ( strstr (bufLine + j + 4, "live") != NULL ))
              keepAlive = true;
          }
          continue;
        }

        if (strncasecmp(bufLine+j, "Accept-Encoding: ",17) == 0) 
        { 
          j+=17;
          if (strstr(bufLine+j,"gzip") != NULL)
            client->compression=GZIP;
          continue;
        }

        if (strncasecmp(bufLine+j, "Content-Type: ", 14) == 0)
        {
          j+=14;
          char *start = bufLine + j , *end = NULL;
          size_t length = 0;
          if ( ( end = index( start, ';' ) ) != NULL )
            length = end - start;
          else
            length = strlen( start );
          if ( length >= 63 ) length = 63;
          strncpy( mimeType, start, length );
          mimeType[ length ] = '\0';

          if ( strncasecmp( mimeType, "application/x-www-form-urlencoded", 33 ) == 0 )
            urlencodedForm = true;
          else
            if ( strncasecmp( mimeType, "multipart/form-data", 19 ) == 0 )
            {
              mutipartContent = ( char * ) malloc( ( strlen( bufLine + j ) + 1 ) * sizeof( char ) );
              strcpy( mutipartContent, bufLine + j );
            }
          continue;
        }
  
        if (strncasecmp(bufLine+j, "Content-Length: ",16) == 0)
          { j+=16; requestContentLength = atoi(bufLine+j); continue; }

        if (strncasecmp(bufLine+j, "Cookie: ",8) == 0) 
        { 
          j+=8; 
          requestCookies = (char*) malloc ( (strlen(bufLine+j)+1) * sizeof(char) );
          strcpy(requestCookies, bufLine+j); 
          continue; 
        }

        if (strncasecmp(bufLine+j, "Origin: ",8) == 0) 
        { 
          j+=8;
          requestOrigin = (char*) malloc ( (strlen(bufLine+j)+1) * sizeof(char) );
          strcpy(requestOrigin, bufLine+j);
          continue;
        }
        
        if (strncasecmp(bufLine+j, "Sec-WebSocket-Key: ", 19) == 0) 
        { 
          j+=19; 
          webSocketClientKey = (char*) malloc ( (strlen(bufLine+j)+1) * sizeof(char) );
          strcpy(webSocketClientKey, bufLine+j);
          continue;
        }

        if (strncasecmp(bufLine+j, "Sec-WebSocket-Extensions: ", 26) == 0)
          { j+=26; if (strstr(bufLine+j, "permessage-deflate")  != NULL) client->compression=ZLIB; continue; }
        
        if (strncasecmp(bufLine+j, "Sec-WebSocket-Version: ", 23) == 0)
          { j+=23; webSocketVersion = atoi(bufLine+j); continue; }

        addExtraHeader(bufLine+j, requestExtraHeaders);
        isQueryStr=false;
        if (strncmp(bufLine+j, "GET", 3) == 0)
        {  requestMethod=GET_METHOD; isQueryStr=true; j+=4; }
        else
          if (strncmp(bufLine+j, "POST", 4) == 0)
          {  requestMethod=POST_METHOD; isQueryStr=true; j+=5; }
          else
            if (strncmp(bufLine+j, "PUT", 3) == 0)
            {  requestMethod=PUT_METHOD; isQueryStr=true; j+=4; }
            else
            if (strncmp(bufLine+j, "DELETE", 6) == 0)
              {  requestMethod=DELETE_METHOD; isQueryStr=true; j+=7; }
              else
              if (strncmp(bufLine+j, "UPDATE", 6) == 0)
                {  requestMethod=UPDATE_METHOD; isQueryStr=true; j+=7; }
                else
                if (strncmp(bufLine+j, "PATCH", 5) == 0)
                  {  requestMethod=PATCH_METHOD; isQueryStr=true; j+=6; }
                else
                  if (strncmp(bufLine+j, "OPTIONS", 7) == 0)
                    { requestMethod=OPTIONS_METHOD; isQueryStr=true; j+=7; }

        if (isQueryStr)
        {
          while (isspace((int)(bufLine[j])) && j < (unsigned)bufLineLen) j++;

          // Decode URL
          urlBuffer = (char*)malloc ( (strlen(bufLine+j)+1) * sizeof(char) );
          i=0; 
          while (!isspace((int)(bufLine[j])) && (i < BUFSIZE - 1) && (j < (unsigned)bufLineLen) && bufLine[j]!='?')
            if ( !i && ( bufLine[j] == '/' ) ) // remove first '/'
              j++;
            else
              urlBuffer[i++] = bufLine[j++];
          urlBuffer[i]='\0';

          // Decode GET Parameters
          if ( !urlencodedForm && (bufLine[j] == '?') )
          {
            i=0; j++;
            requestParams = (char*) malloc ( BUFSIZE * sizeof(char) );
            while (!isspace((int)(bufLine[j])) && (i < BUFSIZE - 1) && (j < (unsigned)bufLineLen))
              requestParams[i++] = bufLine[j++];
            requestParams[i]='\0'; 
          }

          while (isspace((int)(bufLine[j])) && j < (unsigned)bufLineLen) j++;
          if (strncmp(bufLine+j, "HTTP/", 5) == 0)
          {
            strncpy (httpVers, bufLine+j+5, 3);
            *(httpVers+3)='\0';
            j+=8;
            // HTTP/1.1 default behavior is to support keepAlive
            keepAlive = strncmp (httpVers,"1.1", 3) >= 0 ;
          }
        }

        //  authorization through bearer token, RFC 6750
        if ( strncmp(bufLine+j, authBearerStr, sizeof authBearerStr - 1 ) == 0)
        {
            j+=sizeof authStr;

            std::string tokb64="";
            while ( j < (unsigned)bufLineLen && *(bufLine + j) != 0x0d && *(bufLine + j) != 0x0a)
                tokb64+=*(bufLine + j++);
            if (authBearerEnabled)
                authOK=isTokenAllowed(tokb64, urlBuffer, authRespHeader);
            continue;
        }
      }
    }

    if (!authOK)
    {
      const char *abh = authRespHeader.empty()? NULL: authRespHeader.c_str();
      std::string msg = getHttpHeader( "401 Authorization Required", 0, false, abh);
      httpSend(client, (const void*) msg.c_str(), msg.length());
      goto FREE_RETURN_TRUE;
    }

    if ( requestMethod == UNKNOWN_METHOD )
    {
      std::string msg = getNotImplementedErrorMsg();
      httpSend(client, (const void*) msg.c_str(), msg.length());
      goto FREE_RETURN_TRUE;
    }

    // update URL to load the default index.html page
    if ( (*urlBuffer == '\0' || *(urlBuffer + strlen(urlBuffer) - 1) == '/' ) )
    {
      urlBuffer = (char*) realloc( urlBuffer, strlen(urlBuffer) + 10 + 1 );
      strcpy (urlBuffer + strlen(urlBuffer), "index.html");
    }

    // Interpret '%' character
    std::string urlString(urlBuffer);
    size_t start = 0, end = 0;

    while ((end = urlString.find_first_of('%', start)) != std::string::npos)
    {
      size_t len=urlString.length()-end-1;
      if ( urlString[end]=='%' && len>=1 )
      {
        if ( urlString[end+1]=='%' )
          urlString=urlString.erase(end+1,1);
        else
        {
          if (len>=2)
          {
            unsigned int specar;
            std::string hexChar=urlString.substr(end+1,2);
            std::stringstream ss; ss << std::hex << hexChar.c_str();
            ss >> specar;
            urlString[end] = (char)specar;
            urlString=urlString.erase(end+1,2);
          }
        }
      }
      start=end+1;
    }
    strcpy (urlBuffer, urlString.c_str());

    #ifdef DEBUG_TRACES
    char logBuffer[BUFSIZE];
    snprintf(logBuffer, BUFSIZE, "Request : url='%s'  reqType='%d'  param='%s'  requestCookies='%s'  (httpVers=%s keepAlive=%d zipSupport=%d closing=%d)\n", urlBuffer, requestMethod, requestParams, requestCookies, httpVers, keepAlive, client->compression, closing );
    NVJ_LOG->append(NVJ_DEBUG, logBuffer);
    #endif

    if (mutipartContent != NULL)
    {
      try
      {
        // Initialize MPFDParser
        mutipartContentParser = new MPFD::Parser();
        mutipartContentParser->SetUploadedFilesStorage(MPFD::Parser::StoreUploadedFilesInFilesystem);
        mutipartContentParser->SetTempDirForFileUpload( mutipartTempDirForFileUpload );
        mutipartContentParser->SetMaxCollectedDataLength( mutipartMaxCollectedDataLength );
        mutipartContentParser->SetContentType( mutipartContent );
      }
      catch (const MPFD::Exception& e)
      {
        NVJ_LOG->append(NVJ_DEBUG, "WebServer::accept_request -  MPFD::Exception: "+ e.GetError() );
        delete mutipartContentParser;
        mutipartContentParser = NULL;
      }
    }

    // Read request content
    if ( requestContentLength )
    {
      size_t datalen = 0;

      while ( datalen < requestContentLength )
      {
        char buffer[BUFSIZE];
        size_t requestedLength = ( requestContentLength-datalen > BUFSIZE) ? BUFSIZE : requestContentLength-datalen;

        if (sslEnabled)
        {
          int r=BIO_gets(client->bio, buffer, requestedLength+1); //BUFSIZE);

          switch(SSL_get_error(client->ssl,r))
          {
            case SSL_ERROR_NONE:
              if ( (r==0) || (r==-1) )
                continue;
              bufLineLen=r;
              break;
            case SSL_ERROR_ZERO_RETURN:
              NVJ_LOG->append(NVJ_DEBUG, "WebServer::accept_request - BIO_gets() failed with SSL_ERROR_ZERO_RETURN - 2");
              goto FREE_RETURN_TRUE;
          }
        }
        else
          bufLineLen=recvLine(client->socketId, buffer, requestedLength);

        if ( urlencodedForm )
        {
          if (requestParams == NULL)
            requestParams = (char*) malloc ( (bufLineLen+1) * sizeof(char) );
          else
            requestParams = (char*) realloc(requestParams, (datalen + bufLineLen + 1));

          if (requestParams == NULL)
          {
            NVJ_LOG->append( NVJ_DEBUG, "WebServer::accept_request -  memory allocation failed" );
            break;
          }
          memcpy(requestParams + datalen, buffer, bufLineLen);
          *(requestParams + datalen + bufLineLen)='\0';
        }
        else
        {
          if ( mutipartContentParser != NULL && bufLineLen )
            try
            {
              mutipartContentParser->AcceptSomeData( buffer, bufLineLen );
            }
            catch ( const MPFD::Exception& e )
            {
              NVJ_LOG->append( NVJ_DEBUG, "WebServer::accept_request -  MPFD::Exception: " + e.GetError() );
              break;
            }
          else
          {
            if (!payload.size())
            {
              try
              {
                payload.reserve(requestContentLength);
              }
              catch (std::bad_alloc &e)
              {
                NVJ_LOG->append( NVJ_DEBUG, "WebServer::accept_request -  payload.reserve() failed with exception: " + std::string(e.what()) );
                break;
              }
            }

            payload.resize( datalen+bufLineLen );
            memcpy(&payload[datalen], buffer, bufLineLen);
          }
        }

        datalen+=bufLineLen;
      };
    }
      
    /* *************************
    /  * processing WebSockets *
    /  *************************/
    
    if (websocket)
    {
      //search endpoint
      std::map<std::string,WebSocket*>::iterator it;

      it = webSocketEndPoints.find(urlBuffer);
      if (it != webSocketEndPoints.end()) // FOUND
      {
        WebSocket* webSocket=it->second;
        if(!webSocket->isUsingCompression())
          client->compression = NONE;

        std::string header = getHttpWebSocketHeader("101 Switching Protocols", webSocketClientKey, client->compression == ZLIB);

        if (! httpSend(client, (const void*) header.c_str(), header.length()) )
          goto FREE_RETURN_TRUE;

        HttpRequest* request=new HttpRequest(requestMethod, urlBuffer, requestParams, requestCookies, requestExtraHeaders, requestOrigin, username, client, mimeType, &payload, mutipartContentParser);

        webSocket->newConnectionRequest(request);

        if (urlBuffer != NULL) free (urlBuffer);
        if (requestParams != NULL) free (requestParams);
        if (requestCookies != NULL) free (requestCookies);
        if (requestOrigin != NULL) free (requestOrigin);
        if (webSocketClientKey != NULL) free (webSocketClientKey);
        if (mutipartContent != NULL) free (mutipartContent);
        if (mutipartContentParser != NULL) delete mutipartContentParser;
        return false;
      }
      else
      {
        char bufLinestr[300]; snprintf(bufLinestr, 300, "Webserver: Websocket not found %s",  urlBuffer);
        NVJ_LOG->append(NVJ_WARNING,bufLinestr);

        std::string msg = getNotFoundErrorMsg();
        httpSend(client, (const void*) msg.c_str(), msg.length());

        goto FREE_RETURN_TRUE;
      }
    }
    
    /* ********************* */

    bool fileFound=false;
    unsigned char *webpage = NULL;
    size_t webpageLen = 0;
    unsigned char *gzipWebPage=NULL;
    int sizeZip=0;
    bool zippedFile=false;

    HttpRequest request(requestMethod, urlBuffer, requestParams, requestCookies, requestExtraHeaders, requestOrigin, username, client, mimeType, &payload, mutipartContentParser);

    const char *mime=get_mime_type(urlBuffer); 
    std::string mimeStr; if (mime != NULL) mimeStr=mime;
    HttpResponse response(mimeStr);

    std::vector<WebRepository *>::const_iterator repo=webRepositories.begin();
    for( ; repo!=webRepositories.end() && !fileFound && !zippedFile;)
    {
      if (*repo == NULL) continue;

      fileFound = (*repo)->getFile(&request, &response);
      if (fileFound && response.getForwardedUrl() != "")
      {
        urlBuffer = (char*) realloc( urlBuffer, (response.getForwardedUrl().size() + 1) * sizeof(char) ); 
        strcpy( urlBuffer, response.getForwardedUrl().c_str() );
        request.setUrl(urlBuffer);
        response.forwardTo("");
        repo=webRepositories.begin(); fileFound=false;
      }
      else
         repo++;
    }
    
    if (!fileFound)
    {
      char bufLinestr[300]; snprintf(bufLinestr, 300, "Webserver: page not found %s",  urlBuffer);
      NVJ_LOG->append(NVJ_DEBUG,bufLinestr);

      std::string msg = getNotFoundErrorMsg();
      httpSend(client, (const void*) msg.c_str(), msg.length());

      goto FREE_RETURN_TRUE;
    }
    else
    {
      repo--;
      response.getContent(&webpage, &webpageLen, &zippedFile);
      
      if ( webpage == NULL || !webpageLen)
      {
        std::string msg =  getHttpHeader( response.getHttpReturnCodeStr().c_str(), 0, false ); //getNoContentErrorMsg();
        httpSend(client, (const void*) msg.c_str(), msg.length());
	      if (webpage != NULL)
          (*repo)->freeFile(webpage);
        goto FREE_RETURN_TRUE;
      }
        
      if (zippedFile)
      {
        gzipWebPage = webpage;
        sizeZip = webpageLen;
      }
    }
    #ifdef DEBUG_TRACES
    char bufLinestr[300]; snprintf(bufLinestr, 300, "Webserver: page found %s",  urlBuffer);
    NVJ_LOG->append(NVJ_DEBUG,bufLinestr);
    #endif

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
          (*repo)->freeFile(gzipWebPage);
          goto FREE_RETURN_TRUE;
        }
      }
      catch(...)
      {
          NVJ_LOG->append(NVJ_ERROR, "Webserver: nvj_gunzip raised an exception");
          std::string msg = getInternalServerErrorMsg();
          httpSend(client, (const void*) msg.c_str(), msg.length());
          (*repo)->freeFile(gzipWebPage);
          goto FREE_RETURN_TRUE;
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
            (*repo)->freeFile(webpage);
            goto FREE_RETURN_TRUE;
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
              (*repo)->freeFile(webpage);
              goto FREE_RETURN_TRUE;
          }
      }
    }

    if (keepAlive && (--nbFileKeepAlive<=0)) closing=true;

    if (sizeZip>0 && (client->compression == GZIP))
    {
      std::string header = getHttpHeader(response.getHttpReturnCodeStr().c_str(), sizeZip, keepAlive, NULL, true, &response);
      if ( !httpSend(client, (const void*) header.c_str(), header.length())
        || !httpSend(client, (const void*) gzipWebPage, sizeZip) )
      {
        NVJ_LOG->append(NVJ_ERROR, std::string("Webserver: httpSend failed sending the zipped page: ") + urlBuffer + std::string("- err: ") + strerror(errno));
        closing=true;
      }
    }
    else
    {
      std::string header = getHttpHeader(response.getHttpReturnCodeStr().c_str(), webpageLen, keepAlive, NULL, false, &response);
      if ( !httpSend(client, (const void*) header.c_str(), header.length())
        || !httpSend(client, (const void*) webpage, webpageLen) )
      {
        NVJ_LOG->append(NVJ_ERROR, std::string("Webserver: httpSend failed sending the page: ") + urlBuffer + std::string("- err: ") + strerror(errno));
        closing=true;
      }
    }

    if (sizeZip>0 && !zippedFile) // cas compression = double desalloc
    {
      free (gzipWebPage);
      (*repo)->freeFile(webpage); 
    }
    else
      if ((client->compression == NONE) && zippedFile) // cas décompression = double desalloc
      {
        free (webpage);
        (*repo)->freeFile(gzipWebPage);
      }
      else
        (*repo)->freeFile(webpage); 
  }
  while (keepAlive && !closing && !exiting);

  /////////////////
  FREE_RETURN_TRUE:

  if (urlBuffer != NULL) free (urlBuffer);
  if (requestParams != NULL) free (requestParams);
  if (requestCookies != NULL) free (requestCookies);
  if (requestOrigin != NULL) free (requestOrigin);
  if (webSocketClientKey != NULL) free (webSocketClientKey);
  if (mutipartContent != NULL) free (mutipartContent);
  if (mutipartContentParser != NULL) delete mutipartContentParser;


  return true;
}

/***********************************************************************
* httpSend - send data from the socket
* @param client - the ClientSockData to use
* @param buf - the data
* @param len - the data length
* \return false if it's failed
***********************************************************************/

bool WebServer::httpSend(ClientSockData *client, const void *buf, size_t len)
{
//  pthread_mutex_lock( &client->client_mutex );

  if ( !client->socketId )
  {
    //pthread_mutex_unlock( &client->client_mutex );
    return false;
  }

  bool useSSL = client->bio != NULL;
  size_t totalSent=0;
  int sent=0;
  unsigned char* buffer_left = (unsigned char*) buf;

  fd_set writeset;
  FD_ZERO(&writeset);
  FD_SET(client->socketId, &writeset);
  struct timeval tv;
  tv.tv_sec = 10;
  tv.tv_usec = 0;
  int result;

  do
  {
    if ( useSSL )
      sent = BIO_write(client->bio, buffer_left, len - totalSent);
    else
      sent = sendCompat (client->socketId, buffer_left, len - totalSent, MSG_NOSIGNAL);

    if ( sent <= 0 )
    {
      if ( sent < 0 )
      {
        if ( errno == EAGAIN || errno == EWOULDBLOCK
          || ( useSSL && BIO_should_retry(client->bio) ) )
        {
          NVJ_LOG->append(NVJ_ERROR, std::string("Webserver: send buffer full, retrying in 1 second"));
          sleep (1);

          /* retry to send data a second time before returning a failure to caller */
          if ( useSSL )
            sent = BIO_write(client->bio, buffer_left, len - totalSent);
          else
          {
            result = select(client->socketId + 1, NULL, &writeset, NULL, &tv);

            if ( (result <= 0) || (!FD_ISSET(client->socketId, &writeset)) )
              return false;

            sent = sendCompat (client->socketId, buffer_left, len - totalSent, MSG_NOSIGNAL);
          }

          if ( sent < 0 )
          {
            /* this is the second time it failed, no need to be more stubborn */
            return false;
          }
          else
          {
            /* retry succeeded, don't forget to update counters and buffer left to send */
            totalSent+=(size_t)sent;
            buffer_left += sent;
          }
        }
        else if (( errno == EINTR )
              || ( useSSL && !BIO_should_retry(client->bio) ) )
        {
          // write failed
          //pthread_mutex_unlock (&client->client_mutex);
          return false;
        }
      }
      else
      {
        usleep (50);
        continue;
      }
    }
    else
    {
      totalSent+=(size_t)sent;
      buffer_left += sent;
    }
  }
  while (sent >= 0 && totalSent != len);

  if ( useSSL )
    BIO_flush(client->bio);

//  pthread_mutex_unlock( &client->client_mutex );

  return totalSent == len;
}

/***********************************************************************
* fatalError:  Print out a system error and exit
* @param s - error message
***********************************************************************/

void WebServer::fatalError(const char *s)
{
  NVJ_LOG->append(NVJ_FATAL,std::string(s)+": "+std::string(strerror(errno)));
  ::exit(1);
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
  if (strcmp(extLowerCase, ".svg") == 0 || strcmp(extLowerCase, ".svgz") == 0) return "image/svg+xml";
  if (strcmp(extLowerCase, ".cache") == 0) return "text/cache-manifest";
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

std::string WebServer::getHttpHeader(const char *messageType,
                                     const size_t len,
                                     const bool keepAlive,
                                     const char *authBearerAdditionalHeaders,
                                     const bool zipped,
                                     HttpResponse* response)
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
  {
    if (authBearerAdditionalHeaders)
    {
      header+=std::string("WWW-Authenticate: Bearer ");
      header+=authBearerAdditionalHeaders;
      header+="\r\n";
    }
    else
      header+=std::string("WWW-Authenticate: Basic realm=\"Restricted area: please enter Login/Password\"\r\n");
  }

  if (response != NULL)
  {
    if ( response->isCORS() )
    {
      header += "Access-Control-Allow-Origin: " + response->getCORSdomain() + "\r\nAccess-Control-Allow-Credentials: ";
      if ( response->isCORSwithCredentials() )
        header += "true\r\n";
      else header+="false\r\n";
    } 

    header += response->getSpecificHeaders();

    std::vector<std::string>& cookies=response->getCookies();
    for (unsigned i=0; i < cookies.size(); i++)
      header+="Set-Cookie: " + cookies[i] + "\r\n";
  }
   
  header+="Accept-Ranges: bytes\r\n";

  if (keepAlive)
    header+="Connection: Keep-Alive\r\n";
  else
    header+="Connection: close\r\n";

  std::string mimetype="text/html";
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

    setSocketReuseAddr(server_sock [ nbServerSock ]);

    if (device.length())
    {
#ifndef LINUX
      NVJ_LOG->append(NVJ_WARNING, "WebServer: HttpdDevice parameter will be ignored on your system");
#else
      setSocketBindToDevice(server_sock [ nbServerSock ], device.c_str());
#endif
    }

    if ( rp->ai_family == PF_INET && disableIpV4) continue;

    if ( rp->ai_family == PF_INET6 )
    {
      if (disableIpV6) continue;
#if defined( IPV6_V6ONLY )
      
      //Disable IPv4 mapped addresses.
      setSocketIp6Only( server_sock [ nbServerSock ] );
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

  for (std::map<std::string, WebSocket *>::iterator it=webSocketEndPoints.begin(); it!=webSocketEndPoints.end(); ++it)
    it->second->removeAllClients();

  while (nbServerSock>0)
  {
    shutdown ( server_sock[ --nbServerSock ], 2 ) ;
    close (server_sock[ nbServerSock ]);
  }
  pthread_mutex_unlock( &clientsQueue_mutex );

  if (sslEnabled)
  {
    SSL_CTX_free(sslCtx);
    sslCtx = NULL;
  }
}

/***********************************************************************
* password_cb
************************************************************************/

int WebServer::password_cb(char *buf, int num, int /*rwflag*/, void */*userdata*/)
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
    NVJ_LOG->append(NVJ_DEBUG,buftmp);
  }

  /*
  * At this point, err contains the last verification error. We can use
  * it for something special
  */
  if (!preverify_ok && (err == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT))
  {
   X509_NAME_oneline(X509_get_issuer_name(err_cert), buf, 256);  
   char buftmp[300]; snprintf(buftmp, 300, "X509_verify_cert error: issuer= %s", buf);
     NVJ_LOG->append(NVJ_DEBUG,buftmp);
  }

  return 1;
}


/***********************************************************************
* initialize_ctx: 
************************************************************************/

void WebServer::initialize_ctx(const char *certfile, const char *cafile, const char *password)
{
  /* Global system initialization*/
  if (!sslCtx)
  {
    SSL_library_init();
    SSL_load_error_strings();
  }

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
  X509 *peer=NULL;
  bool authSSL=false;

  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGPIPE);
  sigprocmask(SIG_BLOCK, &set, NULL);

  while( !exiting )
  {
    pthread_mutex_lock( &clientsQueue_mutex );

    while( clientsQueue.empty() && !exiting)
      pthread_cond_wait( &clientsQueue_cond, &clientsQueue_mutex );

    if (exiting)  { pthread_mutex_unlock( &clientsQueue_mutex ); break; }

    // clientsQueue is not empty
    ClientSockData* client = clientsQueue.front();
    clientsQueue.pop();
    client->bio = NULL;
    client->ssl = NULL;

    if (sslEnabled)
    {
      BIO *bio = NULL;

      if ( (bio=BIO_new_socket(client->socketId, BIO_NOCLOSE)) == NULL )
      {
        NVJ_LOG->append(NVJ_DEBUG,"BIO_new_socket failed !");
        freeClientSockData(client);
        pthread_mutex_unlock( &clientsQueue_mutex );
        continue;
      }

      if ( (client->ssl=SSL_new(sslCtx)) == NULL )
      {
        NVJ_LOG->append(NVJ_DEBUG,"SSL_new failed !");
        BIO_free(bio);
        freeClientSockData(client);
        pthread_mutex_unlock( &clientsQueue_mutex );
        continue;
      }

      SSL_set_bio(client->ssl, bio, bio);

      ERR_clear_error();

      //SIGSEGV
      if (SSL_accept(client->ssl) <= 0)
      {
        const char *sslmsg=ERR_reason_error_string(ERR_get_error());
        std::string msg="SSL accept error ";
        if (sslmsg != NULL) msg+=": "+std::string(sslmsg);
        NVJ_LOG->append(NVJ_DEBUG,msg);
        freeClientSockData(client);
        pthread_mutex_unlock( &clientsQueue_mutex );
        continue;
      }

      if ( authPeerSsl )
      {
        if ( (peer = SSL_get_peer_certificate(client->ssl)) != NULL )
        {
          if (SSL_get_verify_result(client->ssl) == X509_V_OK)
          {
            // The client sent a certificate which verified OK
            char *str = X509_NAME_oneline(X509_get_subject_name(peer), 0, 0);                  

            if ((authSSL=isAuthorizedDN(str)) == true)
            {
              authSSL=true;
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

      //----------------------------------------------------------------------------------------------------------------

      BIO *ssl_bio = NULL;

      client->bio=BIO_new(BIO_f_buffer());
      ssl_bio=BIO_new(BIO_f_ssl());
      BIO_set_ssl(ssl_bio,client->ssl,BIO_CLOSE);
      BIO_push(client->bio,ssl_bio);

      if (authPeerSsl && !authSSL)
      {
        std::string msg = getHttpHeader( "403 Forbidden Client Certificate Required", 0, false);
        httpSend(client, (const void*) msg.c_str(), msg.length());
        freeClientSockData(client);
        pthread_mutex_unlock( &clientsQueue_mutex );
        continue;
      }
    }

    pthread_mutex_unlock( &clientsQueue_mutex );

    if (accept_request(client, authSSL))
      freeClientSockData (client);
  }
  pthread_mutex_lock( &clientsQueue_mutex );
  exitedThread++;
  pthread_mutex_unlock( &clientsQueue_mutex );
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

  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGPIPE);
  sigprocmask(SIG_BLOCK, &set, NULL);

  ushort port=init();

  initPoolThreads();
  httpdAuth = authLoginPwdList.size() ;

  char buf[300]; snprintf(buf, 300, "WebServer : Listen on port %d", port);
  NVJ_LOG->append(NVJ_DEBUG,buf);

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
      status = poll( pfd, nbServerSock, 500 );
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
        if (socketTimeoutInSecond)
          if (!setSocketSndRcvTimeout(client_sock, socketTimeoutInSecond, 0))
            NVJ_LOG->appendUniq(NVJ_ERROR, std::string("WebServer : setSocketSndRcvTimeout error - ") + strerror(errno) );
        if (!setSocketNoSigpipe(client_sock))
          NVJ_LOG->appendUniq(NVJ_ERROR, std::string("WebServer : setSocketNoSigpipe error - ") + strerror(errno) );

        ClientSockData* client=(ClientSockData*)malloc(sizeof(ClientSockData));
        client->socketId=client_sock;
        client->ip=webClientAddr;
        client->compression=NONE;
        client->ssl=NULL;
        client->bio=NULL;
        client->peerDN=NULL;
        //pthread_mutex_init ( &client->client_mutex, NULL );

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

  pthread_mutex_destroy(&clientsQueue_mutex);
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
  }
  shutdown (client->socketId, SHUT_RDWR);      
  close(client->socketId);
  client->socketId = 0;
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
std::string WebServer::SHA1_encode(const std::string& input)
{
    // Buffer pour stocker le résultat du hash
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int lengthOfHash = 0;

    // Contexte pour le hashing
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    
    if(context == nullptr) {
      NVJ_LOG->append(NVJ_FATAL, "WebServer::SHA1_encode - Failed to create EVP_MD_CTX");
    }

    // Initialisation du contexte pour SHA1
    if(!EVP_DigestInit_ex(context, EVP_sha1(), nullptr)) {
        EVP_MD_CTX_free(context);
        NVJ_LOG->append(NVJ_FATAL, "WebServer::SHA1_encode - Failed to initialize EVP digest context");
    }

    // Mise à jour du contexte avec les données
    if(!EVP_DigestUpdate(context, input.data(), input.size())) {
        EVP_MD_CTX_free(context);
        NVJ_LOG->append(NVJ_FATAL, "WebServer::SHA1_encode - Failed to update digest");
    }

    // Finalisation du hash
    if(!EVP_DigestFinal_ex(context, hash, &lengthOfHash)) {
        EVP_MD_CTX_free(context);
        NVJ_LOG->append(NVJ_FATAL, "WebServer::SHA1_encode - Failed to finalize digest");
    }

    // Nettoyage
    EVP_MD_CTX_free(context);

    // Conversion du résultat du hash en string hexadécimal
    std::string result;
    char buf[3];
    for(unsigned int i = 0; i < lengthOfHash; i++) {
        snprintf(buf, sizeof(buf), "%02x", hash[i]);
        result.append(buf);
    }

    return result;
}

/***********************************************************************
* generateWebSocketServerKey: Generate the websocket server key
* @param webSocketKey - the websocket client key.
* \return the websocket server key
************************************************************************/
std::string WebServer::generateWebSocketServerKey(std::string webSocketKey)
{
  std::string sha1Key=SHA1_encode(webSocketKey+webSocketMagicString);
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

