//********************************************************
/**
 * @file  WebServer.hh 
 *
 * @brief WebServer
 *
 * @author T.Descombes (thierry.descombes@gmail.com)
 *
 * @version 1	
 * @date 19/02/15
 */
//********************************************************
 
#ifndef WEBSERVER_HH_
#define WEBSERVER_HH_


#include <stdio.h>
#include <sys/types.h>
#ifdef LINUX
#include <netinet/in.h>
#include <arpa/inet.h>
#define SO_NOSIGPIPE    0x0800
#endif

#include <queue>
#include <string>
#include <map>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "navajo/LogRecorder.hh"
#include "navajo/IpAddress.hh"
#include "navajo/WebRepository.hh"

#include "navajo/thread.h"
#include "navajo/nvj_gzip.h"

class WebSocket;
class WebServer
{
    SSL_CTX *sslCtx;
    int s_server_session_id_context;
    static char *certpass;
    
    inline static void freeClientSockData(ClientSockData *c)
    {
      if (c == NULL) return;
    printf("freeClientSockData (%d)!\n",c->socketId); fflush(NULL);
      closeSocket(c);
      if (c->peerDN != NULL) { delete c->peerDN; c->peerDN=NULL; }
      free(c);
      c=NULL;
    };
    
    std::queue<ClientSockData *> clientsQueue;
    pthread_cond_t clientsQueue_cond;
    pthread_mutex_t clientsQueue_mutex;

    void initialize_ctx(const char *certfile, const char *cafile, const char *password);
    static int password_cb(char *buf, int num, int rwflag, void *userdata);

    bool isUserAllowed(const string &logpassb64, string &username);
    bool isAuthorizedDN(const std::string str);

    void httpSend(ClientSockData *client, const void *buf, size_t len);

    size_t recvLine(int client, char *bufLine, size_t);
    bool accept_request(ClientSockData* client);
    void fatalError(const char *);
    int setSocketRcvTimeout(int connectSocket, int seconds);
    static std::string getHttpHeader(const char *messageType, const size_t len=0, const bool keepAlive=true, const bool zipped=false, HttpResponse* response=NULL);
    static const char* get_mime_type(const char *name);
    u_short init();

    static std::string getNoContentErrorMsg();
    static std::string getBadRequestErrorMsg();
    static std::string getNotFoundErrorMsg();
    static std::string getInternalServerErrorMsg();
    static std::string getNotImplementedErrorMsg();

    void initPoolThreads();
    static void *startPoolThread(void *);
    void poolThreadProcessing();

    bool httpdAuth;
    
    volatile bool exiting;
    volatile size_t exitedThread;
    volatile int server_sock [ 3 ];
    volatile size_t nbServerSock;
    
    const static char authStr[];

    std::map<std::string,time_t> usersAuthHistory;
    pthread_mutex_t usersAuthHistory_mutex;
    std::map<IpAddress,time_t> peerIpHistory;
    std::map<std::string,time_t> peerDnHistory;
    pthread_mutex_t peerDnHistory_mutex;
    void updatePeerIpHistory(IpAddress&);
    void updatePeerDnHistory(std::string);
    static int verify_callback(int preverify_ok, X509_STORE_CTX *ctx);
    static const int verify_depth;

    pthread_t threadWebServer;
    static void* startThread(void* );
    void threadProcessing();
    void exit();
    
    static std::string webServerName;
    bool disableIpV4, disableIpV6;
    ushort tcpPort;
    size_t threadsPoolSize;
    string device;
    
