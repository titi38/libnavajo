//********************************************************
/**
 * @file  LocalRepository.hh
 *
 * @brief Handles local web repository
 *
 * @author T.Descombes (thierry.descombes@gmail.com)
 *
 * @version 1        
 * @date 19/02/15
 */
//********************************************************
 
#ifndef LOCALREPOSITORY_HH_
#define LOCALREPOSITORY_HH_

#include "WebRepository.hh"

#include <set>
#include <string>
#include "libnavajo/nvjThread.h"


class LocalRepository : public WebRepository
{
    pthread_mutex_t _mutex;

    std::set< std::string > filenamesSet; // list of available files
    //pair<std::string,std::string> aliasesSet; // alias name | Path to local directory
    std::string aliasName;
    std::string fullPathToLocalDir;

    bool loadFilename_dir(const std::string& alias, const std::string& path, const std::string& subpath="");
    bool fileExist(const std::string& url);

    
  public:
    LocalRepository (const std::string& alias, const std::string& dirPath);
    virtual ~LocalRepository () { };

    /**
    * Try to resolve an http request by requesting the LocalRepository. Inherited from class WebRepository
    * called from WebServer::accept_request() method
    * @param request: a pointer to the current request
    * @param response: a pointer to the new generated response
    * \return true if the repository contains the requested resource
    */
    virtual bool getFile(HttpRequest* request, HttpResponse *response);

   /**
    * Free resources after use. Inherited from class WebRepository
    * called from WebServer::accept_request() method
    * @param webpage: a pointer to the generated page
    */
    virtual void freeFile(unsigned char *webpage) { ::free(webpage); };

   /**
    * Reload the content of the directory
    * SHOULD BE CALLED EACH TIME A FILE IS CREATED, MODIFIED, OR DELETED
    */
    void reload();

   /**
    * Return the list of available resources (list of url)
    */
    inline std::set< std::string >* getFilenames() { return &filenamesSet; }
};

#endif

