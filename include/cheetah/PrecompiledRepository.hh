//********************************************************
/**
 * @file  PrecompiledRepository.hh
 *
 * @brief class to access the files of the precompiled HTTP Repository
 *
 * @author T.Descombes (descombes@lpsc.in2p3.fr)
 *
 * @version 1	
 * @date 19/02/10 
 */
//********************************************************

#ifndef PrecompiledRepository_HH_
#define PrecompiledRepository_HH_

#include <string>
#include <map>
#include "thread.h"

#include "cheetah/WebRepository.hh"


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
    
  public:
    PrecompiledRepository() { pthread_mutex_init(&_mutex, NULL); if (!indexMap.size()) initIndexMap(); };
    virtual ~PrecompiledRepository() { indexMap.clear(); };
    
    static void initIndexMap();

    inline void freeFile(unsigned char *webpage) { };

    inline virtual bool getFile(const std::string& URL, unsigned char **webpage, size_t *webpageLen, const char* params="", const char* cookies="")
    {
      pthread_mutex_lock( &_mutex );
      IndexMap::const_iterator i = indexMap.find (URL);
      if (i == indexMap.end())
      {
        pthread_mutex_unlock( &_mutex );
        return false;
      }
      else
      {
        *webpage=(unsigned char*)((i->second).data); *webpageLen=(i->second).length;
        pthread_mutex_unlock( &_mutex );
        return true;
      }
    };
};

#endif

