//********************************************************
/**
 * @file  PrecompiledRepository.hh
 *
 * @brief Handles a precompiled web repository
 *
 * @author T.Descombes (descombes@lpsc.in2p3.fr)
 *
 * @version 1	
 * @date 19/02/15
 */
//********************************************************

#ifndef PRECOMPILEDREPOSITORY_HH_
#define PRECOMPILEDREPOSITORY_HH_

#include <string>
#include <map>
#include "nvjThread.h"

#include "libnavajo/WebRepository.hh"


class PrecompiledRepository : public WebRepository
{
    pthread_mutex_t _mutex;

    struct WebStaticPage
    {
      const unsigned char* data;
      size_t length;
      WebStaticPage(const unsigned char* d,size_t l) : data(d), length(l) {};
    } ;

    typedef std::map<std::string, const WebStaticPage> IndexMap;
    static IndexMap indexMap;
    static std::string location; 
    
  public:
    PrecompiledRepository(const std::string& l="")
    { 
      location=l;
      while (location.size() && location[0]=='/') location.erase(0, 1);
      while (location.size() && location[location.size()-1]=='/') location.erase(location.size() - 1);
      pthread_mutex_init(&_mutex, NULL);
      if (!indexMap.size()) initIndexMap(); 
    };
    virtual ~PrecompiledRepository() { indexMap.clear(); };
    
    static void initIndexMap();

   /**
    * Free resources after use. Inherited from class WebRepository
    * called from WebServer::accept_request() method
    * @param webpage: a pointer to the generated page
    */
    inline void freeFile(unsigned char *webpage) { };

   /**
    * Try to resolve an http request by requesting the PrecompiledRepository. Inherited from class WebRepository
    * called from WebServer::accept_request() method
    * @param request: a pointer to the current request
    * @param response: a pointer to the new generated response
    * \return true if the repository contains the requested resource
    */
    inline virtual bool getFile(HttpRequest* request, HttpResponse *response)
    {
      std::string url = request->getUrl();
      if (url.compare(0, location.length(), location) != 0)
        return false;

      url.erase(0, location.length());
      while (url.size() && url[0]=='/') url.erase(0, 1);
      if (!url.size()) url="index.html";
      
      size_t webpageLen;
      unsigned char *webpage;
      pthread_mutex_lock( &_mutex );
      IndexMap::const_iterator i = indexMap.find (url);
      if (i == indexMap.end())
      {
        i = indexMap.find (url + ".gz");
        if (i == indexMap.end())
        {
          pthread_mutex_unlock( &_mutex );
          return false;
        }
        else
          response->setIsZipped(true);
      }

      webpage=(unsigned char*)((i->second).data); webpageLen=(i->second).length;
      pthread_mutex_unlock( &_mutex );
      response->setContent (webpage, webpageLen);
      return true;

    };
};

#endif

