//****************************************************************************
/**
 * @file  HttpRequest.hh 
 *
 * @brief The Http Request Parameters class
 *
 * @author T.Descombes (descombes@lpsc.in2p3.fr)
 *
 * @version 1	
 * @date 27/01/15
 */
//****************************************************************************

#ifndef HTTPREQUEST_HH_
#define HTTPREQUEST_HH_

#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <openssl/ssl.h>

#include "libnavajo/IpAddress.hh"
#include "HttpSession.hh"

#include "MPFDParser/Parser.h"

//****************************************************************************

typedef enum {
  UNKNOWN_METHOD = 0,
  GET_METHOD = 1,
  POST_METHOD = 2,
  PUT_METHOD = 3,
  DELETE_METHOD = 4,
  UPDATE_METHOD = 5,
  PATCH_METHOD = 6,
  OPTIONS_METHOD = 7
} HttpRequestMethod;

typedef enum { GZIP, ZLIB, NONE } CompressionMode;
typedef struct
{
  int socketId;
  IpAddress ip;
  CompressionMode compression;
  SSL *ssl;
  BIO *bio;
  std::string *peerDN;
//  pthread_mutex_t client_mutex;
} ClientSockData;

class HttpRequest
{
  typedef std::map <std::string, std::string> HttpRequestParametersMap;
  typedef std::map <std::string, std::string> HttpRequestCookiesMap;  

  const char *url;
  const char *origin;
  ClientSockData *clientSockData;
  std::string httpAuthUsername;
  HttpRequestMethod httpMethod;
  HttpRequestCookiesMap cookies;
  HttpRequestParametersMap parameters;
  std::string sessionId;
  MPFD::Parser *mutipartContentParser;
  const char *mimeType;
  std::vector<uint8_t> *payload;

  /**********************************************************************/
  /**
  * decode all http parameters and fill the parameters Map
  * @param p: raw string containing all the http parameters
  */  
  inline void decodParams( const std::string& p )
  {
    size_t start = 0, end = 0;
    std::string paramstr=p;

    while ((end = paramstr.find_first_of("%+", start)) != std::string::npos) 
    {
      size_t len=paramstr.length()-end-1;
      switch (paramstr[end])
      {
        case '%':
          if ( paramstr[end+1]=='%' && len )
            paramstr=paramstr.erase(end+1,1);
          else
          {
            if (len < 2) break;

            unsigned int specar;
            std::string hexChar=paramstr.substr(end+1,2);
            std::stringstream ss; ss << std::hex << hexChar.c_str();
            ss >> specar;
            paramstr[end] = (char)specar;
            paramstr=paramstr.erase(end+1,2);
          }
          break;

        case '+':
          paramstr[end]=' ';
          break;
      }   
      
      start=end+1;
    }
    
    start = 0; end = 0;
    bool islastParam=false;
    while (!islastParam) 
    {
      islastParam= (end = paramstr.find('&', start)) == std::string::npos;
      if (islastParam) end=paramstr.size();
      
      std::string theParam=paramstr.substr(start, end - start);
      
      size_t posEq=0;
      if ((posEq = theParam.find('=')) == std::string::npos)
        parameters[theParam]="";
      else {
        auto key        = theParam.substr(0,posEq);
        auto value      = theParam.substr(posEq+1);
        if( parameters.count( key ) == 0 ) {
          parameters[key] = value;
        } else {
          auto arrayKey = key + "[]";
          if( parameters.count( arrayKey ) == 1 ) {
            parameters[arrayKey] += "|" + value;
          } else {
            parameters[arrayKey] = parameters[key] + "|" + value;
          }
          parameters[key] = value;
        }
      }

      start = end + 1;
    }
  };

  /**********************************************************************/
  /**
  * decode all http cookies and fill the cookies Map
  * @param c: raw string containing all the cockies definitions
  */
  inline void decodCookies( const std::string& c )
  {
    std::stringstream ss(c);
    std::string theCookie;
    while (std::getline(ss, theCookie, ';')) 
    {
      size_t posEq=0;
      if ((posEq = theCookie.find('=')) != std::string::npos)
      {
        size_t firstC=0; while (!iswgraph(theCookie[firstC]) && firstC < posEq) { firstC++; };

        if (posEq-firstC > 0 && theCookie.length()-posEq > 0) cookies[theCookie.substr(firstC,posEq-firstC)]=theCookie.substr(posEq+1, theCookie.length()-posEq);
      }
    }
  };

