//****************************************************************************
/**
 * @file  WebSocket.hh 
 *
 * @brief The Http WebSocket class
 *
 * @author T.Descombes (descombes@lpsc.in2p3.fr)
 *
 * @version 1	
 * @date 27/01/15
 */
//****************************************************************************

#ifndef WEBSOCKET_HH_
#define WEBSOCKET_HH_

#include "navajo/HttpRequest.hh"
#include "navajo/WebServer.hh"

class WebSocket
{
  public:
    virtual void onMessage(HttpRequest* request, const string &message) = 0;
    void sendMessage(HttpRequest* request, const string &message)
    {
      WebServer::webSocketSend(request, message);
    }
};


#endif

