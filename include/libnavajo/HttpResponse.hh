//****************************************************************************
/**
 * @file  HttpResponse.hh 
 *
 * @brief The Http Response Parameters class
 *
 * @author T.Descombes (descombes@lpsc.in2p3.fr)
 *
 * @version 1	
 * @date 27/01/15
 */
//****************************************************************************

#ifndef HTTPRESPONSE_HH_
#define HTTPRESPONSE_HH_


class HttpResponse
{
  unsigned char *responseContent;
  size_t responseContentLength;
  std::vector<std::string> responseCookies;
  bool zippedFile;
  std::string mimeType;
  std::string forwardToUrl;
  bool cors, corsCred;
  std::string corsDomain;
  unsigned httpReturnCode;
  std::string httpReturnCodeMessage;
  std::string httpSpecificHeaders;

  static const unsigned unsetHttpReturnCodeMessage = 0;

  static std::map<unsigned, const char*> httpReturnCodes;

  public:
    HttpResponse(const std::string mime="") : responseContent (NULL), responseContentLength (0), zippedFile (false), mimeType(mime), forwardToUrl(""), cors(false), corsCred(false), corsDomain(""),
                                        httpReturnCode(unsetHttpReturnCodeMessage), httpReturnCodeMessage("Unspecified"), httpSpecificHeaders("")
    {
      initializeHttpReturnCode();
    }
    
    /************************************************************************/
    /**
    * set the response body
    * @param content: The content's buffer
    * @param length: The content's length
    */  
    inline void setContent( unsigned char * const content, const size_t length )
    {
      responseContent = content;
      responseContentLength = length;

      if ( httpReturnCode  == unsetHttpReturnCodeMessage )
      {
        if (length)
          setHttpReturnCode(200);
        else
          setHttpReturnCode(204);
      }
    }
    
    /************************************************************************/
    /**
    * Returns the response body of the HTTP method 
    * @param content: The content's buffer
    * @param length: The content's length
    * @param cookies: The cookies entries
    * @param zip: set to true if the content is compressed (else: false)
    */    
    inline void getContent(unsigned char **content, size_t *length, bool *zip) const
    {
      *content = responseContent;
      *length = responseContentLength;
      *zip = zippedFile;
    }
    
    /************************************************************************/
    /**
    * Set if the content is compressed (zip) or not
    * @param b: true if the content is compressed, false if not.
    */    
    inline void setIsZipped(bool b=true) { zippedFile=b; };

    /************************************************************************/
    /**
    * return true if the content is compressed (zip)
    */
    inline bool isZipped() const { return zippedFile; }; 

    /************************************************************************/
    /**
    * insert a cookie entry (rfc6265) 
    *   format: "<name>=<value>[; <Max-Age>=<age>][; expires=<date>]
    *      [; domain=<domain_name>][; path=<some_path>][; secure][; HttpOnly]"
    * @param name: the cookie's name
    * @param value: the cookie's value
    * @param maxage: the cookie's max-age
    * @param expiresTime: the cookie's expiration date
    * @param path: the cookie's path
    * @param domain: the cookie's domain
    * @param secure: the cookie's secure flag
    * @param httpOnly: the cookie's httpOnly flag
    */  

    inline void addCookie(const std::string& name, const std::string& value, const time_t maxage=0, const time_t expiresTime=0, const std::string& path="/", const std::string& domain="", const bool secure=false, bool httpOnly=false)
    { 
      std::string cookieEntry=name+'='+value;

      if (maxage)
      {
        std::stringstream maxageSs; maxageSs << maxage;
        cookieEntry+="; Max-Age="+maxageSs.str();
      }

      if (expiresTime)
      {
        char expBuf[100];
        struct tm timeinfo;
        gmtime_r ( &expiresTime, &timeinfo );
        strftime (expBuf,100,"%a, %d %b %Y %H:%M:%S GMT", &timeinfo);
        cookieEntry+="; expires="+std::string(expBuf);
      }
      
      if (domain.length()) cookieEntry+="; domain="+domain;
      
      if (path != "/" && path.length()) cookieEntry+="; path="+path;

      if (secure) cookieEntry+="; secure";
            
      if (httpOnly) cookieEntry+="; HttpOnly";
      
      responseCookies.push_back(cookieEntry);
    }
    
