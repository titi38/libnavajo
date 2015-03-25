//****************************************************************************
/**
 * @file  HttpSession.hh 
 *
 * @brief Contains Http Sessions
 *
 * @author T.Descombes (descombes@lpsc.in2p3.fr)
 *
 * @version 1	
 * @date 27/01/15
 */
//****************************************************************************

#ifndef HTTPSESSION_HH_
#define HTTPSESSION_HH_

#include <map>
#include <vector>
#include <string>
#include <sstream>


class HttpSession
{
  typedef std::map <std::string, std::map <std::string, void*>* > HttpSessionsContainerMap;
  static HttpSessionsContainerMap sessions;
  static pthread_mutex_t sessions_mutex; 

  public:

    static void create(std::string& id)
    {
      const size_t idLength=128;
      const char elements[] = "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
      const size_t numElements = sizeof(elements) / sizeof(char);
      srand(time(NULL));

      id.reserve(idLength);

      do
      {
        id.clear();
        for(std::size_t i = 0; i < idLength; ++i) 
          id+=elements[rand()%(numElements - 1)];

  //      printf("new SID = '%s'\n",id.c_str()); fflush(NULL);
      }
      while (find(id));

      pthread_mutex_lock( &sessions_mutex );
      sessions[id]=new std::map <std::string, void*>();
      pthread_mutex_unlock( &sessions_mutex );
    };
    
    static bool find(const std::string& id)
    {
      bool res;
      pthread_mutex_lock( &sessions_mutex );
      res=sessions.size() && sessions.find(id) != sessions.end() ;
      pthread_mutex_unlock( &sessions_mutex );
      return res;
    }

    static void remove(const std::string& sid)
    {
      pthread_mutex_lock( &sessions_mutex );
      HttpSessionsContainerMap::iterator it = sessions.find(sid);
      if (it == sessions.end()) { pthread_mutex_unlock( &sessions_mutex ); return; };
      delete it->second;
      sessions.erase(it);
      pthread_mutex_unlock( &sessions_mutex );
    }

    static void setAttribute ( const std::string &sid, const std::string &name, void* value )
    {
      pthread_mutex_lock( &sessions_mutex );
      HttpSessionsContainerMap::const_iterator it = sessions.find(sid);

      if (it == sessions.end()) { pthread_mutex_unlock( &sessions_mutex ); return; };
      
      it->second->insert(std::pair<std::string, void*>(name, value));
      pthread_mutex_unlock( &sessions_mutex );
    }
      
    static void *getAttribute( const std::string &sid, const std::string &name )
    {
      pthread_mutex_lock( &sessions_mutex );
      HttpSessionsContainerMap::iterator it = sessions.find(sid);
      if (it == sessions.end()) { pthread_mutex_unlock( &sessions_mutex ); return NULL; }
      
      std::map <std::string, void*>* sessionMap=it->second;
      std::map <std::string, void*>::iterator it2 = sessionMap->find(name);
      pthread_mutex_unlock( &sessions_mutex );

      if ( it2 != sessionMap->end() ) return it2->second;
      return NULL;
    }
    
    static void removeAttribute( const std::string &sid, const std::string &name )
    {
      pthread_mutex_lock( &sessions_mutex );
      HttpSessionsContainerMap::iterator it = sessions.find(sid);
      if (it == sessions.end()) { pthread_mutex_unlock( &sessions_mutex ); return; }
      std::map <std::string, void*>* sessionMap=it->second;
      std::map <std::string, void*>::iterator it2 = sessionMap->find(name);
      if ( it2 != sessionMap->end() ) sessionMap->erase(it2);
      pthread_mutex_unlock( &sessions_mutex ); 
    }
    
    static std::vector<std::string> getAttributeNames( const std::string &sid )
    {
      pthread_mutex_lock( &sessions_mutex );
      std::vector<std::string> res;
      HttpSessionsContainerMap::iterator it = sessions.find(sid);
      if (it != sessions.end()) 
      {
        std::map <std::string, void*>* sessionMap=it->second;
        std::map <std::string, void*>::iterator iter = sessionMap->begin();
        for(; iter!=sessionMap->end(); ++iter)
          res.push_back(iter->first);
      }
      pthread_mutex_unlock( &sessions_mutex );
      return res;
    }
};

//****************************************************************************


#endif
