//****************************************************************************
/**
 * @file  WebRepository.hh 
 *
 * @brief Web Repository abstract class
 *
 * @author T.Descombes (descombes@lpsc.in2p3.fr)
 *
 * @version 1	
 * @date 27/01/15
 */
//****************************************************************************

#ifndef WEBREPOSITORY_HH_
#define WEBREPOSITORY_HH_

#include "HttpRequest.hh"
#include "HttpResponse.hh"


class WebRepository
{
  public:
    virtual bool getFile(HttpRequest* request, HttpResponse *response) = 0;
    virtual void freeFile(unsigned char *webpage) = 0;
};

#endif
