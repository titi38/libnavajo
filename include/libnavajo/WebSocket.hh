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

#include "libnavajo/WebServer.hh"
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


    /**
    * Send Text Message to all connected clients
    * @param message: the message content
    * @param fin: is-it the final fragment of the message ?
    */
    inline void sendBroadcastTextMessage(const std::string &message, bool fin=true)
    {
      pthread_mutex_lock(&webSocketClientList_mutex);
      for (std::list<WebSocketClient*>::iterator it = webSocketClientList.begin(); it != webSocketClientList.end(); it++)
          (*it)->sendTextMessage(message, fin);
      pthread_mutex_unlock(&webSocketClientList_mutex);
    };

    /**
    * Send Binary Message to all connected clients
    * @param message: the message content
    * @param length: the message length
    * @param fin: is-it the final fragment of the message ?
    */
    inline void sendBroadcastBinaryMessage(const unsigned char *message, size_t length, bool fin = true)
    {
      pthread_mutex_lock(&webSocketClientList_mutex);
      for (std::list<WebSocketClient*>::iterator it = webSocketClientList.begin(); it != webSocketClientList.end(); it++)
        (*it)->sendBinaryMessage(message, length, fin);
      pthread_mutex_unlock(&webSocketClientList_mutex);
    };

    /**
    * Send Close Message Notification to all connected clients
    * @param reasonMsg: the message content
    * @param length: the message length
    */
    inline void sendBroadcastCloseCtrlFrame(const unsigned char *message, size_t length)
    {
      pthread_mutex_lock(&webSocketClientList_mutex);
      for (std::list<WebSocketClient*>::iterator it = webSocketClientList.begin(); it != webSocketClientList.end(); it++)
        (*it)->sendCloseCtrlFrame(message, length);
      pthread_mutex_unlock(&webSocketClientList_mutex);
    };

    /**
    * Send Close Message Notification to all connected clients
    * @param reasonMsg: the message content
    */
    inline void sendBroadcastCloseCtrlFrame(const string &reasonMsg = "")
    {
      sendBroadcastCloseCtrlFrame((const unsigned char*)reasonMsg.c_str(), reasonMsg.length());
    }

    /**
    * Send Ping Message Notification to all connected clients
    * @param message: the message content
    * @param length: the message length
    */
    inline void sendBroadcastPingCtrlFrame(const unsigned char *message, size_t length)
    {
      pthread_mutex_lock(&webSocketClientList_mutex);
      for (std::list<WebSocketClient*>::iterator it = webSocketClientList.begin(); it != webSocketClientList.end(); it++)
        (*it)->sendPingCtrlFrame(message, length);
      pthread_mutex_unlock(&webSocketClientList_mutex);
    };

    /**
    * Send Ping Message Notification to all connected clients
    * @param message: the message content
    */
    inline void sendBroadcastPingCtrlFrame(const string &message)
    {
      sendBroadcastPingCtrlFrame((const unsigned char*)message.c_str(), message.length());
    }

    /**
    * Send Pong Message Notification to all connected clients
    * @param message: the message content
    * @param length: the message length
    */
    inline void sendBroadcastPongCtrlFrame(const unsigned char *message, size_t length)
    {
      pthread_mutex_lock(&webSocketClientList_mutex);
      for (std::list<WebSocketClient*>::iterator it = webSocketClientList.begin(); it != webSocketClientList.end(); it++)
        (*it)->sendPongCtrlFrame(message, length);
      pthread_mutex_unlock(&webSocketClientList_mutex);
    }

    /**
    * Send Pong Message Notification to all connected clients
    * @param message: the message content
    */
    inline void sendBroadcastPongCtrlFrame(const string &message)
    {
      sendBroadcastPongCtrlFrame((const unsigned char*)message.c_str(), message.length());
    }

    /**
    * New Websocket Connection Request
    * create a new websocket client if onOpening() return true
    * @param request: the http request object
    */
    inline void newConnectionRequest(HttpRequest* request)
    {
      pthread_mutex_lock(&webSocketClientList_mutex);
      WebSocketClient* newClient = new WebSocketClient(this, request);
      if (onOpening(newClient))
        webSocketClientList.push_back(newClient);
      else
      {
        delete newClient;
        WebServer::freeClientSockData( request->getClientSockData() );
      }

      pthread_mutex_unlock(&webSocketClientList_mutex);
    };

    /**
    * Remove and close all Websocket client's Connection
    */
    inline void removeAllClients()
    {
      pthread_mutex_lock(&webSocketClientList_mutex);
      for (std::list<WebSocketClient*>::iterator it = webSocketClientList.begin(); it != webSocketClientList.end(); )
      {
        WebSocketClient *client=*it;
        it++;
        client->close(true);
      }
      pthread_mutex_unlock(&webSocketClientList_mutex);
    }

    /**
    * Remove and close a given Websocket client
    * @param client: the websocket client
    * @param cs: are we inside the critical section ?
    */
    inline void removeFromClientsList(WebSocketClient *client, bool cs=false)
    {
      if (!cs) pthread_mutex_lock(&webSocketClientList_mutex);
      std::list<WebSocketClient*>::iterator it = std::find(webSocketClientList.begin(), webSocketClientList.end(), client);
      if (it != webSocketClientList.end())
        webSocketClientList.erase(it);
      if(!cs) pthread_mutex_unlock(&webSocketClientList_mutex);
    }

};

#endif

