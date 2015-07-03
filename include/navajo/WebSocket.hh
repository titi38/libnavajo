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
    /**
    * Callback on new websocket client connection
    * @param request: the http request object
    */
    virtual bool onOpen(HttpRequest* request) { return true; };

    /**
    * Callback on new text message
    * @param request: the http request object
    * @param message: the message
    * @param fin: is the current message finished ?
    */ 
    virtual void onTextMessage(HttpRequest* request, const string &message, const bool fin) = 0;

    /**
    * Callback on new binary message
    * @param request: the http request object
    * @param message: the binary message
    * @param len: the message length
    * @param fin: is the current message finished ?
    */ 
    virtual void onBinaryMessage(HttpRequest* request, const unsigned char* message, size_t len, const bool fin) = 0;
    
    /**
    * Callback on client close notification
    * @param request: the http request object
    */ 
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

