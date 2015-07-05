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
#include "libnavajo/thread.h"

using namespace std;

class LocalRepository : public WebRepository
{
    pthread_mutex_t _mutex;

    set< string > filenamesSet; // list of available files
    set< pair<string,string> > aliasesSet; // alias name | Path to local directory

    bool loadFilename_dir(const string& alias, const string& path, const string& subpath);
    bool fileExist(const string& url);

    
  public:
    LocalRepository () { pthread_mutex_init(&_mutex, NULL); };
    virtual ~LocalRepository () { clearAliases(); };

    virtual bool getFile(HttpRequest* request, HttpResponse *response);
    virtual void freeFile(unsigned char *webpage) { ::free(webpage); };
    void addDirectory(const string& alias, const string& dirPath);    
    void clearAliases();
    void printFilenames();
};

#endif

