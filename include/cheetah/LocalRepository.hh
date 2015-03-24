//********************************************************
/**
 * @file  LocalRepository.hh 
 *
 * @brief Local aliases manager for webserver
 *
 * @author T.Descombes (descombes@lpsc.in2p3.fr)
 *
 * @version 1	
 * @date 27/01/14
 */
//********************************************************
 
#ifndef LocalRepository_HH_
#define LocalRepository_HH_

#include "WebRepository.hh"

#include <set>
#include <string>
#include "cheetah/thread.h"

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

    virtual bool getFile(const string& url, unsigned char **webpage, size_t *webpageLen, char **respCookies, const HttpRequestType reqType, const char* reqParams="", const char* reqCookies="");
    virtual void freeFile(unsigned char *webpage) { ::free(webpage); };
    void addDirectory(const string& alias, const string& dirPath);    
    void clearAliases();
    void printFilenames();
};

#endif