    bool sslEnabled;
    std::string sslCertFile, sslCaFile, sslCertPwd;
    std::vector<std::string> authLoginPwdList;
    bool authPeerSsl;
    std::vector<std::string> authDnList;
    bool authPam;
    std::string pamService;
    std::vector<std::string> authPamUsersList;
    inline bool isAuthPam() { return authPam; };
    std::vector<IpNetwork> hostsAllowed;
    std::vector<WebRepository *> webRepositories;
    static inline bool is_base64(unsigned char c)
      { return (isalnum(c) || (c == '+') || (c == '/')); };
    static const string base64_chars;
    static std::string base64_decode(const std::string& encoded_string);
    static std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len);
    static void closeSocket(ClientSockData* client);
    std::map<std::string, WebSocket *> webSocketEndPoints;
    static std::string SHA1_encode(const string& input);
    static const string webSocketMagicString;
    static string generateWebSocketServerKey(string webSocketKey);
    static string getHttpWebSocketHeader(const char *messageType, const char* webSocketClientKey, const bool webSocketDeflate);
    void listenWebSocket(WebSocket *websocket, HttpRequest* request);
    void startWebSocketListener(WebSocket *websocket, HttpRequest* request);
    std::list<int> webSocketClientList;
    pthread_mutex_t webSocketClientList_mutex;
    typedef struct
    {
      WebServer* webserver;
      WebSocket* websocket;
      HttpRequest* request;
    } WebSocketParams;
    inline static void* startThreadListenWebSocket(void* t)
    {
      WebSocketParams *p=static_cast<WebSocketParams *>(t);
      p->webserver->listenWebSocket(p->websocket, p->request);
      free(p);
      pthread_exit(NULL);
      return NULL;
    };
    
  public:
    WebServer();
    
    static void webSocketSend(HttpRequest* request, const string &message);

    /**
    * Set the web server name in the http header
    * @param name: the new name
    */ 
    inline void setWebServerName(const std::string& name) { webServerName = name; }
    
    /**
    * Set the size of the listener thread pool. 
    * @param nbThread: the number of thread available (Default value: 5)
    */ 
    inline void setThreadsPoolSize(const size_t nbThread) { threadsPoolSize = nbThread; };

    /**
    * Set the tcp port to listen. 
    * @param p: the port number, from 1 to 65535 (Default value: 8080)
    */     
    inline void listenTo(const ushort p) { tcpPort = p; };

    /**
    * Set the device to use (work on linux only). 
    * @param d: the device name
    */   
    inline void setDevice(const char* d) { device = d; };
    
    /**
    * Enabled or disabled HTTPS
    * @param ssl: boolean. SSL connections are used if ssl is true.
    * @param certFile: the path to cert file
    * @param certPwd: optional certificat password
    */   
    inline void setUseSSL(bool ssl, const char* certFile = "", const char* certPwd = "")
        { sslEnabled = ssl; sslCertFile = certFile; sslCertPwd = certPwd; };

    /**
    * Enabled or disabled X509 authentification
    * @param a: boolean. X509 authentification is required if a is true.
    * @param caFile: the path to cachain file
    */
    inline void setAuthPeerSSL(const bool a = true, const char* caFile = "") { authPeerSsl = a; sslCaFile = caFile; };

    /**
    * Restricted X509 authentification to a DN user list. Add this given DN.
    * @param dn: user certificate DN
    */ 
    inline void addAuthPeerDN(const char* dn) { authDnList.push_back(string(dn)); };

    /**
    * Enabled http authentification for a given login/password list
    * @param login: user login
    * @param pass : user password
    */ 
    inline void addLoginPass(const char* login, const char* pass) { authLoginPwdList.push_back(string(login)+':'+string(pass)); };

    /**
    * Use PAM authentification (if supported)
    * @param service : pam configuration file
    */ 
    inline void usePamAuth(const char* service="/etc/pam.d/login") { authPam = true; pamService = service; };

    /**
    * Restricts PAM authentification to a list of allowed users. Add this user.
    * @param user : an allowed pam user login
    */ 
    inline void addAuthPamUser(const char* user) { authPamUsersList.push_back(string(user)); };
    
    /**
    * Add a web repository (containing web pages)
    * @param repo : a pointer to a WebRepository instance
    */  
    void addRepository(WebRepository* repo) { webRepositories.push_back(repo); };

    /**
    * Add a websocket
    * @param endpoint : websocket endpoint
    * @param websocket : WebSocket instance
    */  
    void addWebSocket(const string endPoint, WebSocket* websocket) { webSocketEndPoints[endPoint]=websocket; };
    
    /**
    * IpV4 hosts only
    */  
    inline void listenIpV4only() { disableIpV6=true; };
    
    /**
    * IpV6 hosts only
    */  
    inline void listenIpV6only() { disableIpV4=true; };

    /**
    * set network access restriction to webserver. 
    * @param ipnet: an IpNetwork of allowed web client to add
    */   
    inline void addHostsAllowed(const IpNetwork &ipnet) { hostsAllowed.push_back(ipnet); };    
    
    /**
    * Get the list of http client peer IP address. 
    * @return a map of every IP address and last connection to the webserver
    */ 
    inline std::map<IpAddress,time_t>& getPeerIpHistory() { return peerIpHistory; };

    /**
    * Get the list of http client DN (work with X509 authentification)
    * @return a map of every DN and last connection to the webserver
    */ 
    inline std::map<std::string,time_t>& getPeerDnHistory() { return peerDnHistory; };
 
    /**
    * startService: the webserver starts
    */ 
    void startService()
    {
      NVJ_LOG->append(NVJ_INFO, "WebServer: Service is starting !");
      create_thread( &threadWebServer, WebServer::startThread, this );
    };
    
    /**
    * stopService: the webserver stops
    */ 
    void stopService() 
    {
      NVJ_LOG->append(NVJ_INFO, "WebServer: Service is stopping !");
      exit();
      threadWebServer=0;
    };
    
    /**
    * wait until the webserver is stopped
    */ 
    void wait()
    {
      wait_for_thread(threadWebServer);
    };

    /**
    * is the webserver runnning ?
    */ 
    bool isRunning()
    {
      return threadWebServer != 0;
    }
};

#endif

