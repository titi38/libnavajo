//********************************************************
/**
 * @file  DynamicRepository.hh 
 *
 * @brief Get json files to webserver
 *
 * @author T.Descombes (descombes@lpsc.in2p3.fr)
 *
 * @version 1	
 * @date 19/02/10 
 */
//********************************************************
 
#ifndef DYNAMICREPOSITORY_HH_
#define DYNAMICREPOSITORY_HH_

#include <string>
#include <map>

#include "cheetah/WebRepository.hh"


class DynamicRepository : public WebRepository
{

    pthread_mutex_t _mutex;
    typedef std::map<std::string, DynamicPage *> IndexMap;
    IndexMap indexMap;
    
  public:
    DynamicRepository() {  };
    virtual ~DynamicRepository() { pthread_mutex_init(&_mutex, NULL); indexMap.clear(); };
    
    inline void freeFile(unsigned char *webpage) { ::free (webpage); };

    inline void add(std::string url, DynamicPage *page) { indexMap.insert(pair<string, DynamicPage *>(url, page)); };

    inline virtual bool getFile(const string& url, unsigned char **webpage, size_t *webpageLen, char **respCookies, const HttpRequestType reqType, const char* reqParams="", const char* reqCookies="")
    {
      pthread_mutex_lock( &_mutex );
      IndexMap::const_iterator i = indexMap.find (url);
      if (i == indexMap.end())
      {
        pthread_mutex_unlock( &_mutex );
        return false;
      }
      else
      {
        pthread_mutex_unlock( &_mutex );
        return i->second->getPage(reqParams, webpage, webpageLen);
      }
    };

};
#endif

