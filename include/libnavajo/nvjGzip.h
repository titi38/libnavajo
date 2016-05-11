//********************************************************
/**
 * @file  nvjGzip.h
 *
 * @brief zip compression's facilities
 *
 * @author T.Descombes (thierry.descombes@gmail.com)
 *
 * @version 1        
 * @date 19/02/15
 */
//********************************************************

#ifndef NVJGZIP_H_
#define NVJGZIP_H_


#include <stdlib.h>
#include <string>
#include <stdexcept>
 
#include "zlib.h" 
#if (ZLIB_VERNUM < 0x1271)
 #include "zlibPatchOldVersion.h"
#endif

#define CHUNK 16384



//********************************************************

inline size_t nvj_gzip( unsigned char** dst, const unsigned char* src, const size_t sizeSrc, bool rawDeflateData=false )
{
  z_stream strm;
  size_t sizeDst=CHUNK;

  /* allocate deflate state */
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;

  if ( deflateInit2(&strm, Z_BEST_SPEED, Z_DEFLATED, rawDeflateData ? -15 : 16+MAX_WBITS, 9, Z_DEFAULT_STRATEGY) != Z_OK)
    throw std::runtime_error(std::string("gzip : inflateInit2 error") );

  if ( (*dst=(unsigned char *)malloc(CHUNK * sizeof (unsigned char))) == NULL )
    throw std::runtime_error(std::string("gzip : malloc error (1)") );

  strm.avail_in = sizeSrc;
  strm.next_in = (Bytef*)src;

  int i=0;
  
  do 
  {
    strm.avail_out = CHUNK;
    strm.next_out = (Bytef*)*dst + i*CHUNK;
    sizeDst=CHUNK * (i+1);
  
     if (deflate(&strm, Z_FINISH ) == Z_STREAM_ERROR)  /* state not clobbered */
    {
      free (*dst);
      throw std::runtime_error(std::string("gzip : deflate error") );
    }

    i++;
  
    if (strm.avail_out == 0)
    {
      unsigned char* reallocDst = (unsigned char*) realloc (*dst, CHUNK * (i+1) * sizeof (unsigned char) );

      if (reallocDst!=NULL)
        *dst=reallocDst;
       else
      {
        free (reallocDst);
        free (*dst);
        throw std::runtime_error(std::string("gzip : (re)allocating memory") );
      }
    }
  }
  while (strm.avail_out == 0);

  /* clean up and return */
  (void)deflateEnd(&strm);
  return sizeDst - strm.avail_out;
}

//********************************************************

inline size_t nvj_gunzip( unsigned char** dst, const unsigned char* src, const size_t sizeSrc, bool rawDeflateData=false )
{
  z_stream strm;
  size_t sizeDst=CHUNK;
  int ret;

  if (src == NULL)
    throw std::runtime_error(std::string("gunzip : src == NULL !") );

  /* allocate inflate state */
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = 0;
  strm.next_in = Z_NULL;


  if (inflateInit2(&strm, rawDeflateData ? -15 : 16+MAX_WBITS) != Z_OK)
    throw std::runtime_error(std::string("gunzip : inflateInit2 error") );

  if ( (*dst=(unsigned char *)malloc(CHUNK * sizeof (unsigned char))) == NULL )
    throw std::runtime_error(std::string("gunzip : malloc error (2)") );

  strm.avail_in = sizeSrc;
  strm.next_in = (Bytef*)src;

  int i=0;
 
  do
  {
    strm.avail_out = CHUNK;
    strm.next_out = (Bytef*)*dst + i*CHUNK;
    sizeDst=CHUNK * (i+1);

    ret = inflate(&strm, Z_NO_FLUSH);

    switch (ret) 
    {
      case Z_STREAM_ERROR:
        free (*dst);
        throw std::runtime_error(std::string("gunzip : inflate Z_STREAM_ERROR") );
      case Z_NEED_DICT:
      case Z_DATA_ERROR:
      case Z_MEM_ERROR:
        (void)inflateEnd(&strm);
        free (*dst);
        throw std::runtime_error(std::string("gunzip : inflate error") );
    }
  
    i++;
     if (strm.avail_out == 0)
     {
      unsigned char* reallocDst = (unsigned char*) realloc (*dst, CHUNK * (i+1) * sizeof (unsigned char) );

      if (reallocDst!=NULL)
        *dst=reallocDst;
      else
      {
        free (reallocDst);
        free (*dst);
        throw std::runtime_error(std::string("gunzip : (re)allocating memory") );
      }
    }
  }
  while (strm.avail_out == 0);

  /* clean up and return */
  (void)inflateEnd(&strm);
  return sizeDst - strm.avail_out;
}

