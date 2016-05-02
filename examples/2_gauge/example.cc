//********************************************************
/**
 * @file  example2.cc 
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

/***********************************************************************/

  unsigned long cpu_work=0, cpu_total=0;
  
  int getCpuLoad(void)
  {
    #ifndef LINUX
    srand (time(NULL));
    return 40 + rand() % 40;
    #else
    char buf[1024];
    static FILE     *proc_stat_fd;
    int result = -1;
    unsigned long work=0, total=0;

    if ((proc_stat_fd = fopen("/proc/stat", "r")) == NULL)
      return -1;

    while ((fgets(buf, sizeof(buf), proc_stat_fd)) != NULL)
    {
      if (!strncmp("cpu ", buf, 4))
      {
        unsigned long  user=0, nice=0, sys=0, idle=0, iowait=0, irq=0, softirq=0;
        int n = sscanf(buf+5, "%lu %lu %lu %lu %lu %lu %lu", &user, &nice, &sys, &idle, &iowait, &irq, &softirq);
        if (n<5) return -1;
        work=user+nice+sys;
        total=work+idle;
        if (n>=5) total+=iowait;
        if (n>=6) total+=irq;
        if (n==7) total+=softirq;
      }
    }
    fclose (proc_stat_fd);

    if (cpu_work || cpu_total)
      result = 100 * ( (float)(work - cpu_work) / (float)(total - cpu_total));

    cpu_work = work; cpu_total = total;
    return result;
    #endif
  }

/***********************************************************************/

class MyDynamicRepository : public DynamicRepository
{
    class MyDynamicPage : public DynamicPage
    {
      protected:
        bool isValidSession(HttpRequest* request)
        {
          void *myAttribute = request->getSessionAttribute("username");          
          return myAttribute != NULL;
        }
    };
    
    class Auth: public MyDynamicPage
    {
      bool getPage(HttpRequest* request, HttpResponse *response)
      {
	std::string login, password;
        // User libnavajo/libnavajo is allowed
        if (request->getParameter("login", login) && request->getParameter("pass", password)
            && (login == "libnavajo" && password == "libnavajo"))
        {
          char *username = (char*)malloc((login.length()+1)*sizeof(char));
          strcpy(username, login.c_str());
          request->setSessionAttribute ( "username", (void*)username );
          return fromString("authOK", response);
        }
        return fromString("authBAD", response);
      } 
    } auth;
    
    class CpuValue: public MyDynamicPage
    {
      bool getPage(HttpRequest* request, HttpResponse *response)
      {
        if (!isValidSession(request))
          return fromString("ERR", response);;
	std::ostringstream ss;
        ss << getCpuLoad();
        return fromString(ss.str(), response);
      }
    } cpuValue;

    class Controller: public MyDynamicPage
    {
      bool getPage(HttpRequest* request, HttpResponse *response)
      {
	std::string param;

        if (request->getParameter("pageId", param) && param == "LOGIN" && isValidSession(request))
        {
          response->forwardTo("gauge.html"); 
          return true;
        } 

        if (request->hasParameter("disconnect")) // Button disconnect
          request->removeSession();

        if (!isValidSession(request))
          response->forwardTo("login.html");
        else
          response->forwardTo("gauge.html");       
        
        return true;
      }

    } controller;
     
  public:
    MyDynamicRepository() : DynamicRepository()
    {
      add("cpuvalue.txt",&cpuValue);
      add("index.html",&controller);
      add("auth.txt",&auth);
    }
};

/***********************************************************************/

int main()
{
  // connect signals
  signal( SIGTERM, exitFunction );
  signal( SIGINT, exitFunction );
  
  NVJ_LOG->addLogOutput(new LogStdOutput);
  webServer = new WebServer;
  //webServer->setUseSSL(true, "../myCert.pem");
  LocalRepository myLocalRepo("", "./html");
  webServer->addRepository(&myLocalRepo);

  MyDynamicRepository myRepo;
  webServer->addRepository(&myRepo);

  webServer->startService();

  webServer->wait();
  
  LogRecorder::freeInstance();
  return 0;
}