    /************************************************************************/
    /**
    * insert the session's cookie
    * @param sid: the session id
    */  
    inline void addSessionCookie(const std::string& sid)
    { 
      addCookie("SID",sid, HttpSession::getSessionLifeTime(), 0, "", "", false,  true);
    }

    /************************************************************************/
    /**
    * get the http response's cookies
    * @return the cookies vector
    */     
    inline std::vector<std::string>& getCookies()
    {
      return responseCookies;
    };
    
    /************************************************************************/
    /**
    * set a new mime type (by default, mime type is automatically set)
    * @param mime: the new mime type
    */
    inline void setMimeType(const std::string& mime)
    {
      mimeType=mime;
    }
    
    /************************************************************************/
    /**
    * get the current mime type
    * @return the mime type
    */
    inline const std::string& getMimeType() const
    {
      return mimeType;
    }

    /************************************************************************/
    /**
    * Request redirection to a new url
    * @param url: the new url
    */    
    void forwardTo(const std::string& url)
    {
      forwardToUrl=url;
    }

    /************************************************************************/
    /**
    * get the new url
    * @return the new url
    */    
    std::string getForwardedUrl() const
    {
      return forwardToUrl;
    }

    /************************************************************************/
    /**
    * allow Cross Site Request
    * @param cors: enabled or not
    * @param cred: enabled credentials or not
    */
    void setCORS(bool cors=true, bool cred=false, std::string domain="*")
    {
      this->cors=cors;
      corsCred=cred;
      corsDomain=domain;
    }

    /**
    * is Cross Site Request allowed ?
    * @return boolean
    */      
    bool isCORS()
    {
      return cors;
    }
    
    bool isCORSwithCredentials()
    {
      return corsCred;
    }
    
    std::string& getCORSdomain()
    {
      return corsDomain;
    };

    /************************************************************************/
    /**
    * set Http Return Code
    * @param value: the http return code
    */

    void setHttpReturnCode(const unsigned value)
    {
      httpReturnCode = value;
      std::map<unsigned, const char*>::iterator it;
      it = httpReturnCodes.find(value);
      if (it != httpReturnCodes.end())
        httpReturnCodeMessage=it->second;
      else
        httpReturnCodeMessage="Unspecified";
    }

    /************************************************************************/
    /**
    * set Http Return Code
    * @param value: the http return code
    * @param message: the http return code message
    */
    void setHttpReturnCode(const unsigned value, const std::string message)
    {
      httpReturnCode = value;
      httpReturnCodeMessage = message;
    }

    /************************************************************************/
    /**
    * generate the http return code string
    */
    std::string getHttpReturnCodeStr()
    {
      if ( httpReturnCode  == unsetHttpReturnCodeMessage )
        setHttpReturnCode(204);

      std::ostringstream httpRetCodeSS;   // stream used for the conversion
      httpRetCodeSS << httpReturnCode;
      return httpRetCodeSS.str()+" "+httpReturnCodeMessage;
    }

