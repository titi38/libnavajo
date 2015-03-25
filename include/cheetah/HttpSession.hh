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

  static void createSession(std::string& id)
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

    sessions[id]=new std::map <std::string, void*>();
  };
  
  static bool find(const std::string& id)
  {
    bool res;
    pthread_mutex_lock( &sessions_mutex );
    res=sessions.size() && sessions.find(id) != sessions.end() ;
    pthread_mutex_unlock( &sessions_mutex );
    return res;
  }

    static void setSessionAttribute ( const std::string &sid, const std::string &name, void* value )
    {
      HttpSessionsContainerMap::const_iterator it = sessions.find(sid);
      if (it == sessions.end()) return;
      
      it->second->insert(std::pair<std::string, void*>(name, value));
    }
      
    static void *getSessionAttribute( const std::string &sid, const std::string &name )
    {
      HttpSessionsContainerMap::iterator it = sessions.find(sid);
      if (it == sessions.end()) return NULL;
      
      std::map <std::string, void*>* sessionMap=it->second;
      std::map <std::string, void*>::iterator it2 = sessionMap->find(name);
      if ( it2 != sessionMap->end() ) return it2->second;
      return NULL;
    }
    
    static std::vector<std::string> getSessionAttributeNames( const std::string &sid )
    {
      std::vector<std::string> res;
      HttpSessionsContainerMap::iterator it = sessions.find(sid);
      if (it != sessions.end()) 
      {
        std::map <std::string, void*>* sessionMap=it->second;
        std::map <std::string, void*>::iterator iter = sessionMap->begin();
        for(; iter!=sessionMap->end(); ++iter)
          res.push_back(iter->first);
      }
      return res;
    }
};

//****************************************************************************


#endif
