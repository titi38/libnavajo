//****************************************************************************
/**
 * @file  WebRepository.hh 
 *
 * @brief Web Repository handler (abstract class)
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

    /**
    * Try to resolve an http request by requesting the webrepository
    * called from WebServer::accept_request() method
    * @param request: a pointer to the current request
    * @param response: a pointer to the new generated response
    * \return true if the repository contains the requested resource
    */
    virtual bool getFile(HttpRequest* request, HttpResponse *response) = 0;

    /**
    * Free resources after use.
    * called from WebServer::accept_request() method
    * @param webpage: a pointer to the generated page
    */
    virtual void freeFile(unsigned char *webpage) = 0;

};

#endif
