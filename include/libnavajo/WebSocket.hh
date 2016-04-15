//****************************************************************************
/**
 * @file  WebSocket.hh 
 *
 * @brief The Http WebSocket class
 *
 * @author T.Descombes (descombes@lpsc.in2p3.fr)
 *
 * @version 1	
 * @date 27/01/16
 */
//****************************************************************************

#ifndef WEBSOCKET_HH_
#define WEBSOCKET_HH_

#include <algorithm>
#include <list>
#include "libnavajo/HttpRequest.hh"
#include "libnavajo/nvjThread.h"

#include <queue>


class WebSocket
{
    typedef struct
    {
      u_int8_t opcode;
      unsigned char* message;
      size_t length;
      bool fin;
    } MessageContent;

    class WebSocketClient
    {
        std::queue<MessageContent *> sendingQueue;
        pthread_mutex_t sendingQueueMutex;
        pthread_cond_t sendingNotification;
        void addSendingQueue(MessageContent *msgContent);

        WebSocket *websocket;
        HttpRequest *request;
        volatile bool closing;
        pthread_t receivingThreadId, sendingThreadId;

        void receivingThread();
        void sendingThread();

        void sendMessage(const MessageContent *msg);

        inline void waitingThreadsExit()
        {
          void *status;
          int rc = pthread_join(receivingThreadId, &status);
          rc = pthread_join(sendingThreadId, &status);
        };

        inline static void* startReceivingThread(void* t)
        {
          WebSocketClient *_this=static_cast<WebSocketClient *>(t);
          _this->receivingThread();
          pthread_exit(NULL);
          return NULL;
        };

        inline static void* startSendingThread(void* t)
        {
          WebSocketClient *_this=static_cast<WebSocketClient *>(t);
          _this->sendingThread();
          pthread_exit(NULL);
          return NULL;
        };

        void startWebSocketListener()
        {
          create_thread( &receivingThreadId, WebSocketClient::startReceivingThread, static_cast<void *>(this) );
          create_thread( &sendingThreadId,   WebSocketClient::startSendingThread, static_cast<void *>(this) );
        }

      public:
        WebSocketClient(::WebSocket *ws, HttpRequest *req) : websocket(ws), request(req), closing(false)
        {
          pthread_mutex_init(&sendingQueueMutex, NULL);
          pthread_cond_init(&sendingNotification, NULL);
          startWebSocketListener();
        };

        /**
        * Send Text Message on the websocket
        * @param request: the http request object
        * @param message: the text message
        * @param fin: is-it the final fragment of the message ?
        */
        void sendTextMessage(const string &message, bool fin = true);

        /**
        * Send Binary Message on the websocket
        * @param request: the http request object
        * @param message: the content
        * @param length: the message length
        * @param fin: is-it the final fragment of the message ?
        */
        void sendBinaryMessage(const unsigned char *message, size_t length, bool fin = true);

        /**
        * Send Close Message Notification on the websocket
        * @param request: the http request object
        * @param message: the closure reason message
        * @param length: the message length
        */
        void sendCloseCtrlFrame(const unsigned char *message, size_t length);

        /**
        * Send Close Message Notification on the websocket
        * @param request: the http request object
        * @param message: the closure reason message
        */
        void sendCloseCtrlFrame(const string &reasonMsg = "");

        /**
        * Send Ping Message Notification on the websocket
        * @param request: the http request object
        * @param message: the content
        * @param length: the message length
        */
        void sendPingCtrlFrame(const unsigned char *message, size_t length);

        /**
        * Send Ping Message Notification on the websocket
        * @param request: the http request object
        * @param message: the content
        */
        void sendPingCtrlFrame(const string &message);

        /**
        * Send Pong Message Notification on the websocket
        * @param request: the http request object
        * @param message: the content
        * @param length: the message length
        */
        inline void sendPongCtrlFrame(const unsigned char *message, size_t length);

        /**
        * Send Pong Message Notification on the websocket
        * @param request: the http request object
        * @param message: the content
        */
        inline void sendPongCtrlFrame(const string &message);

        HttpRequest *getHttpRequest() { return request; };

        void close();
    };

    list<WebSocketClient*> wsclients;
    pthread_mutex_t wsclients_mutex;

