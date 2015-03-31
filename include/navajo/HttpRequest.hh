//****************************************************************************
/**
 * @file  HttpRequest.hh 
 *
 * @brief Contains Http Request Parameters
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

#include "HttpSession.hh"

//****************************************************************************

typedef enum { UNKNOWN_METHOD = 0, GET_METHOD = 1, POST_METHOD = 2 } HttpRequestType;


class HttpRequest
{
  typedef std::map <std::string, std::string> HttpRequestParametersMap;
  typedef std::map <std::string, std::string> HttpRequestCookiesMap;  

  const char *urlRequest;
  HttpRequestType typeRequest;
  HttpRequestCookiesMap cookies;
  HttpRequestParametersMap parameters;
  std::string sessionId;

  /**********************************************************************/

  inline void decodParams( const std::string& p )
  {
    size_t start = 0, end = 0;
    std::string paramstr=p;

    while ((end = paramstr.find_first_of("%+", start)) != std::string::npos) 
    {
      switch (paramstr[end])
      {
        case '%':
          if (paramstr[end+1]=='%')
            paramstr=paramstr.erase(end+1,1);
          else
          {
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
      else
        parameters[theParam.substr(0,posEq)]=theParam.substr(posEq+1);
       
      start = end + 1;
    }
  };

  /**********************************************************************/

  inline void decodCookies( const std::string& c )
  {
    std::stringstream ss(c);
    std::string theCookie;
    while (std::getline(ss, theCookie, ';')) 
    {
      size_t posEq=0;
      if ((posEq = theCookie.find('=')) != std::string::npos)
      {
        size_t firstC=0; while (!iswgraph(theCookie[firstC]) && firstC < theCookie.length()-1) { firstC++; };
        size_t lenC=0; while (iswgraph(theCookie[posEq+1+lenC]) && (posEq+1+lenC<theCookie.length()-1)) { lenC++; };
        cookies[theCookie.substr(firstC,posEq)]=theCookie.substr(posEq+1, lenC);        
      }
    }
  };


  public:
  
    /**********************************************************************/

    inline std::string getCookie( const std::string& name ) const
    {
      std::string res="";
      getCookie(name, res);
      return res;
    }
    
    /**********************************************************************/
  
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

    inline std::vector<std::string> getCookiesNames() const
    {
      std::vector<std::string> res;
      for(HttpRequestCookiesMap::const_iterator iter=cookies.begin(); iter!=cookies.end(); ++iter)
       res.push_back(iter->first);
      return res;
    }

    /**********************************************************************/
  
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

    inline std::string getParameter( const std::string& name ) const
    {
      std::string res="";
      getParameter(name, res);
      return res;
    }

    /**********************************************************************/

    inline std::vector<std::string> getParameterNames() const
    {
      std::vector<std::string> res;
      for(HttpRequestParametersMap::const_iterator iter=parameters.begin(); iter!=parameters.end(); ++iter)
       res.push_back(iter->first);
      return res;
    }

    /**********************************************************************/

    inline void getSession(bool b = true)
    {
      sessionId = getCookie("SID");
      
      if (sessionId.length() && HttpSession::find(sessionId))
        return;

      if (b)
         HttpSession::create(sessionId);
    }

    inline void removeSession()
    {
      if (sessionId == "") getSession();
      HttpSession::remove(sessionId);
    }

    void setSessionAttribute ( const std::string &name, void* value )
    {
      if (sessionId == "") getSession();
      HttpSession::setAttribute(sessionId, name, value);
    }
      
    void *getSessionAttribute( const std::string &name )
    {
      if (sessionId == "") getSession();
      return HttpSession::getAttribute(sessionId, name);
    }

    inline std::vector<std::string> getSessionAttributeNames()
    {
      if (sessionId == "") getSession();
      return HttpSession::getAttributeNames(sessionId);
    }

    inline void getSessionRemoveAttribute( const std::string &name )
    {
      if (sessionId == "") getSession();
      HttpSession::removeAttribute( sessionId, name );
    }

    inline void initSessionId() { sessionId = ""; };
    std::string getSessionId() const { return sessionId; };

    /**********************************************************************/
        
    HttpRequest(const HttpRequestType type, const char *url, const char *params, const char *cookies) 
    { 
      typeRequest = type;
      urlRequest = url;
      if (params != NULL && strlen(params))
        decodParams(params);
      if (cookies != NULL && strlen(cookies))
        decodCookies(cookies);        
      initSessionId();
    };
    
    inline const char *getUrl() const { return urlRequest; };
    inline HttpRequestType getRequestType() const { return typeRequest; };
    
};

//****************************************************************************


#endif
