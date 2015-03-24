//********************************************************
/**
 * @file  WebRepository.hh 
 *
 * @brief Web Repository abstract class
 *
 * @author T.Descombes (descombes@lpsc.in2p3.fr)
 *
 * @version 1	
 * @date 27/01/14
 */
//********************************************************

#ifndef WEBREPOSITORY_HH_
#define WEBREPOSITORY_HH_

typedef enum { UNKNOWN_METHOD = 0, GET_METHOD = 1, POST_METHOD = 2 } HttpRequestType;

class WebRepository
{
  public:
    virtual bool getFile(const std::string& url, unsigned char **webpage, size_t *webpageLen, char **respCookies, const HttpRequestType reqType, const char* reqParams="", const char* reqCookies="") = 0;
    virtual void freeFile(unsigned char *webpage) = 0;
};

#endif
