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

    virtual bool getFile(HttpRequest* request, HttpResponse *response);
    virtual void freeFile(unsigned char *webpage) { ::free(webpage); };
    //void addDirectory(const std::string& alias, const std::string& dirPath);
    //void clearAliases();
    void reload();
    inline std::set< std::string >* getFilenames() { return &filenamesSet; }
    void printFilenames();
};

#endif

