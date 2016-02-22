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

#define UPLOAD_DIR "./upload"

WebServer *webServer = NULL;

LocalRepository *myUploadRepo = NULL;

void exitFunction( int dummy )
{
   if (webServer != NULL) webServer->stopService();
}

/***********************************************************************/

class MyDynamicRepository : public DynamicRepository
{

    class Uploader: public DynamicPage
    {
      bool getPage(HttpRequest* request, HttpResponse *response)
      {
        if (!request->isMultipartContent())
          return false;

        MPFD::Parser *parser=request->getMPFDparser();

        std::map<std::string,MPFD::Field *> fields=parser->GetFieldsMap();
        std::map<std::string,MPFD::Field *>::iterator it;
        for (it=fields.begin();it!=fields.end();it++) 
        {
          if (fields[it->first]->GetType()==MPFD::Field::TextType)
            NVJ_LOG->append(NVJ_INFO, "Got text field: [" + it->first + "], value: [" + fields[it->first]->GetTextTypeContent() + "]");
          else
          {
            NVJ_LOG->append(NVJ_INFO, "Got file field: [" + it->first + "] Filename:[" + fields[it->first]->GetFileName() + "] TempFilename:["  + fields[it->first]->GetTempFileName() + "]\n");

            // Copy files to upload directory
            std::ifstream  src( fields[it->first]->GetTempFileName(), std::ios::binary);
            std::ofstream  dst( string(UPLOAD_DIR)+'/'+fields[it->first]->GetFileName(), std::ios::binary);
            if (!src || !dst)
              NVJ_LOG->append(NVJ_ERROR, "Copy error: check read/write permissions");
            else
              dst << src.rdbuf();
            src.close();
            dst.close();
            myUploadRepo->reload();
          }
        }
        return true;
      }

    } uploader;


    class ListUploadedFiles: public DynamicPage
    {
      bool getPage(HttpRequest* request, HttpResponse *response)
      {
        string json = "{ \"data\" : [";
        set< string >* filenames = myUploadRepo->getFilenames();
        std::set<std::string>::iterator it = filenames->begin(); 
        while (it != filenames->end())
        {
          json += string("\"") + it->c_str() + '\"';
          if (++it != filenames->end()) 
            json += ", ";
        }
        json += "] }";
        
        return fromString(json, response);
      }

    } listUploadedFiles;
    
  public:
    MyDynamicRepository() : DynamicRepository()
    {
      add("uploader",&uploader);
      add("getListUploadedFiles.txt", &listUploadedFiles);
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
  //webServer->setUseSSL(true, "../mycert.pem");
  
  LocalRepository myLocalRepo("", "./html");
  webServer->addRepository(&myLocalRepo);

  myUploadRepo = new LocalRepository("upload", "./upload");

  MyDynamicRepository myRepo;
  webServer->addRepository(&myRepo);
  webServer->addRepository(myUploadRepo);
  
  webServer->startService();

  webServer->wait();
  
  LogRecorder::freeInstance();
  
  delete myUploadRepo;
  
  return 0;
}