  /**********************************************************************/
  /**
  * check the SID cookie and set the sessionID attribute if the session is valid
  * (called by constructor)
  */
  inline void getSession()
  {
    sessionId = getCookie("SID");

    if (sessionId.length() && HttpSession::find(sessionId))
      return;

    initSessionId();
  };



  public:
  
    /**********************************************************************/
    /**
    * get cookie value
    * @param name: the cookie name
    */
    inline std::string getCookie( const std::string& name ) const
    {
      std::string res="";
      getCookie(name, res);
      return res;
    }
    
    /**********************************************************************/
    /**
    * get cookie value
    * @param name: the cookie name
    * @param value: the cookie value
    * @return true is the cookie exist
    */  
    inline bool getCookie( const std::string& name, std::string &value ) const
    {
      if(!cookies.empty())
      {
        HttpRequestCookiesMap::const_iterator it;
        if((it = cookies.find(name)) != cookies.end())
        {
          value=it->second;
          return true;
        }
      }
      return false;
    }
    
    /**********************************************************************/
    /**
    * get cookies list
    * @return a vector containing all cookies names
    */ 
    inline std::vector<std::string> getCookiesNames() const
    {
      std::vector<std::string> res;
      for(HttpRequestCookiesMap::const_iterator iter=cookies.begin(); iter!=cookies.end(); ++iter)
       res.push_back(iter->first);
      return res;
    }

    /**********************************************************************/
    /**
    * get parameter value
    * @param name: the parameter name
    * @param value: the parameter value
    * @return true is the parameter exist
    */   
    inline bool getParameter( const std::string& name, std::string &value ) const
    {
      if(!parameters.empty())
      {
        HttpRequestParametersMap::const_iterator it;
        if((it = parameters.find(name)) != parameters.end())
        {
          value=it->second;
          return true;
        }
      }
      return false;
    }

    /**********************************************************************/
    /**
    * get parameter value
    * @param name: the parameter name
    * @return the parameter value
    */  
    inline std::string getParameter( const std::string& name ) const
    {
      std::string res="";
      getParameter(name, res);
      return res;
    }
    
    /**********************************************************************/
    /**
    * does the parameter exist ?
    * @param name: the parameter name
    * @return true is the parameter exist
    */   
    inline bool hasParameter( const std::string& name ) const
    {
      std::string tmp;
      return getParameter(name, tmp);
    }

    /**********************************************************************/
    /**
    * get parameters list
    * @return a vector containing all parameters names
    */ 
    inline std::vector<std::string> getParameterNames() const
    {
      std::vector<std::string> res;
      for(HttpRequestParametersMap::const_iterator iter=parameters.begin(); iter!=parameters.end(); ++iter)
       res.push_back(iter->first);
      return res;
    }
    
    /**********************************************************************/
    /**
    * is there a valid session cookie
    */
    inline bool isSessionValid()
    {
      return sessionId != "";
    }

    /**********************************************************************/
    /**
    * create a session cookie
    */ 
    inline void createSession()
    {
      HttpSession::create(sessionId);
    }
    
    /**
    * remove the session cookie
    */ 
    inline void removeSession()
    {
      if (sessionId == "") return;
      HttpSession::remove(sessionId);
    }

    /**
    * add an attribute to the session
    * @param name: the attribute name
    * @param value: the attribute value
    */ 
    void setSessionAttribute ( const std::string &name, void* value )
    {
      if (sessionId == "") createSession();
      HttpSession::setAttribute(sessionId, name, value);
    }

    /**
    * add an object attribute to the session
    * @param name: the attribute name
    * @param value: the object instance
    */
    void setSessionObjectAttribute ( const std::string &name, SessionAttributeObject* value )
    {
      if (sessionId == "") createSession();
      HttpSession::setObjectAttribute(sessionId, name, value);
    }
    
    /**
    * get an attribute of the server session
    * @param name: the attribute name
    * @return the attribute value or NULL if not found
    */ 
    void *getSessionAttribute( const std::string &name )
    {
      if (sessionId == "") return NULL;
      return HttpSession::getAttribute(sessionId, name);
    }

    /**
    * get an object attribute of the server session
    * @param name: the attribute name
    * @return the object instance or NULL if not found
    */
    SessionAttributeObject* getSessionObjectAttribute( const std::string &name )
    {
      if (sessionId == "") return NULL;
      return HttpSession::getObjectAttribute(sessionId, name);
    }
    
    /**
    * get the list of the attribute's Names of the server session
    * @return a vector containing all attribute's names
    */ 
    inline std::vector<std::string> getSessionAttributeNames()
    {
      if (sessionId == "") return std::vector<std::string>();;
      return HttpSession::getAttributeNames(sessionId);
    }

