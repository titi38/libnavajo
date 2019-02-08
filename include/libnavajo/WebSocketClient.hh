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
#include <sys/timeb.h>
#include "libnavajo/HttpRequest.hh"
#include "libnavajo/nvjThread.h"
#include "libnavajo/nvjGzip.h"

class WebSocket;
class WebSocketClient
{
    typedef struct
    {
      u_int8_t opcode;
      unsigned char* message;
      size_t length;
      bool fin;
      timeb date;
    } MessageContent;

    typedef struct
    {
      unsigned char z_dictionary_inflate[32768];
      unsigned int dictInfLength;
      z_stream strm_deflate;
    } GzipContext;

    GzipContext gzipcontext;
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

    bool sendMessage(const MessageContent *msg);

    void updateSessionExpiration(HttpRequest *request);

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

    void startWebSocketThreads()
    {
      create_thread( &receivingThreadId, WebSocketClient::startReceivingThread, static_cast<void *>(this) );
      create_thread( &sendingThreadId,   WebSocketClient::startSendingThread,   static_cast<void *>(this) );
    }

    void freeSendingQueue()
    {
      while (!sendingQueue.empty())
      {
        MessageContent *msg = sendingQueue.front();
        free(msg->message);
        free(msg);
        sendingQueue.pop();
      }
    }

    unsigned short snd_maxLatency;

    void noSessionExpiration(HttpRequest *request);
    void restoreSessionExpiration(HttpRequest *request);


  public:
    WebSocketClient(WebSocket *ws, HttpRequest *req);

    ~WebSocketClient()
    {
      pthread_mutex_lock(&sendingQueueMutex);
      freeSendingQueue();
      pthread_mutex_unlock(&sendingQueueMutex);
      nvj_end_stream(&(gzipcontext.strm_deflate));
      pthread_mutex_destroy(&sendingQueueMutex);
      pthread_cond_destroy(&sendingNotification);
    }

    /**
    * Send Text Message on the websocket
    * @param message: the text message
    * @param fin: is-it the final fragment of the message ?
    */
    void sendTextMessage(const std::string &message, bool fin = true);

    /**
    * Send Binary Message on the websocket
    * @param message: the content
    * @param length: the message length
    * @param fin: is-it the final fragment of the message ?
    */
    void sendBinaryMessage(const unsigned char *message, size_t length, bool fin = true);

    /**
    * Send Close Message Notification on the websocket
    * @param message: the closure reason message
    * @param length: the message length
    */
    void sendCloseCtrlFrame(const unsigned char *message, size_t length);

    /**
    * Send Close Message Notification on the websocket
    * @param message: the closure reason message
    */
    void sendCloseCtrlFrame(const std::string &reasonMsg = "");

    /**
    * Send Ping Message Notification on the websocket
    * @param request: the http request object
    * @param message: the content
    * @param length: the message length
    */
    void sendPingCtrlFrame(const unsigned char *message, size_t length);

    /**
    * Send Ping Message Notification on the websocket
    * @param message: the content
    */
    void sendPingCtrlFrame(const std::string &message);

    /**
    * Send Pong Message Notification on the websocket
    * @param message: the content
    * @param length: the message length
    */
    void sendPongCtrlFrame(const unsigned char *message, size_t length);

    /**
    * Send Pong Message Notification on the websocket
    * @param message: the content
    */
    void sendPongCtrlFrame(const std::string &message);

    HttpRequest *getHttpRequest() { return request; };

    void closeWS();
    void closeSend();
    void closeRecv();
};

#endif //WEBSOCKETCLIENT_HH_