//----------------------------------------------------------------------------------------
  
  inline size_t nvj_gzip_websocket_v2( unsigned char** dst, const unsigned char* src, const size_t sizeSrc, z_stream* pstream=NULL, const unsigned int sizeChunk = CHUNK)
  {

    size_t sizeDst = 0;

    int rmndr = sizeSrc % sizeChunk;
    int iterations = (rmndr == 0 ? sizeSrc / sizeChunk : sizeSrc / sizeChunk + 1) ;
    int in = sizeChunk;
    int flush = Z_NO_FLUSH;

    *dst = NULL;

    for ( int i = 0; i < iterations; i++ ) 
    {
      if(i == iterations - 1){
        flush = Z_SYNC_FLUSH;
        if(rmndr != 0){
          in = rmndr;
        }
      }


      (*pstream).avail_in = in;
      (*pstream).next_in = (Bytef*)src + i * sizeChunk;

      do
      {
        unsigned char* reallocDst = (unsigned char*) realloc (*dst, (sizeDst + sizeChunk) * sizeof (unsigned char) );

        if (reallocDst!=NULL){
          *dst = reallocDst; 
        }
        else
        {
          free (reallocDst);
          free (*dst);
          throw std::runtime_error(std::string("gzip : (re)allocating memory") );
        }

        (*pstream).avail_out = sizeChunk;
        (*pstream).next_out = (Bytef*)*dst + sizeDst;

        if (deflate(pstream, flush) == Z_STREAM_ERROR)  /* state not clobbered */
        {
          free (*dst);
          throw std::runtime_error(std::string("gzip : deflate error") );
        }
        sizeDst += sizeChunk - (*pstream).avail_out;
      }
      while ((*pstream).avail_out == 0 );
    }

    sizeDst -= 4;

    unsigned char* reallocDst = (unsigned char*) realloc (*dst, sizeDst * sizeof (unsigned char) );

    if (reallocDst!=NULL){
      *dst = reallocDst; 
    }
    else
    {
      free (reallocDst);
      free (*dst);
      throw std::runtime_error(std::string("gzip : (re)allocating memory") );
    }

    return sizeDst;

  }



  //********************************************************

  inline void nvj_end_stream(z_stream* pstream=NULL){
    (void)deflateEnd(pstream);
  }

  //********************************************************

  inline void nvj_init_stream(z_stream* pstream=NULL, bool rawDeflateData=false, int level=Z_BEST_COMPRESSION, int strategy=Z_DEFAULT_STRATEGY ){
        (*pstream).zalloc = Z_NULL;
        (*pstream).zfree = Z_NULL;
        (*pstream).opaque = Z_NULL;
    if ( deflateInit2(pstream, level, Z_DEFLATED, rawDeflateData ? -15 : 16+MAX_WBITS, 9, strategy) != Z_OK)
      throw std::runtime_error(std::string("gzip : deflateInit2 error") );
  }

   //********************************************************

inline size_t nvj_gunzip_websocket_v2( unsigned char** dst, const unsigned char* src,  size_t sizeSrc, bool rawDeflateData=false, unsigned char* dictionary = NULL, unsigned int* dictLength = NULL, const unsigned int sizeChunk = CHUNK)
{
  z_stream strm;
  size_t sizeDst=0;
  int rmndr = sizeSrc % sizeChunk;
  int iterations = (rmndr == 0 ? sizeSrc / sizeChunk : sizeSrc / sizeChunk + 1) ;
  int in = sizeChunk;
  int ret;
  *dst = NULL;

  if (src == NULL)
    throw std::runtime_error(std::string("gunzip : src == NULL !") );

  /* allocate inflate state */
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = 0;
  strm.next_in = Z_NULL;


  if (inflateInit2(&strm, rawDeflateData ? -15 : 16+MAX_WBITS) != Z_OK)
    throw std::runtime_error(std::string("gunzip : inflateInit2 error") );

  if( *dictLength != 0) {
    if( inflateSetDictionary(&strm, (Bytef*) dictionary, *dictLength) != Z_OK ) 
       throw std::runtime_error(std::string("gunzip : inflateSetDictionary error") ) ;
  }

  for ( int i = 0; i < iterations; i++ ) //TODO Ne pas oublier le reste
  {

    if( (i == iterations - 1) && (rmndr != 0) ){
      in = rmndr;
    }
    
    strm.avail_in = in;
    strm.next_in = (Bytef*)src + i * sizeChunk;

    do
    {

      unsigned char* reallocDst = (unsigned char*) realloc (*dst, (sizeDst + sizeChunk) * sizeof (unsigned char) );

      if (reallocDst!=NULL)
        *dst=reallocDst;
      else
      {
        free (reallocDst);
        free (*dst);
        throw std::runtime_error(std::string("gunzip : (re)allocating memory") );
      }

      strm.avail_out = sizeChunk;
      strm.next_out = (Bytef*)*dst + sizeDst;

      ret = inflate(&strm, Z_NO_FLUSH);

      switch (ret) 
      {
        case Z_STREAM_ERROR:
          free (*dst);
          throw std::runtime_error(std::string("gunzip : inflate Z_STREAM_ERROR") );
        case Z_NEED_DICT:
        case Z_DATA_ERROR:
    // ICI
        case Z_MEM_ERROR:
          (void)inflateEnd(&strm);
          free (*dst);
          throw std::runtime_error(std::string("gunzip : inflate error") );
      }

      sizeDst += sizeChunk - strm.avail_out;
    }
    while (strm.avail_out == 0);
  }

  unsigned char* reallocDst = (unsigned char*) realloc (*dst, (sizeDst) * sizeof (unsigned char) );

  if (reallocDst!=NULL)
    *dst=reallocDst;
  else
  {
    free (reallocDst);
    free (*dst);
    throw std::runtime_error(std::string("gunzip : (re)allocating memory") );
  }

  inflateGetDictionary(&strm, (Bytef*)dictionary, dictLength);

  /* clean up and return */
  (void)inflateEnd(&strm);
  return sizeDst;
}

#endif

