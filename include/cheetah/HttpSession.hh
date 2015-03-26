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
  static time_t lastExpirationSearchTime;
  
  public:

    /**********************************************************************/

    static void create(std::string& id, time_t sessionLifeTime=20*6)
    {

      const size_t idLength=128;
      const char elements[] = "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
      const size_t nbElements = sizeof(elements) / sizeof(char);
      srand(time(NULL));

      id.reserve(idLength);

      do
      {
        id.clear();
        for(size_t i = 0; i < idLength; ++i) 
          id+=elements[rand()%(nbElements - 1)];
      }
      while (find(id));

      pthread_mutex_lock( &sessions_mutex );
      sessions[id]=new std::map <std::string, void*>();
      pthread_mutex_unlock( &sessions_mutex );
      time_t *expiration=(time_t *)malloc(sizeof(time_t));
      *expiration=time(NULL)+sessionLifeTime;
      setAttribute(id, "session_expiration", expiration);
      
      // look for expired session
      if (time(NULL) - lastExpirationSearchTime > 60)
        removeExpiredSession();

    };
    
    /**********************************************************************/

    static void updateExpiration(const std::string& id, time_t sessionLifeTime=20*6)
    {
      time_t *expiration=(time_t*)getAttribute(id, "session_expiration");
      if (expiration != NULL)
        *expiration=time(NULL)+sessionLifeTime;
    };

    /**********************************************************************/

    static void removeExpiredSession()
    {

      pthread_mutex_lock( &sessions_mutex );
      HttpSessionsContainerMap::iterator it = sessions.begin();
      for (;it != sessions.end(); it++)
      {
        std::map <std::string, void*>* attributesMap=it->second;
        std::map <std::string, void*>::iterator it2 = attributesMap->find("session_expiration");
        time_t *expiration=NULL;
        if (it2 != attributesMap->end()) expiration=(time_t*) it2->second;
        if (expiration!=NULL && *expiration > time(NULL))
          continue;
        removeAllAttribute(attributesMap);
        delete attributesMap;
        sessions.erase(it);
      }
      pthread_mutex_unlock( &sessions_mutex );
    }

    /**********************************************************************/

    static bool find(const std::string& id)
    {

      bool res;
      pthread_mutex_lock( &sessions_mutex );
      res=sessions.size() && sessions.find(id) != sessions.end() ;
      pthread_mutex_unlock( &sessions_mutex );
      if (res)
        updateExpiration(id);

      return res;
    }
    
    /**********************************************************************/
    
    static void remove(const std::string& sid)
    {
      pthread_mutex_lock( &sessions_mutex );
      HttpSessionsContainerMap::iterator it = sessions.find(sid);
      if (it == sessions.end()) { pthread_mutex_unlock( &sessions_mutex ); return; };
      removeAllAttribute(it->second);
      delete it->second;
      sessions.erase(it);
      pthread_mutex_unlock( &sessions_mutex );
    }
    
    /**********************************************************************/
    
    static void setAttribute ( const std::string &sid, const std::string &name, void* value )
    {
      pthread_mutex_lock( &sessions_mutex );
      HttpSessionsContainerMap::const_iterator it = sessions.find(sid);

      if (it == sessions.end()) { pthread_mutex_unlock( &sessions_mutex ); return; };
      
      it->second->insert(std::pair<std::string, void*>(name, value));
      pthread_mutex_unlock( &sessions_mutex );
    }

    /**********************************************************************/ 

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
    
    /**********************************************************************/
    
    static void removeAllAttribute( std::map <std::string, void*>* attributesMap)
    {
      std::map <std::string, void*>::iterator iter = attributesMap->begin();
      for(; iter!=attributesMap->end(); ++iter)
        if (iter->second != NULL) free (iter->second);
    }

    /**********************************************************************/
    
    static void removeAttribute( const std::string &sid, const std::string &name )
    {
      pthread_mutex_lock( &sessions_mutex );
      HttpSessionsContainerMap::iterator it = sessions.find(sid);
      if (it == sessions.end()) { pthread_mutex_unlock( &sessions_mutex ); return; }
      std::map <std::string, void*>* attributesMap=it->second;
      std::map <std::string, void*>::iterator it2 = attributesMap->find(name);
      if ( it2 != attributesMap->end() ) 
      { 
        if (it2->second != NULL) free (it2->second);
        attributesMap->erase(it2);
      }
      pthread_mutex_unlock( &sessions_mutex ); 
    }

    /**********************************************************************/
    
    static std::vector<std::string> getAttributeNames( const std::string &sid )
    {
      pthread_mutex_lock( &sessions_mutex );
      std::vector<std::string> res;
      HttpSessionsContainerMap::iterator it = sessions.find(sid);
      if (it != sessions.end()) 
      {
        std::map <std::string, void*>* attributesMap=it->second;
        std::map <std::string, void*>::iterator iter = attributesMap->begin();
        for(; iter!=attributesMap->end(); ++iter)
          res.push_back(iter->first);
      }
      pthread_mutex_unlock( &sessions_mutex );
      return res;
    }
};

//****************************************************************************


#endif
