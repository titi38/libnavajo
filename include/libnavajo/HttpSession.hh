//****************************************************************************
/**
 * @file  HttpSession.hh 
 *
 * @brief The Http Sessions Manager class
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

#include <stdlib.h>

class SessionAttributeObject
{
  public:
    virtual ~SessionAttributeObject() {};
};

class HttpSession
{
  typedef struct {
    enum{BASIC, OBJECT} type;
    union
    {
      void *ptr;
      SessionAttributeObject *obj;
    };
  } SessionAttribute;


  typedef std::map <std::string, std::map <std::string, SessionAttribute>* > HttpSessionsContainerMap;

  static HttpSessionsContainerMap sessions;
  static pthread_mutex_t sessions_mutex; 
  static time_t lastExpirationSearchTime;
  static time_t sessionLifeTime;
  
  public:

    inline static void setSessionLifeTime(const time_t sec) { sessionLifeTime = sec; };

    inline static time_t getSessionLifeTime() { return sessionLifeTime; };

    /**********************************************************************/

    static void create(std::string& id)
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
      sessions[id]=new std::map <std::string, SessionAttribute>();
      pthread_mutex_unlock( &sessions_mutex );
      time_t *expiration=(time_t *)malloc(sizeof(time_t));
      *expiration=time(NULL)+sessionLifeTime;
      setAttribute(id, "session_expiration", expiration);
      
      // look for expired session (max every minute)
      if (time(NULL) > lastExpirationSearchTime + 60)
      {
        removeExpiredSession();
        lastExpirationSearchTime = time(NULL);
      }
    };
    
    /**********************************************************************/

    static void updateExpiration(const std::string& id)
    {
      time_t *expiration=(time_t*)getAttribute(id, "session_expiration");
      if (expiration != NULL)
        *expiration=time(NULL)+sessionLifeTime;
    };

    /**********************************************************************/

    static void noExpiration(const std::string& id)
    {
      time_t *expiration=(time_t*)getAttribute(id, "session_expiration");
      if (expiration != NULL)
        *expiration=0;
    };

    /**********************************************************************/

    static void removeExpiredSession()
    {

      pthread_mutex_lock( &sessions_mutex );
      HttpSessionsContainerMap::iterator it = sessions.begin();
      for (;it != sessions.end(); )
      {
        std::map <std::string, SessionAttribute>* attributesMap=it->second;
        std::map <std::string, SessionAttribute>::iterator it2 = attributesMap->find("session_expiration");
        time_t *expiration=NULL;
        if (it2 != attributesMap->end()) expiration=(time_t*) it2->second.ptr;

        if (expiration!=NULL && *expiration && *expiration > time(NULL))
        {
          it++;
          continue;
        }

        removeAllAttribute(attributesMap);
        delete attributesMap;
        sessions.erase(it++);
      }
      pthread_mutex_unlock( &sessions_mutex );
    }

    /**********************************************************************/

    static void removeAllSession()
    {
      pthread_mutex_lock( &sessions_mutex );
      HttpSessionsContainerMap::iterator it = sessions.begin();
      for (;it != sessions.end(); )
      {
        std::map <std::string, SessionAttribute>* attributesMap=it->second;
        removeAllAttribute(attributesMap);
        delete attributesMap;
        sessions.erase(it++);
      }    
    }
    
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

    static void setObjectAttribute ( const std::string &sid, const std::string &name, SessionAttributeObject *sessionAttributeObject )
    {
      pthread_mutex_lock( &sessions_mutex );
      HttpSessionsContainerMap::const_iterator it = sessions.find(sid);

      if (it == sessions.end()) { pthread_mutex_unlock( &sessions_mutex ); return; };

      SessionAttribute attribute;
      attribute.type=SessionAttribute::OBJECT;
      attribute.obj=sessionAttributeObject;
      it->second->insert(std::pair<std::string, SessionAttribute>(name, attribute ));
      pthread_mutex_unlock( &sessions_mutex );
    }

    /**********************************************************************/

    static void setAttribute ( const std::string &sid, const std::string &name, void* value )
    {
      pthread_mutex_lock( &sessions_mutex );
      HttpSessionsContainerMap::const_iterator it = sessions.find(sid);

      if (it == sessions.end()) { pthread_mutex_unlock( &sessions_mutex ); return; };

      SessionAttribute attribute;
      attribute.type=SessionAttribute::BASIC;
      attribute.ptr=value;
      it->second->insert(std::pair<std::string, SessionAttribute>(name, attribute ));
      pthread_mutex_unlock( &sessions_mutex );
    }

    /**********************************************************************/

    static SessionAttributeObject *getObjectAttribute( const std::string &sid, const std::string &name )
    {
      pthread_mutex_lock( &sessions_mutex );
      HttpSessionsContainerMap::iterator it = sessions.find(sid);
      if (it == sessions.end()) { pthread_mutex_unlock( &sessions_mutex ); return NULL; }

      std::map <std::string, SessionAttribute>* sessionMap=it->second;
      std::map <std::string, SessionAttribute>::iterator it2 = sessionMap->find(name);
      pthread_mutex_unlock( &sessions_mutex );

      if ( it2 != sessionMap->end() && (it2->second.type == SessionAttribute::OBJECT) )
        return it2->second.obj;
      return NULL;
    }

    /**********************************************************************/

    static void *getAttribute( const std::string &sid, const std::string &name )
    {
      pthread_mutex_lock( &sessions_mutex );
      HttpSessionsContainerMap::iterator it = sessions.find(sid);
      if (it == sessions.end()) { pthread_mutex_unlock( &sessions_mutex ); return NULL; }

      std::map <std::string, SessionAttribute>* sessionMap=it->second;
      std::map <std::string, SessionAttribute>::iterator it2 = sessionMap->find(name);
      pthread_mutex_unlock( &sessions_mutex );

      if ( it2 != sessionMap->end() && (it2->second.type == SessionAttribute::BASIC))
        return it2->second.ptr;
      return NULL;
    }

    /**********************************************************************/
    
    static void removeAllAttribute( std::map <std::string, SessionAttribute>* attributesMap)
    {
      std::map <std::string, SessionAttribute>::iterator iter = attributesMap->begin();
      for(; iter!=attributesMap->end(); ++iter)
        if (iter->second.ptr != NULL)
        {
          if (iter->second.type==SessionAttribute::OBJECT)
            delete iter->second.obj;
          else
            free (iter->second.ptr);
        }
    }

    /**********************************************************************/
    
    static void removeAttribute( const std::string &sid, const std::string &name )
    {
      pthread_mutex_lock( &sessions_mutex );
      HttpSessionsContainerMap::iterator it = sessions.find(sid);
      if (it == sessions.end()) { pthread_mutex_unlock( &sessions_mutex ); return; }
      std::map <std::string, SessionAttribute>* attributesMap=it->second;
      std::map <std::string, SessionAttribute>::iterator it2 = attributesMap->find(name);
      if ( it2 != attributesMap->end() ) 
      { 
        if (it2->second.ptr != NULL)
        {
          if (it2->second.type==SessionAttribute::OBJECT)
            delete it2->second.obj;
          else
            free (it2->second.ptr);
        }
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
        std::map <std::string, SessionAttribute>* attributesMap=it->second;
        std::map <std::string, SessionAttribute>::iterator iter = attributesMap->begin();
        for(; iter!=attributesMap->end(); ++iter)
          res.push_back(iter->first);
      }
      pthread_mutex_unlock( &sessions_mutex );
      return res;
    }
    
    /**********************************************************************/
    
    static void printAll()
    {
      pthread_mutex_lock( &sessions_mutex );
      HttpSessionsContainerMap::iterator it = sessions.begin();
      for (;it != sessions.end(); ++it )
      {
        std::map <std::string, SessionAttribute>* attributesMap=it->second;
        printf("Session SID : '%s' \n", it->first.c_str());
        std::map <std::string, SessionAttribute>::iterator iter = attributesMap->begin();
        for(; iter!=attributesMap->end(); ++iter)
          if ( iter->second.ptr != NULL ) printf("\t'%s'\n", iter->first.c_str());
      }
            pthread_mutex_unlock( &sessions_mutex );    
    }
    
    /**********************************************************************/    

};

//****************************************************************************


#endif
