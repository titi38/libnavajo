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

class WebRepository
{
  public:
    virtual bool getFile(const std::string& URL, unsigned char **webpage, size_t *webpageLen, const char* params="", const char* cookies="") = 0;
    virtual void freeFile(unsigned char *webpage) = 0;
};

#endif
