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

#ifndef HTTPRESPONSE_HH_
#define HTTPRESPONSE_HH_


class HttpResponse
{
  unsigned char *responseContent;
  size_t responseContentLength;
  std::vector<std::string> responseCookies;
  bool zippedFile;

  public:
    HttpResponse() : responseContent (NULL), responseContentLength (0), zippedFile (false)
    {
    }
    
    inline void setContent(unsigned char *content, size_t length)
    {
      responseContent = content;
      responseContentLength = length;
    }

    inline void getResponse(unsigned char **content, size_t *length, std::vector<std::string> *cookies, bool *zip)
    {
      *content = responseContent;
      *length = responseContentLength;
      *cookies = responseCookies;
      *zip = zippedFile;
    }
    
    inline void setIsZipped(bool b=true) { zippedFile=b; };
    inline bool isZipped() { return zippedFile; };
    
    inline void addSessionCookie(const std::string& sid)
    { 
      responseCookies.push_back("SID="+sid);
    }
};



//****************************************************************************

#endif
