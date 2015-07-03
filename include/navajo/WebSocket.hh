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
    virtual bool onOpen(HttpRequest* request) { return true; };
    virtual void onTextMessage(HttpRequest* request, const string &message, const bool fin) = 0;
    virtual void onBinaryMessage(HttpRequest* request, const unsigned char* message, size_t len, const bool fin) = 0;
    virtual void onClose(HttpRequest* request) { };

    inline static void sendTextMessage(HttpRequest* request, const string &message, bool fin=true)
    {
      WebServer::webSocketSendTextMessage(request, message, fin);
    };
    inline static void sendBinaryMessage(HttpRequest* request, const unsigned char* message, size_t length, bool fin=true)
    {
      WebServer::webSocketSendBinaryMessage(request, message, length, fin);
    };
    inline static void sendClose(HttpRequest* request, const string reasonMsg="")
    {
      WebServer::webSocketSendClose(request, reasonMsg);
    };
    inline static void sendPing(HttpRequest* request, const unsigned char* message, size_t length)
    {
      WebServer::webSocketSendPingMessage(request, message, length);
    };
    inline static void sendPing(HttpRequest* request, const string &message)
    {
      WebServer::webSocketSendPingMessage(request, message);
    };

};


#endif

