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

// Max latency allowed is fixed to 500ms by default
#define CLIENTSENDING_MAXLATENCY_DEFAULT 500
#define DEFAULT_WEBSOCKET_TIMEOUT 750

class WebSocket
{
    std::list<WebSocketClient*> webSocketClientList;
    pthread_mutex_t webSocketClientList_mutex;
    bool useCompression;
    bool useNaggleAlgo;
    unsigned short clientSending_maxLatency;
    ushort websocketTimeoutInMilliSecond;

  public:
    WebSocket(bool compression=true): useCompression(compression), useNaggleAlgo(true),
                                      clientSending_maxLatency(CLIENTSENDING_MAXLATENCY_DEFAULT),
                                      websocketTimeoutInMilliSecond(DEFAULT_WEBSOCKET_TIMEOUT)

    {
      pthread_mutex_init(&webSocketClientList_mutex, NULL);
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
    virtual bool onOpening(HttpRequest* request) { return true; };

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
    inline void sendBroadcastCloseCtrlFrame(const std::string &reasonMsg = "")
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
    inline void sendBroadcastPingCtrlFrame(const std::string &message)
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
    inline void sendBroadcastPongCtrlFrame(const std::string &message)
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

      if (onOpening(request))
        webSocketClientList.push_back(new WebSocketClient(this, request));
      else
        WebServer::freeClientSockData( request->getClientSockData() );

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
        client->closeWS();
      }
      pthread_mutex_unlock(&webSocketClientList_mutex);
    }

    /**
    * Remove and close a given Websocket client
    * @param client: the websocket client
    * @param cs: are we inside the critical section ?
    */
    inline void removeClient(WebSocketClient *client, bool cs=false)
    {
      if (!cs) pthread_mutex_lock(&webSocketClientList_mutex);
      std::list<WebSocketClient*>::iterator it = std::find(webSocketClientList.begin(), webSocketClientList.end(), client);
      if (it != webSocketClientList.end())
        webSocketClientList.erase(it);
      if(!cs) pthread_mutex_unlock(&webSocketClientList_mutex);
    }

    /**
    * Remove and close a given Websocket client
    * @param request: the related http request object
    */
    inline void removeClient(HttpRequest *request)
    {
      pthread_mutex_lock(&webSocketClientList_mutex);
      for (std::list<WebSocketClient*>::iterator it = webSocketClientList.begin(); it != webSocketClientList.end(); )
      {
        WebSocketClient *client=*it;
        it++;
        if (client->getHttpRequest() == request)
        {
          client->closeWS();
          break;
        }
      }
      pthread_mutex_unlock(&webSocketClientList_mutex);
    }

    /**
    * Get the compression behavior: data compression allowed or not
    *  @return true if compression is allowed
    */
    inline bool isUsingCompression()
    {
      return useCompression;
    }

    /**
    * Set if client should compress data (if it's supported by browser) or not
    * @param compression: true to compress
    */
    inline void setUseCompression(bool compression)
    {
      useCompression = compression;
    }

    /**
    * Get if client socket behavior: Naggle Algorithm is used or not
    * @return true if enabled
    */
    inline bool isUsingNaggleAlgo()
    {
      return useNaggleAlgo;
    }

    /**
    * Set if client socket should use the Naggle Algorithm
    * @param naggle: true if enabled
    */
    inline void setUseNaggleAlgo(bool naggle)
    {
      useNaggleAlgo=naggle;
    }

    /**
    * Get the maximum latency allowed to send data
    * @return the latency in milliseconds
    */
    inline unsigned short getClientSendingMaxLatency()
    {
      return clientSending_maxLatency;
    }

    /**
    * Set the maximum latency allowed to the clients to send data
    * @param ms: the latency in milliseconds
    */
    inline void setClientSendingMaxLatency(unsigned short ms)
    {
      clientSending_maxLatency=ms;
    }

    /**
    * Get the websocket timeout value in milliseconds
    * @return the value in milliseconds
    */
    inline ushort getWebsocketTimeoutInMilliSecond()
    {
      return websocketTimeoutInMilliSecond;
    }

    /**
    * Get the websocket timeout value in milliseconds
    * @param ms: the value in milliseconds
    */
    inline void setWebsocketTimeoutInMilliSecond(unsigned short ms)
    {
      websocketTimeoutInMilliSecond=ms;
    }

};

#endif

