//********************************************************
/**
 * @file  example.cc 
 *
 * @brief libnavajo example.
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

WebServer *webServer = NULL;

void exitFunction( int dummy )
{
   if (webServer != NULL) webServer->stopService();
}



class MyDynamicPage: public DynamicPage
{
  bool getPage(HttpRequest* request, HttpResponse *response)
  {
    return fromString(getPageString(request), response);
  }
  
  string getPageString(HttpRequest* request)
  {
    // example using session's object
    int *cptExample=(int*)malloc(sizeof(int));
    *cptExample=0;
    
    void *myAttribute = request->getSessionAttribute("myAttribute");
    if (myAttribute == NULL)
      request->setSessionAttribute ( "myAttribute", (void*)cptExample );
    else
      cptExample=(int*)request->getSessionAttribute("myAttribute");

    *cptExample=*cptExample+1;    
    //

    string response="<HTML><BODY>";
    string param;
    if (request->getParameter("param1", param))
    {
      //int pint=getValue<int>(param);
      response+="param1 has been set to "+param;
    }
    else
      response+="param1 hasn't been set";

    stringstream myAttributess; myAttributess << *cptExample;
    response+="<BR>my session attribute myAttribute contains "+myAttributess.str();

    response+="<HTML><BODY>";
   
    return response;
  }
};


int main()
{
  // connect signals
  signal( SIGTERM, exitFunction );
  signal( SIGINT, exitFunction );
  
  LOG->addLogOutput(new LogStdOutput);
  //LOG->addLogOutput(new LogFile("/var/log/navajo.log"));

  webServer = new WebServer;

  webServer->listenTo(8080);
  webServer->setThreadsPoolSize(1);
  //uncomment to switch to https
  //webServer->setUseSSL(true, "serverCert.pem", "caFile.crt");
   webServer->setUseSSL(true, "/etc/znets/lpsc-test.in2p3.fr.pem2", "/etc/znets/ca-chain.crt");

  //uncomment to active X509 auth
//  webServer->setAuthPeerSSL();
  webServer->addAuthPeerDN("/C=FR/O=CNRS/OU=UMR5821/CN=Thierry Descombes/emailAddress=thierry.descombes@lpsc.in2p3.fr");

  //uncomment to active login/passwd auth
  //webServer->addLoginPass("login","password");

  //uncomment to use Pam authentification
  //webServer->usePamAuth("/etc/pam.d/login");


  // Fill the web repository with local files, statically compiled files or dynamic files
  PrecompiledRepository thePrecompRepo;
  webServer->addRepository(&thePrecompRepo);

  LocalRepository myLocalRepo;
  myLocalRepo.addDirectory("toto/tata", "./exampleRepository/");
  // myLocalRepo.addDirectory(WEB_LOCATION, DIRECTORY);
  webServer->addRepository(&myLocalRepo);

  MyDynamicPage page1;
  DynamicRepository myRepo;
  myRepo.add("dynpage.html",&page1); // unusual html extension for a dynamic page !
  webServer->addRepository(&myRepo);

  webServer->startService();

  // Your Processing here !
  //...
  webServer->wait();

  return 0;
}
