//********************************************************
/**
 * @file  WebSocket.cc
 *
 * @brief WebSocket Server
 *        rfc6455 The WebSocket Protocol
 *
 * @author T.Descombes (thierry.descombes@gmail.com)
 *
 * @version 1
 * @date 19/02/16
 */
//********************************************************

#include "libnavajo/nvjSocket.h"
#include "libnavajo/nvjGzip.h"
#include "libnavajo/htonll.h"
#include "libnavajo/WebSocket.hh"
#include "libnavajo/WebServer.hh"

#define BUFSIZE 32768

/***********************************************************************/

void WebSocket::listenWebSocket(WebSocket *websocket, HttpRequest* request)
{
  char bufferRecv[BUFSIZE];
  volatile bool closing=false;
  u_int64_t msgLength=0, msgContentIt=0;
  bool msgMask=false;

  unsigned char msgKeys[4];
  unsigned char* msgContent = NULL;
  enum MsgDecodSteps { FIRSTBYTE, LENGTH, MASK, CONTENT };

  ClientSockData* client = request->getClientSockData();

  setSocketSndRcvTimeout(client->socketId, 0, 500); // Reduce socket timeout

  addNewClient(request);

  bool fin=false;
  unsigned char rsv=0, opcode=0;
  u_int64_t readLength=1;
  MsgDecodSteps step=FIRSTBYTE;
  memset( msgKeys, 0, 4*sizeof(unsigned char) );

  for (;!closing;)
  {

    int n=0;
    size_t it=0;
    size_t length=readLength;

    if (length > BUFSIZE)
    {
      length=BUFSIZE;
      readLength-=BUFSIZE;
    }

    do
    {
      if (client->bio != NULL && client->ssl != NULL)
      {
        n=BIO_read(client->bio, bufferRecv+it, length-it);

        if (SSL_get_error(client->ssl,n) == SSL_ERROR_ZERO_RETURN)
          closing=true;
      }
      else
      {
        n=recv(client->socketId, bufferRecv+it, length-it, 0);
        if ( n <= 0 )
        {
          if ( errno==ENOTCONN || errno==EBADF || errno==ECONNRESET )
            closing=true;
          continue;
        }
      }

      it += n;
    }
    while (it != length && !closing);

    if (closing) continue;

    // Decode message header
    switch(step)
    {

      case FIRSTBYTE:
        fin=(bufferRecv[0] & 0x80) >> 7;
        rsv=(bufferRecv[0] & 0x70) >> 4;
        opcode=bufferRecv[0] & 0xf;

        step=LENGTH;
        readLength=1;
        break;

      case LENGTH:
        if (!msgLength)
        {
          msgMask = (bufferRecv[0]  & 0x80) >> 7;
          if (!msgMask)
          {
            closing=true;
            continue;
          }

          msgLength =  bufferRecv[0] & 0x7f;
          if (!msgLength)
          {
            NVJ_LOG->append(NVJ_WARNING, "Websocket: Message length is null. Closing socket.");
            msgLength=0;
            msgContent=NULL;
            step=CONTENT;
            continue;
          }
          if (msgLength == 126) { readLength=2; break; }
          if (msgLength == 127) { readLength=8; break; }
        }
        else
        {
          if (msgLength == 126)
          {
            u_int16_t *tmp=(u_int16_t*)bufferRecv;
            msgLength=ntohs(*tmp);
            //msgLength=ntohs(*(u_int16_t*)bufferRecv);
          }
          if (msgLength == 127)
          {
            u_int64_t *tmp=(u_int64_t*)bufferRecv;
            msgLength=ntohll(*tmp);
            //msgLength=ntohll(*(u_int64_t*)bufferRecv);
          }
        }

        if ( (msgLength > 0x7FFF)
             || ( (msgContent = (unsigned char*)malloc(msgLength*sizeof(unsigned char))) == NULL ))
        {
          char logBuffer[500];
          snprintf(logBuffer, 500, " Websocket: Message content allocation failed (length: %llu)", static_cast<unsigned long long>(msgLength));
          NVJ_LOG->append(NVJ_WARNING, logBuffer);
          msgContent=NULL;
        }
        msgContentIt=0;
        if (msgMask) { step=MASK; readLength=4; } else { step=CONTENT; readLength=msgLength; }
        break;

      case MASK:
        memcpy(msgKeys, bufferRecv, 4*sizeof(unsigned char));
        readLength=msgLength;
        step=CONTENT;
        break;

      case CONTENT:
        char buf[300]; snprintf(buf, 300, "WebSocket: new message received (len=%llu fin=%d rsv=%d opcode=%d mask=%d)",
                                static_cast<unsigned long long>(msgLength), fin, rsv, opcode, msgMask);
        NVJ_LOG->append(NVJ_DEBUG,buf);
        if (msgContent != NULL)
        {
          for (size_t i=0; i<length; i++)
          {
            if (msgMask)
              msgContent[msgContentIt] = bufferRecv[i] ^ msgKeys[msgContentIt % 4];
            else msgContent[msgContentIt] = bufferRecv[i];

            msgContentIt++;
          }
        }

        if (msgContentIt == msgLength)
        {
          if (msgContent != NULL && (client->compression == ZLIB) && (rsv & 4) )
          {
            try
            {
              unsigned char *msg = NULL;
              size_t msgLen=nvj_gunzip( &msg, msgContent, msgLength, true );
              free(msgContent);
              msgContent=msg;
              msgLength=msgLen;

              msgContent[msgLength]='\0';
            }
            catch (std::exception& e)
            {
              NVJ_LOG->append(NVJ_ERROR, string(" Websocket: nvj_gzip raised an exception: ") +  e.what());
              msgLength = 0;
            }
          }

          switch(opcode)
          {
            case 0x1:
              if (msgLength)
                websocket->onTextMessage(request, string((char*)msgContent, msgLength), fin);
              else websocket->onTextMessage(request, "", fin);
              break;
            case 0x2:
              websocket->onBinaryMessage(request, msgContent, msgLength, fin);
              break;
            case 0x8:
              if (websocket->onCloseCtrlFrame(request, msgContent, msgLength))
              {
                webSocketSendCloseCtrlFrame(request, msgContent, msgLength);
                closing=true;
              }
              break;
            case 0x9:
              if (websocket->onPingCtrlFrame(request, msgContent, msgLength))
                webSocketSendPongCtrlFrame(request, msgContent, msgLength);
              break;
            case 0xa:
              websocket->onPongCtrlFrame(request, msgContent, msgLength);
              break;
            default:
              char buf[300]; snprintf(buf, 300, "WebSocket: message received with unknown opcode (%d) has been ignored", opcode);
              NVJ_LOG->append(NVJ_INFO,buf);
              break;
          }

          // FINISHED
          /*        if (msgContent != NULL)
                    for (u_int64_t i=0; i<msgLength; i++)
                      printf("%c(%2x)", msgContent[i],msgContent[i]);
                    printf("\n"); fflush(NULL);*/

          if ((client->compression == ZLIB) && (rsv & 4) && msgLength)
            free (msgContent);
          fin=false; rsv=0;
          opcode=0;
          msgLength=0;
          msgContentIt=0;
          msgMask=false;
          memset( msgKeys, 0, 4*sizeof(unsigned char) );
          msgContent=NULL;
          readLength=1;
          step=FIRSTBYTE;
        }
        break;
    }
  }

  onClosing(request);
  removeClient(request);
  WebServer::freeClientSockData(client);
}

