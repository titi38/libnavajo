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
#include <sstream>
#include <iomanip>
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

inline std::string escape_json(const std::string &s)
{
  std::ostringstream o;
  
  for (size_t i=0; i < s.length(); i++)
  {
    switch (s[i]) {
      case '"': o << "\\\""; break;
      case '\\': o << "\\\\"; break;
      case '\b': o << "\\b"; break;
      case '\f': o << "\\f"; break;
      case '\n': o << "\\n"; break;
      case '\r': o << "\\r"; break;
      case '\t': o << "\\t"; break;
      default:
        if ('\x00' <= s[i] && s[i] <= '\x1f')
        {
          o << "\\u"
          << std::hex << std::setw(4) << std::setfill('0') << (int) s[i];
        }
        else
          o << s[i];
    }
  }
  return o.str();
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
            std::ifstream  src( fields[it->first]->GetTempFileName().c_str(), std::ios::binary);
	    std::string dstFilename= std::string(UPLOAD_DIR)+'/'+fields[it->first]->GetFileName();
            std::ofstream  dst( dstFilename.c_str(), std::ios::binary);
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
        std::string json = "{ \"data\" : [";
        std::set< std::string >* filenames = myUploadRepo->getFilenames();
        std::set<std::string>::iterator it = filenames->begin(); 
        while (it != filenames->end())
        {
          json += std::string("\"") + escape_json(it->c_str()) + '\"';
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
  //webServer->setUseSSL(true, "../myCert.pem");
  
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


