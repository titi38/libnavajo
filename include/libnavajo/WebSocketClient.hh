//****************************************************************************
/**
 * @file  WebSocketClient.hh
 *
 * @brief The WebSocket Client class
 *
 * @author T.Descombes (descombes@lpsc.in2p3.fr)
 *
 * @version 1
 * @date 15/04/2016
 */
//****************************************************************************

#ifndef WEBSOCKETCLIENT_HH_
#define WEBSOCKETCLIENT_HH_

#include <queue>

#include "libnavajo/HttpRequest.hh"
#include "libnavajo/nvjThread.h"

class WebSocket;
class WebSocketClient
{
    typedef struct
    {
      u_int8_t opcode;
      unsigned char* message;
      size_t length;
      bool fin;
    } MessageContent;

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
    WebSocketClient(WebSocket *ws, HttpRequest *req) : websocket(ws), request(req), closing(false)
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

    void close(bool cs = false);
};

#endif //WEBSOCKETCLIENT_HH_
