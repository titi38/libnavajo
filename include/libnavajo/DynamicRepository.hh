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
#include <pthread.h>

#include "libnavajo/WebRepository.hh"
#include "libnavajo/DynamicPage.hh"


class DynamicRepository : public WebRepository
{

    pthread_mutex_t _mutex;

    struct PageEntry
    {
      DynamicPage *page;
      bool owned;

      PageEntry() : page(NULL), owned(false) {}
      PageEntry(DynamicPage *p, bool o) : page(p), owned(o) {}
    };

    typedef std::map<std::string, PageEntry> IndexMap;
    IndexMap indexMap;

    static inline std::string normalizeUrl(const std::string& url)
    {
      size_t i = 0;
      while (i < url.size() && url[i] == '/') i++;
      return url.substr(i);
    }

    inline void clearIndex(bool deleteOwnedPages)
    {
      if (deleteOwnedPages)
      {
        for (IndexMap::iterator it = indexMap.begin(); it != indexMap.end(); ++it)
        {
          if (it->second.owned)
            delete it->second.page;
        }
      }
      indexMap.clear();
    }
    
  public:
    DynamicRepository() { pthread_mutex_init(&_mutex, NULL); };

    virtual ~DynamicRepository()
    {
      pthread_mutex_lock(&_mutex);
      clearIndex(true);
      pthread_mutex_unlock(&_mutex);
      pthread_mutex_destroy(&_mutex);
    };

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
    * @param takeOwnership: true if the repository must delete the DynamicPage instance
    */
    inline void add(const std::string& url, DynamicPage *page, bool takeOwnership=false)
    { 
      std::string normalizedUrl = normalizeUrl(url);
      pthread_mutex_lock( &_mutex );

      IndexMap::iterator it = indexMap.find(normalizedUrl);
      if (it != indexMap.end())
      {
        if (it->second.owned)
          delete it->second.page;
        it->second = PageEntry(page, takeOwnership);
      }
      else
      {
        indexMap.insert(std::pair<std::string, PageEntry>(normalizedUrl, PageEntry(page, takeOwnership)));
      }

      pthread_mutex_unlock( &_mutex );
    };

    /**
    * Remove page from the repository
    * @param urlToRemove: the url (from the document root)
    * @param deleteDynamicPage: true if the related DynamicPage must be deleted
    */
    inline void remove(const std::string& urlToRemove, bool deleteDynamicPage=false)
    {
      std::string url = normalizeUrl(urlToRemove);

      pthread_mutex_lock( &_mutex );
      IndexMap::iterator i = indexMap.find (url);
      if (i != indexMap.end())
      {
        DynamicPage *page = i->second.page;
        bool owned = i->second.owned;
        indexMap.erase(i);

        if (deleteDynamicPage || owned)
          delete page;
      }
      pthread_mutex_unlock( &_mutex );
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
      std::string url = normalizeUrl(request->getUrl());

      pthread_mutex_lock( &_mutex );
      IndexMap::const_iterator i = indexMap.find (url);
      if (i == indexMap.end() || i->second.page == NULL)
      {
        pthread_mutex_unlock( &_mutex );
        return false;
      }

      DynamicPage *page = i->second.page;
      pthread_mutex_unlock( &_mutex );

      bool res = page->getPage( request, response );
      if (request->getSessionId().size())
        response->addSessionCookie(request->getSessionId());
      return res;
    };

};
#endif
