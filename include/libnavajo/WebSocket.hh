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

#include "libnavajo/HttpRequest.hh"
#include "libnavajo/WebServer.hh"

class WebSocket
{
  public:
    /**
    * Callback on new websocket client connection
    * @param request: the http request object
    */
    virtual bool onOpening(HttpRequest* request) { return true; };

    /**
    * Callback before closing websocket client connection
    * @param request: the http request object
    */
    virtual void onClosing(HttpRequest* request) { };

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
    * Callback on new pong message (should came after a ping message)
    * @param request: the http request object
    * @param message: the message
    * @param len: the message length
    */     
    virtual void onPongCtrlFrame(HttpRequest* request, const unsigned char* message, size_t len) 
    { /* should check application data received is the same than in the ping message */ };

    /**
    * Callback on new ping message (should came after a ping message)
    * @param request: the http request object
    * @param message: the message
    * @param len: the message length
    * @return true to send an automatic pong reply message
    */     
    virtual bool onPingCtrlFrame(HttpRequest* request, const unsigned char* message, size_t len) 
    { return true; };
    
    /**
    * Callback on client close notification
    * @param request: the http request object
    * @return true to send an automatic close reply message
    */ 
    virtual bool onCloseCtrlFrame(HttpRequest* request, const unsigned char* message, size_t len)
    { return true; };

    /**
    * Send Text Message on the websocket
    * @param request: the http request object
    * @param message: the text message
    * @param fin: is-it the final fragment of the message ?
    */ 
    inline static void sendTextMessage(HttpRequest* request, const string &message, bool fin=true)
    {
      WebServer::webSocketSendTextMessage(request, message, fin);
    };

    /**
    * Send Binary Message on the websocket
    * @param request: the http request object
    * @param message: the content
    * @param length: the message length    
    * @param fin: is-it the final fragment of the message ?
    */ 
    inline static void sendBinaryMessage(HttpRequest* request, const unsigned char* message, size_t length, bool fin=true)
    {
      WebServer::webSocketSendBinaryMessage(request, message, length, fin);
    };
    
    /**
    * Send Close Message Notification on the websocket
    * @param request: the http request object
    * @param message: the closure reason message
    * @param length: the message length
    */ 
    inline static void sendCloseCtrlFrame(HttpRequest* request, const unsigned char* message, size_t length)
    {
      WebServer::webSocketSendCloseCtrlFrame(request, message, length);
    };

    /**
    * Send Close Message Notification on the websocket
    * @param request: the http request object
    * @param message: the closure reason message
    */ 
    inline static void sendCloseCtrlFrame(HttpRequest* request, const string& reasonMsg="")
    {
      WebServer::webSocketSendCloseCtrlFrame(request, reasonMsg);
    };
    
    /**
    * Send Ping Message Notification on the websocket
    * @param request: the http request object
    * @param message: the content
    * @param length: the message length
    */     
    inline static void sendPingCtrlFrame(HttpRequest* request, const unsigned char* message, size_t length)
    {
      WebServer::webSocketSendPingCtrlFrame(request, message, length);
    };

    /**
    * Send Ping Message Notification on the websocket
    * @param request: the http request object
    * @param message: the content
    */     
    inline static void sendPingCtrlFrame(HttpRequest* request, const string &message)
    {
      WebServer::webSocketSendPingCtrlFrame(request, message);
    };

    
    /**
    * Send Pong Message Notification on the websocket
    * @param request: the http request object
    * @param message: the content
    * @param length: the message length
    */     
    inline static void sendPongCtrlFrame(HttpRequest* request, const unsigned char* message, size_t length)
    {
      WebServer::webSocketSendPongCtrlFrame(request, message, length);
    };

    /**
    * Send Pong Message Notification on the websocket
    * @param request: the http request object
    * @param message: the content
    */     
    inline static void sendPongCtrlFrame(HttpRequest* request, const string &message)
    {
      WebServer::webSocketSendPongCtrlFrame(request, message);
    };

};


#endif

