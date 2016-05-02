//********************************************************
/**
 * @file  DynamicRepository.hh 
 *
 * @brief Handles dynamic web repository
 *
 * @author T.Descombes (descombes@lpsc.in2p3.fr)
 *
 * @version 1	
 * @date 19/02/15
 */
//********************************************************
 
#ifndef DYNAMICREPOSITORY_HH_
#define DYNAMICREPOSITORY_HH_

#include <string>
#include <map>

#include "libnavajo/WebRepository.hh"


class DynamicRepository : public WebRepository
{

    pthread_mutex_t _mutex;
    typedef std::map<std::string, DynamicPage *> IndexMap;
    IndexMap indexMap;
    
  public:
    DynamicRepository() { pthread_mutex_init(&_mutex, NULL); };
    virtual ~DynamicRepository() { indexMap.clear(); };
    
    inline void freeFile(unsigned char *webpage) { ::free (webpage); };

    inline void add(std::string url, DynamicPage *page)
    { 
      size_t i=0; 
      while (url.size() && url[i]=='/') i++;
      indexMap.insert(std::pair<std::string, DynamicPage *>(url.substr(i, url.size()-i), page)); };

    inline virtual bool getFile(HttpRequest* request, HttpResponse *response)
    {
      std::string url = request->getUrl();
      while (url.size() && url[0]=='/') url.erase(0, 1);
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
        bool res = i->second->getPage( request, response );
        if (request->getSessionId().size())
          response->addSessionCookie(request->getSessionId());
        return res;
      }
    };

};
#endif

