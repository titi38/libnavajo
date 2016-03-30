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

#include <algorithm>
#include <list>
#include "libnavajo/HttpRequest.hh"


class WebSocket
{
    std::list<int> webSocketClientList;
    pthread_mutex_t webSocketClientList_mutex;

    list<HttpRequest*> wsclients;
    pthread_mutex_t wsclients_mutex;

    static void webSocketSend(HttpRequest* request, const u_int8_t opcode, const unsigned char* message, size_t length, bool fin);
    static void webSocketSendTextMessage(HttpRequest* request, const string &message, bool fin=true);
    static void webSocketSendBinaryMessage(HttpRequest* request, const unsigned char* message, size_t length, bool fin=true);
    static void webSocketSendPingCtrlFrame(HttpRequest* request, const unsigned char* message, size_t length);
    static void webSocketSendPingCtrlFrame(HttpRequest* request, const string &message);
    static void webSocketSendPongCtrlFrame(HttpRequest* request, const unsigned char* message, size_t length);
    static void webSocketSendPongCtrlFrame(HttpRequest* request, const string &message);
    static void webSocketSendCloseCtrlFrame(HttpRequest* request, const unsigned char* message, size_t length);
    static void webSocketSendCloseCtrlFrame(HttpRequest* request, const string &message="");

//    inline void freeClientSockData(ClientSockData *c) { c); };

  public:
    WebSocket()
    {
      pthread_mutex_init(&wsclients_mutex, NULL);
      //  pthread_mutex_init(&webSocketClientList_mutex, NULL);
    }

    ~WebSocket()
    {
      // Free all request
    }

    void listenWebSocket(WebSocket *websocket, HttpRequest* request);

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
    virtual void onTextMessage(HttpRequest* request, const string &message, const bool fin)
    { };


    /**
    * Callback on new binary message
    * @param request: the http request object
    * @param message: the binary message
    * @param len: the message length
    * @param fin: is the current message finished ?
    */ 
    virtual void onBinaryMessage(HttpRequest* request, const unsigned char* message, size_t len, const bool fin)
    { };
    
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
      webSocketSendTextMessage(request, message, fin);
    };

    inline void sendBroadcastTextMessage(const string &message, bool fin=true)
    {
      pthread_mutex_lock(&wsclients_mutex);

      for (std::list<HttpRequest*>::iterator it = wsclients.begin(); it != wsclients.end(); it++)
        webSocketSendTextMessage(*it, message, fin);

      pthread_mutex_unlock(&wsclients_mutex);
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
      webSocketSendBinaryMessage(request, message, length, fin);
    };
    
    /**
    * Send Close Message Notification on the websocket
    * @param request: the http request object
    * @param message: the closure reason message
    * @param length: the message length
    */ 
    inline static void sendCloseCtrlFrame(HttpRequest* request, const unsigned char* message, size_t length)
    {
      webSocketSendCloseCtrlFrame(request, message, length);
    };

    /**
    * Send Close Message Notification on the websocket
    * @param request: the http request object
    * @param message: the closure reason message
    */ 
    inline static void sendCloseCtrlFrame(HttpRequest* request, const string& reasonMsg="")
    {
      webSocketSendCloseCtrlFrame(request, reasonMsg);
    };
    
    /**
    * Send Ping Message Notification on the websocket
    * @param request: the http request object
    * @param message: the content
    * @param length: the message length
    */     
    inline static void sendPingCtrlFrame(HttpRequest* request, const unsigned char* message, size_t length)
    {
      webSocketSendPingCtrlFrame(request, message, length);
    };

    /**
    * Send Ping Message Notification on the websocket
    * @param request: the http request object
    * @param message: the content
    */     
    inline static void sendPingCtrlFrame(HttpRequest* request, const string &message)
    {
      webSocketSendPingCtrlFrame(request, message);
    };

    
    /**
    * Send Pong Message Notification on the websocket
    * @param request: the http request object
    * @param message: the content
    * @param length: the message length
    */     
    inline static void sendPongCtrlFrame(HttpRequest* request, const unsigned char* message, size_t length)
    {
      webSocketSendPongCtrlFrame(request, message, length);
    };

    /**
    * Send Pong Message Notification on the websocket
    * @param request: the http request object
    * @param message: the content
    */     
    inline static void sendPongCtrlFrame(HttpRequest* request, const string &message)
    {
      webSocketSendPongCtrlFrame(request, message);
    };

    inline void addNewClient(HttpRequest* request)
    {
      pthread_mutex_lock(&wsclients_mutex);
      wsclients.push_back(request);
      pthread_mutex_unlock(&wsclients_mutex);
    };

    inline void removeClient(HttpRequest* request)
    {
      pthread_mutex_lock(&wsclients_mutex);
      std::list<HttpRequest*>::iterator it = std::find(wsclients.begin(), wsclients.end(), request);
      if (it != wsclients.end())
      {
        delete *it; *it = NULL;
        wsclients.erase(it);
      }
      pthread_mutex_unlock(&wsclients_mutex);
    };
    
    inline bool isClient(HttpRequest* request)
    {
      bool res;
      pthread_mutex_lock(&wsclients_mutex);
      std::list<HttpRequest*>::iterator it = std::find(wsclients.begin(), wsclients.end(), request);
      res = it != wsclients.end();
      pthread_mutex_unlock(&wsclients_mutex);
      return res;
    };

};

#endif