    /**
    * remove an attribute of the server session (if found)
    * @param name: the attribute name
    */ 
    inline void getSessionRemoveAttribute( const std::string &name )
    {
      if (sessionId != "")
        HttpSession::removeAttribute( sessionId, name );
    }

    /**
    * initialize sessionId value
    */ 
    inline void initSessionId() { sessionId = ""; };

    /**
    * get sessionId value
    * @return the sessionId value
    */    
    std::string getSessionId() const { return sessionId; };

    /**********************************************************************/
    /**
    * HttpRequest constructor
    * @param type:  the Http Request Type ( GET/POST/...)
    * @param url:  the requested url
    * @param params:  raw http parameters string
    * @cookies params: raw http cookies string
    */         
    HttpRequest(const HttpRequestMethod type, const char *url, const char *params, const char *cookies, const char *origin, const std::string &username, ClientSockData *client,
                const char* mimeType, std::vector<uint8_t>* payload=NULL, MPFD::Parser *parser=NULL)
    {
      this->httpMethod = type;
      this->url = url;
      this->origin = origin;
      this->httpAuthUsername=username;
      this->clientSockData=client;
      this->mimeType=mimeType ;
      this->payload=payload ;
      this->mutipartContentParser=parser;

      setParams( params );
      
      if (cookies != NULL && strlen(cookies))
        decodCookies(cookies);        
      getSession();
    };

    /**********************************************************************/
    /**
    * is there a multipart content in the request ?    
    * @return true or false
    */    
    inline bool isMultipartContent() const { return mutipartContentParser != NULL; };
    
    /**********************************************************************/
    /**
    * get the MPFD parser   
    * @return a pointer to the MPFDparser instance
    */    
    inline MPFD::Parser *getMPFDparser() { return mutipartContentParser; };

    /**********************************************************************/
    /**
    * get the MIME content-type (if defined)
    * @return the MIME content-type string
    */
    inline const char* getMimeType() const { return mimeType; };

    /**********************************************************************/
    /**
    * get the Request Payload (if it exists)
    * @return raw byte content
    */
    inline std::vector<uint8_t>& getPayload() { return *payload; };

    /**********************************************************************/
    /**
    * get url    
    * @return the requested url
    */
    inline const char *getUrl() const { return url; };

    /**********************************************************************/
    /**
    * set new url
    * @param name: the attribute name
    * */
    inline void setUrl(const char *newUrl) { url=newUrl; };

    // GLSR: torna pública a configuração de parâmetros permitindo realizar forwardTo com novos parâmetros
    inline void setParams( const char*params ) {
      if (params != NULL && strlen(params)) {
        decodParams(params);
      }
    }

    /**********************************************************************/
    /**
    * get request type    
    * @return the Http Request Type ( GET/POST/...)
    */
    inline HttpRequestMethod getRequestType() const { return httpMethod; };

    /**********************************************************************/
    /**
    * set new Request Type
    * @param name: the HttpRequestMethod
    * */
    inline void setRequestType(HttpRequestMethod newMethod) { httpMethod=newMethod; };

  /**********************************************************************/
    /**
    * get request origin    
    * @return the Http Request Origin
    */
    inline const char* getRequestOrigin() const { return origin; };

    /**********************************************************************/
    /**
    * get peer IP address    
    * @return the ip address
    */
    inline IpAddress& getPeerIpAddress()
    {
      return clientSockData->ip;
    };
    
    /**********************************************************************/
    /**
    * get http authentification username
    * @return the login
    */    
    inline std::string& getHttpAuthUsername()
    {
      return httpAuthUsername;
    };

    /**********************************************************************/
    /**
    * get peer x509 dn 
    * @return the DN of the peer certificate
    */   
    inline std::string& getX509PeerDN()
    {
      return *(clientSockData->peerDN);
    };

    /**********************************************************************/
    /**
    * is it a x509 authentification request ?
    * @return true if x509 auth
    */
    inline bool isX509auth()
    { return clientSockData->peerDN != NULL; }

    /**********************************************************************/
    /**
    * get compression mode
    * @return the compression mode requested
    */   
    inline CompressionMode getCompressionMode()
    {
      return clientSockData->compression;
    };

    /**********************************************************************/
    /**
    * get the http request client socket data 
    * @return the clientSockData
    */       
    ClientSockData *getClientSockData()
    {
      return clientSockData;
    }
};

//****************************************************************************

#endif
