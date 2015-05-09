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

#include <stack>
#include <string>
#include <map>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "navajo/LogRecorder.hh"
#include "navajo/IpAddress.hh"
#include "navajo/WebRepository.hh"
#include "navajo/thread.h"


class WebServer
{
    SSL_CTX *sslCtx;
    int s_server_session_id_context;
    static char *certpass;
    void initialize_ctx(const char *certfile, const char *cafile, const char *password);
    static int password_cb(char *buf, int num, int rwflag, void *userdata);

    bool isUserAllowed(const string &logpassb64);
    bool isAuthorizedDN(const std::string str);

    void httpSend(int s, const void *buf, size_t len, BIO *b);

    size_t recvLine(int client, char *bufLine, size_t);
    void accept_request(int client, SSL *ssl=NULL);
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

    std::stack<int> clientsSockLifo;
    pthread_cond_t clientsSockLifo_cond;
    pthread_mutex_t clientsSockLifo_mutex;

    
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
    static std::string base64_decode(const std::string& encoded_string);
    
  public:
    WebServer();

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
      LOG->append(_INFO_, "WebServer: Service is starting !");
      create_thread( &threadWebServer, WebServer::startThread, this );
    };
    
    /**
    * stopService: the webserver stops
    */ 
    void stopService() 
    {
      LOG->append(_INFO_, "WebServer: Service is stopping !");
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

