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

    /**
    * Free resources after use. Inherited from class WebRepository
    * called from WebServer::accept_request() method
    * @param webpage: a pointer to the generated page
    */
    inline void freeFile(unsigned char *webpage) { ::free (webpage); };


    /**
    * Add new page to the repository
    * @param name: the url (from the document root)
    * @param page: the DynamicPage instance responsible for content generation
    */
    inline void add(const std::string url, DynamicPage *page)
    { 
      size_t i=0; 
      while (url.size() && url[i]=='/') i++;
      pthread_mutex_lock( &_mutex );
      indexMap.insert(std::pair<std::string, DynamicPage *>(url.substr(i, url.size()-i), page));
      pthread_mutex_unlock( &_mutex );
    };

    /**
    * Remove page from the repository
    * @param urlToRemove: the url (from the document root)
    * @param deleteDynamicPage: true if the related DynamicPage must be deleted
    */
    inline void remove(const std::string urlToRemove, bool deleteDynamicPage=false)
    {
      std::string url(urlToRemove);
      while (url.size() && url[0]=='/') url.erase(0, 1);
      pthread_mutex_lock( &_mutex );
      IndexMap::iterator i = indexMap.find (url);
      if (i == indexMap.end())
      {
        pthread_mutex_unlock( &_mutex );
        return ;
      }
      else
      {
        pthread_mutex_unlock( &_mutex );
        if (deleteDynamicPage)
          delete i->second;
        indexMap.erase(i);
        return ;
      }

      pthread_mutex_lock( &_mutex );
    }

    /**
    * Try to resolve an http request by requesting the DynamicRepository. Inherited from class WebRepository
    * called from WebServer::accept_request() method
    * @param request: a pointer to the current request
    * @param response: a pointer to the new generated response
    * \return true if the repository contains the requested resource
    */
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

