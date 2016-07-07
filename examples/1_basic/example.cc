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
#include "libnavajo/libnavajo.hh"
#include "libnavajo/LogStdOutput.hh"

WebServer *webServer = NULL;

void exitFunction( int dummy )
{
   if (webServer != NULL) webServer->stopService();
}



class MyDynamicPage: public DynamicPage
{
  bool getPage(HttpRequest* request, HttpResponse *response)
  {
    // example using session's object
    int *cptExample=NULL;
    
    void *myAttribute = request->getSessionAttribute("myAttribute");
    if (myAttribute == NULL)
    {
      cptExample=(int*)malloc(sizeof(int));
      *cptExample=0;
      request->setSessionAttribute ( "myAttribute", (void*)cptExample );
    }
    else
      cptExample=(int*)request->getSessionAttribute("myAttribute");

    *cptExample=*cptExample+1;    
    //

    std::string content="<HTML><BODY>";
    std::string param;
    if (request->getParameter("param1", param))
    {
      //int pint=getValue<int>(param);
      content+="param1 has been set to "+param;
    }
    else
      content+="param1 hasn't been set";

    std::stringstream myAttributess; myAttributess << *cptExample;
    content+="<BR/>my session attribute myAttribute contains "+myAttributess.str();

    content+="</BODY></HTML>";
   
    return fromString(content, response);
  }
};


int main()
{
  // connect signals
  signal( SIGTERM, exitFunction );
  signal( SIGINT, exitFunction );
  
  NVJ_LOG->addLogOutput(new LogStdOutput);
  //NVJ_LOG->addLogOutput(new LogFile("/var/log/navajo.log"));

  webServer = new WebServer;

  webServer->setServerPort(8080);
//  webServer->setThreadsPoolSize(1);
  //uncomment to switch to https
  //webServer->setUseSSL(true, "mycert.pem");

  //uncomment to active X509 auth
  //webServer->setAuthPeerSSL(true, "cachain.pem");
  //webServer->addAuthPeerDN("/C=FR/O=CNRS/OU=UMR5821/CN=Thierry Descombes/emailAddress=thierry.descombes@lpsc.in2p3.fr");

  //webServer->addHostsAllowed(IpNetwork("134.158.40.0/21"));

  //uncomment to active login/passwd auth
  //webServer->addLoginPass("login","password");


  // Fill the web repository with local files, statically compiled files or dynamic files
  PrecompiledRepository thePrecompRepo("") ;
  webServer->addRepository(&thePrecompRepo);

  LocalRepository myLocalRepo("/docs", "../../docs/html"); // if doxygen documentation is generated in "docs" folder, we will browse it at http://localhost:8080/docs/index.html
  webServer->addRepository(&myLocalRepo);

  MyDynamicPage page1;
  DynamicRepository myRepo;
  myRepo.add("/dynpage.html",&page1); // unusual html extension for a dynamic page !
  webServer->addRepository(&myRepo);

  webServer->startService();

  // Your Processing here !
  //...
  webServer->wait();
  
  LogRecorder::freeInstance();
  return 0;
}
