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
#include "navajo/libnavajo.hh"
#include "navajo/LogStdOutput.hh"
#include "navajo/WebSocket.hh"

WebServer *webServer = NULL;

void exitFunction( int dummy )
{
   if (webServer != NULL) webServer->stopService();
}

class MyWebSocket : public WebSocket
{
  void onMessage(HttpRequest* request, const string &message) 
  {
    printf ("Message: '%s' received from host '%s'\n", message.c_str(), request->getPeerIpAddress().str().c_str());
    sendMessage(request, "The message has been received !");
  };
} myWebSocket;


int main()
{
  // connect signals
  signal( SIGTERM, exitFunction );
  signal( SIGINT, exitFunction );
  
  NVJ_LOG->addLogOutput(new LogStdOutput);
  //NVJ_LOG->addLogOutput(new LogFile("/var/log/navajo.log"));

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

  webServer->addWebSocket("echo", &myWebSocket);

  webServer->startService();

  // Your Processing here !
  //...
  webServer->wait();
  
  LogRecorder::freeInstance();
  return 0;
}
