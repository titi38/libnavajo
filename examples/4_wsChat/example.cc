//********************************************************
/**
 * @file  example.cc 
 *
 * @brief libnavajo example code.
 *
 * @author T.Descombes (descombes@lpsc.in2p3.fr)
 *
 * @version 1	
 * @date 27/01/15
 */
//********************************************************

#include <signal.h> 
#include <string.h>
#include <pwd.h>
#include "libnavajo/libnavajo.hh"
#include "libnavajo/AuthPAM.hh"
#include "libnavajo/LogStdOutput.hh"
#include "libnavajo/WebSocket.hh"

WebServer *webServer = NULL;

void exitFunction( int dummy )
{
   if (webServer != NULL) webServer->stopService();
}

/***********************************************************************/

bool isValidSession(HttpRequest* request)
{
  void *username = request->getSessionAttribute("username");
  bool *connect = (bool*) request->getSessionAttribute("wschat");

  return username != NULL && connect != NULL && !(*connect);
}

/***********************************************************************/

void setSessionIsConnected(HttpRequest* request, const bool b)
{
  bool *connect = (bool*) request->getSessionAttribute("wschat");
  if (connect != NULL) *connect=b;
}

/***********************************************************************/

bool checkMessage(HttpRequest* request, const string msg)
{
  char *username = (char *)request->getSessionAttribute("username");
  return msg.length() > strlen(username)
      && ( strncmp(msg.c_str(), username, strlen(username)) == 0 );
}

/***********************************************************************/

class MyDynamicRepository : public DynamicRepository
{
    class Connect: public DynamicPage
    {        
      bool getPage(HttpRequest* request, HttpResponse *response)
      {
        string login, password;
        // User libnavajo/libnavajo is allowed and all PAM logins !
        if (request->getParameter("login", login) && request->getParameter("pass", password)
            && ( (login == "libnavajo" && password == "libnavajo")
              || AuthPAM::authentificate(login.c_str(), password.c_str(), "/etc/pam.d/login")) )
        {
          char *username = (char*)malloc((login.length()+1)*sizeof(char));
          strcpy(username, login.c_str());
          request->setSessionAttribute ( "username", (void*)username );
          bool *connect = (bool*)malloc(sizeof(bool));
          *connect=false;
          request->setSessionAttribute ( "wschat", (void*)connect );
          
          return fromString("authOK", response);
        }
        return fromString("authBAD", response);
      } 
    } connect;

   class Disconnect: public DynamicPage
    {        
      bool getPage(HttpRequest* request, HttpResponse *response)
      {
        request->removeSession();
        return noContent(response);
      } 
    } disconnect;
    
  public:
    MyDynamicRepository() : DynamicRepository()
    {
      add("connect.txt",&connect);
      add("disconnect.txt",&disconnect);
    }
} myDynRepo;

/***********************************************************************/

class MyWebSocket : public WebSocket
{
  bool onOpening(HttpRequest* request)
  {
    printf ("New Websocket (host '%s' - socketId=%d)\n", request->getPeerIpAddress().str().c_str(), request->getClientSockData()->socketId);
    if ( ! isValidSession(request) )
      return false;
    setSessionIsConnected(request, true);
    return true;
  }

  void onClosing(HttpRequest* request)
  {
    printf ("Closing Websocket (host '%s' - socketId=%d)\n", request->getPeerIpAddress().str().c_str(), request->getClientSockData()->socketId);
    setSessionIsConnected(request, false);
  }

  void onTextMessage(HttpRequest* request, const string &message, const bool fin)
  {
    printf ("Message: '%s' received from host '%s'\n", message.c_str(), request->getPeerIpAddress().str().c_str());
    //check username
    if (checkMessage(request, message))
      sendBroadcastTextMessage(message);
    else
      sendCloseCtrlFrame(request, "Not allowed message format");
  };
  void onBinaryMessage(HttpRequest* request, const unsigned char* message, size_t len, const bool fin)
  { };
} myWebSocket;

/***********************************************************************/

int main()
{
  // connect signals
  signal( SIGTERM, exitFunction );
  signal( SIGINT, exitFunction );
  
  NVJ_LOG->addLogOutput(new LogStdOutput);
  //NVJ_LOG->addLogOutput(new LogFile("/var/log/navajo.log"));
  NVJ_LOG->setDebugMode();
  AuthPAM::start(); 
  webServer = new WebServer;

  webServer->listenTo(8080);
//  webServer->setThreadsPoolSize(1);
  //uncomment to switch to https
  //webServer->setUseSSL(true, "serverCert.pem", "MyPwd");

  //uncomment to active X509 auth
  //webServer->setAuthPeerSSL(true, "cachain.pem");
  //webServer->addAuthPeerDN("/C=FR/O=CNRS/OU=UMR5821/CN=Thierry Descombes/emailAddress=thierry.descombes@lpsc.in2p3.fr");

  //webServer->addHostsAllowed(IpNetwork(string("134.158.40.0/21")));

  //uncomment to active login/passwd auth
  //webServer->addLoginPass("login","password");

  //uncomment to use Pam authentification
  //webServer->usePamAuth("/etc/pam.d/login");

  // Fill the web repository with local files, statically compiled files or dynamic files
  PrecompiledRepository thePrecompRepo("") ;
  webServer->addRepository(&thePrecompRepo);
  webServer->addRepository(&myDynRepo);
  
  webServer->addWebSocket("wschat", &myWebSocket);

  webServer->startService();

  // Your Processing here !
  //...
  webServer->wait();
  AuthPAM::stop(); 
  LogRecorder::freeInstance();
  return 0;
}
