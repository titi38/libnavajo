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
#include <string>

#include "libnavajo/WebSocketClient.hh"

class WebSocket
{
    list<WebSocketClient*> webSocketClientList;
    pthread_mutex_t webSocketClientList_mutex;

  public:
    WebSocket()
    {
      pthread_mutex_init(&webSocketClientList_mutex, NULL);
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
    virtual void onTextMessage(WebSocketClient* client, const std::string &message, const bool fin)
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

    inline void sendBroadcastTextMessage(const std::string &message, bool fin=true)
    {
      pthread_mutex_lock(&webSocketClientList_mutex);
      for (std::list<WebSocketClient*>::iterator it = webSocketClientList.begin(); it != webSocketClientList.end(); it++)
          (*it)->sendTextMessage(message, fin);
      pthread_mutex_unlock(&webSocketClientList_mutex);
    };

    inline void newConnectionRequest(HttpRequest* request)
    {
      pthread_mutex_lock(&webSocketClientList_mutex);
      WebSocketClient* newClient = new WebSocketClient(this, request);
      if (onOpening(newClient))
        webSocketClientList.push_back(newClient);
      else
        delete newClient;
      pthread_mutex_unlock(&webSocketClientList_mutex);
    };

    inline void removeAllClients()
    {
      pthread_mutex_lock(&webSocketClientList_mutex);
      for (std::list<WebSocketClient*>::iterator it = webSocketClientList.begin(); it != webSocketClientList.end(); it++)
      {
        (*it)->close();
        webSocketClientList.erase(it);
      }
    }

    inline void removeFromClientsList(WebSocketClient *client)
    {
      pthread_mutex_lock(&webSocketClientList_mutex);
      std::list<WebSocketClient*>::iterator it = std::find(webSocketClientList.begin(), webSocketClientList.end(), client);
      if (it != webSocketClientList.end())
        webSocketClientList.erase(it);
      pthread_mutex_unlock(&webSocketClientList_mutex);
    }

    /*
    inline bool isClient(HttpRequest* request)
    {
      bool res;
      pthread_mutex_lock(&webSocketClientList_mutex);
      std::list<WebSocketClient*>::iterator it = std::find(webSocketClientList.begin(), webSocketClientList.end(), request);
      res = it != webSocketClientList.end();
      pthread_mutex_unlock(&webSocketClientList_mutex);
      return res;
    };

    inline void removeClient(HttpRequest* request)
    {
      pthread_mutex_lock(&webSocketClientList_mutex);
      std::list<WebSocketClient*>::iterator it = std::find(webSocketClientList.begin(), webSocketClientList.end(), request);
      if (it != webSocketClientList.end())
      {
        delete *it; *it = NULL;
        webSocketClientList.erase(it);
      }
      pthread_mutex_unlock(&webSocketClientList_mutex);
    };
    */

};

#endif