    /************************************************************************/
    /**
    * initialize standart Http Return Codes
    * @param value: the http return code
    */
    void initializeHttpReturnCode() const
    {
      if (httpReturnCodes.size())
        return;

      //1xx Informational responses
      httpReturnCodes.insert(std::pair<unsigned, const char*>(100, "Continue"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(101, "Switching Protocols"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(102, "Processing")); //(WebDAV; RFC 2518)

      //2xx Success
      httpReturnCodes.insert(std::pair<unsigned, const char*>(200, "OK"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(201, "Created"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(202, "Accepted"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(203, "Non-Authoritative Information"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(204, "No Content"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(205, "Reset Content"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(206, "Partial Content")); // (RFC 7233)
      httpReturnCodes.insert(std::pair<unsigned, const char*>(207, "Multi-Status")); // (WebDAV; RFC 4918)
      httpReturnCodes.insert(std::pair<unsigned, const char*>(208, "Already Reported")); // (WebDAV; RFC 5842)
      httpReturnCodes.insert(std::pair<unsigned, const char*>(226, "IM Used")); // (RFC 3229)

      //3xx Redirection
      httpReturnCodes.insert(std::pair<unsigned, const char*>(300, "Multiple Choices"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(301, "Moved Permanently"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(302, "Found"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(303, "See Other"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(304, "Not Modified")); // (RFC 7232)
      httpReturnCodes.insert(std::pair<unsigned, const char*>(305, "Use Proxy"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(306, "Switch Proxy"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(307, "Temporary Redirect"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(308, "Permanent Redirect")); // (RFC 7538)

      //4xx Client errors
      httpReturnCodes.insert(std::pair<unsigned, const char*>(400, "Bad Request"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(401, "Unauthorized")); // (RFC 7235)
      httpReturnCodes.insert(std::pair<unsigned, const char*>(402, "Payment Required"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(403, "Forbidden"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(404, "Not Found"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(405, "Method Not Allowed"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(406, "Not Acceptable"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(407, "Proxy Authentication Required")); // (RFC 7235)
      httpReturnCodes.insert(std::pair<unsigned, const char*>(408, "Request Timeout"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(409, "Conflict"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(410, "Gone"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(411, "Length Required"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(412, "Precondition Failed")); // (RFC 7232)
      httpReturnCodes.insert(std::pair<unsigned, const char*>(413, "Payload Too Large")); // (RFC 7231)
      httpReturnCodes.insert(std::pair<unsigned, const char*>(414, "URI Too Long")); //(RFC 7231)
      httpReturnCodes.insert(std::pair<unsigned, const char*>(415, "Unsupported Media Type"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(416, "Range Not Satisfiable")); // (RFC 7233)
      httpReturnCodes.insert(std::pair<unsigned, const char*>(417, "Expectation Failed"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(418, "I'm a teapot")); // (RFC 2324)
      httpReturnCodes.insert(std::pair<unsigned, const char*>(421, "Misdirected Request")); // (RFC 7540)
      httpReturnCodes.insert(std::pair<unsigned, const char*>(422, "Unprocessable Entity")); // (WebDAV; RFC 4918)
      httpReturnCodes.insert(std::pair<unsigned, const char*>(423, "Locked")); // (WebDAV; RFC 4918)
      httpReturnCodes.insert(std::pair<unsigned, const char*>(424, "Failed Dependency")); // (WebDAV; RFC 4918)
      httpReturnCodes.insert(std::pair<unsigned, const char*>(426, "Upgrade Required"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(428, "Precondition Required")); // (RFC 6585)
      httpReturnCodes.insert(std::pair<unsigned, const char*>(429, "Too Many Requests")); //(RFC 6585)
      httpReturnCodes.insert(std::pair<unsigned, const char*>(431, "Request Header Fields Too Large")); // (RFC 6585)
      httpReturnCodes.insert(std::pair<unsigned, const char*>(451, "Unavailable For Legal Reasons")); // (RFC 7725)

      //5xx Server error[edit]
      httpReturnCodes.insert(std::pair<unsigned, const char*>(500, "Internal Server Error"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(501, "Not Implemented"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(502, "Bad Gateway"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(503, "Service Unavailable"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(504, "Gateway Timeout"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(505, "HTTP Version Not Supported"));
      httpReturnCodes.insert(std::pair<unsigned, const char*>(506, "Variant Also Negotiates")); // (RFC 2295)
      httpReturnCodes.insert(std::pair<unsigned, const char*>(507, "Insufficient Storage")); // (WebDAV; RFC 4918)
      httpReturnCodes.insert(std::pair<unsigned, const char*>(508, "Loop Detected")); // (WebDAV; RFC 5842)
      httpReturnCodes.insert(std::pair<unsigned, const char*>(510, "Not Extended")); // (RFC 2774)
      httpReturnCodes.insert(std::pair<unsigned, const char*>(511, "Network Authentication Required")); // (RFC 6585)

    }

    void addSpecificHeader(const char* header)
    {
        httpSpecificHeaders += header;
        httpSpecificHeaders += "\r\n";
    }

    void addSpecificHeader(const std::string& header)
    {
        httpSpecificHeaders += header;
        httpSpecificHeaders += "\r\n";
    }

    std::string getSpecificHeaders() const
    {
        return httpSpecificHeaders;
    }
};



//****************************************************************************

#endif