  public:
    WebSocket()
    {
      pthread_mutex_init(&wsclients_mutex, NULL);
      //  pthread_mutex_init(&webSocketClientList_mutex, NULL);
    }

    ~WebSocket()
    {
      // Free all request, close all sockets
      removeAllClients();
    }

    /**
    * Callback on new websocket client connection
    * @param request: the http request object
    */
    virtual bool onOpening(WebSocketClient* client) { return true; };

    /**
    * Callback before closing websocket client connection
    * @param request: the http request object
    */
    virtual void onClosing(WebSocketClient* client) { };

    /**
    * Callback on new text message
    * @param request: the http request object
    * @param message: the message
    * @param fin: is the current message finished ?
    */ 
    virtual void onTextMessage(WebSocketClient* client, const string &message, const bool fin)
    { };

    /**
    * Callback on new binary message
    * @param request: the http request object
    * @param message: the binary message
    * @param len: the message length
    * @param fin: is the current message finished ?
    */ 
    virtual void onBinaryMessage(WebSocketClient* client, const unsigned char* message, size_t len, const bool fin)
    { };
    
    /**
    * Callback on new pong message (should came after a ping message)
    * @param request: the http request object
    * @param message: the message
    * @param len: the message length
    */     
    virtual void onPongCtrlFrame(WebSocketClient* client, const unsigned char* message, size_t len)
    { /* should check application data received is the same than in the ping message */ };

    /**
    * Callback on new ping message (should came after a ping message)
    * @param request: the http request object
    * @param message: the message
    * @param len: the message length
    * @return true to send an automatic pong reply message
    */     
    virtual bool onPingCtrlFrame(WebSocketClient* client, const unsigned char* message, size_t len)
    { return true; };
    
    /**
    * Callback on client close notification
    * @param request: the http request object
    * @return true to send an automatic close reply message
    */ 
    virtual bool onCloseCtrlFrame(WebSocketClient* client, const unsigned char* message, size_t len)
    { return true; };

    inline void sendBroadcastTextMessage(const string &message, bool fin=true)
    {
      pthread_mutex_lock(&wsclients_mutex);
      for (std::list<WebSocketClient*>::iterator it = wsclients.begin(); it != wsclients.end(); it++)
          (*it)->sendTextMessage(message, fin);
      pthread_mutex_unlock(&wsclients_mutex);
    };

    inline void newConnectionRequest(HttpRequest* request)
    {
      pthread_mutex_lock(&wsclients_mutex);
      WebSocketClient* newClient = new WebSocketClient(this, request);
      if (onOpening(newClient))
        wsclients.push_back(newClient);
      else
        delete newClient;
      pthread_mutex_unlock(&wsclients_mutex);
    };

    inline void removeAllClients()
    {
      pthread_mutex_lock(&wsclients_mutex);
      for (std::list<WebSocketClient*>::iterator it = wsclients.begin(); it != wsclients.end(); it++)
      {
        (*it)->close();
        wsclients.erase(it);
      }
    }

    inline void removeFromClientsList(WebSocketClient *client)
    {
      pthread_mutex_lock(&wsclients_mutex);
      std::list<WebSocketClient*>::iterator it = std::find(wsclients.begin(), wsclients.end(), client);
      if (it != wsclients.end())
        wsclients.erase(it);
      pthread_mutex_unlock(&wsclients_mutex);
    }

    /*
    inline bool isClient(HttpRequest* request)
    {
      bool res;
      pthread_mutex_lock(&wsclients_mutex);
      std::list<WebSocketClient*>::iterator it = std::find(wsclients.begin(), wsclients.end(), request);
      res = it != wsclients.end();
      pthread_mutex_unlock(&wsclients_mutex);
      return res;
    };

    inline void removeClient(HttpRequest* request)
    {
      pthread_mutex_lock(&wsclients_mutex);
      std::list<WebSocketClient*>::iterator it = std::find(wsclients.begin(), wsclients.end(), request);
      if (it != wsclients.end())
      {
        delete *it; *it = NULL;
        wsclients.erase(it);
      }
      pthread_mutex_unlock(&wsclients_mutex);
    };
    */

};

#endif