/***********************************************************************/

void WebSocket::webSocketSend(HttpRequest* request, const u_int8_t opcode, const unsigned char* message, size_t length, bool fin)
{
  ClientSockData* client = request->getClientSockData();

  unsigned char headerBuffer[10]; // 10 is the max header size
  size_t headerLen=2; // default header size
  unsigned char *msg = NULL;
  size_t msgLen=0;

  headerBuffer[0]= 0x80 | (opcode & 0xf) ; // FIN & OPCODE:0x1
  if (client->compression == ZLIB)
  {
    headerBuffer[0] |= 0x40; // Set RSV1
    try
    {
      msgLen=nvj_gzip( &msg, message, length, true );
    }
    catch(...)
    {
      NVJ_LOG->append(NVJ_ERROR, " Websocket: nvj_gzip raised an exception");
      return;
    }
  }
  else
  {
    msg=(unsigned char*)message;
    msgLen=length;
  }


  if (msgLen < 126)
    headerBuffer[1]=msgLen;
  else
  {
    if (msgLen < 0xFFFF)
    {
      headerBuffer[1]=126;
      *(u_int16_t*)(headerBuffer+2)=htons((u_int16_t)msgLen);
      headerLen+=2;
    }
    else
    {
      headerBuffer[1]=127;
      *(u_int64_t*)(headerBuffer+2)=htonll((u_int64_t)msgLen);
      headerLen+=8;
    }
  }

  if (  !WebServer::httpSend(client, headerBuffer, headerLen)
     || !WebServer::httpSend(client, msg, msgLen) )
  {
    //onError(request, "write to websocket failed");
    onClosing(request);
    removeClient(request);
    WebServer::freeClientSockData(client);
  }

  if (client->compression == ZLIB)
    free (msg);
}

/***********************************************************************/

void WebSocket::webSocketSendTextMessage(HttpRequest* request, const string &message, bool fin)
{
  webSocketSend(request, 0x1, (const unsigned char*)(message.c_str()), message.length(), fin);
}

/***********************************************************************/

void WebSocket::webSocketSendBinaryMessage(HttpRequest* request, const unsigned char* message, size_t length, bool fin)
{
  webSocketSend(request, 0x2, message, length, fin);
}

/***********************************************************************/

void WebSocket::webSocketSendPingCtrlFrame(HttpRequest* request, const unsigned char* message, size_t length)
{
  webSocketSend(request, 0x9, message, length, true);
}

void WebSocket::webSocketSendPingCtrlFrame(HttpRequest* request, const string &message)
{
  webSocketSend(request, 0x9, (const unsigned char*)message.c_str(), message.length(), true);
}

/***********************************************************************/

void WebSocket::webSocketSendPongCtrlFrame(HttpRequest* request, const unsigned char* message, size_t length)
{
  webSocketSend(request, 0xa, message, length, false);
}

void WebSocket::webSocketSendPongCtrlFrame(HttpRequest* request, const string &message)
{
  webSocketSend(request, 0xa, (const unsigned char*)message.c_str(), message.length(), false);
}

/***********************************************************************/

void WebSocket::webSocketSendCloseCtrlFrame(HttpRequest* request, const unsigned char* message, size_t length)
{
  webSocketSend(request, 0x8, message, length, true);
}

void WebSocket::webSocketSendCloseCtrlFrame(HttpRequest* request, const string &message)
{
  webSocketSend(request, 0x8, (const unsigned char*)message.c_str(), message.length(), true);
}



