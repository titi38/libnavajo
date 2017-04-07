//********************************************************
/**
 * @file  LogStdOutput.cc
 *
 * @brief Write log messages to standart output
 *
 * @author T.Descombes (thierry.descombes@gmail.com)
 *
 * @version 1        
 * @date 19/02/15
 */
//********************************************************

#include <stdio.h>
#include "libnavajo/LogStdOutput.hh"

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"


  /***********************************************************************/
  /**
  * append - append a message
  * \param l - LogSeverity
  * \param m - message
  */
  void LogStdOutput::append(const NvjLogSeverity& l, const std::string& message, const std::string& details)
  {
   
    switch (l)
    {
      case NVJ_DEBUG:
        fprintf(stdout, CYN "%s\n" RESET, message.c_str()); fflush(NULL);
        break;
      case NVJ_WARNING:
        fprintf(stdout, YEL "%s\n" RESET, message.c_str()); fflush(NULL);
        break;
      case NVJ_ALERT:
        fprintf(stdout, MAG "%s\n" RESET, message.c_str()); fflush(NULL);
        break;
      case NVJ_INFO:
        fprintf(stdout, GRN "%s\n" RESET, message.c_str()); fflush(NULL);
        break;
      case NVJ_ERROR:
        fprintf(stderr, RED "%s\n" RESET, message.c_str());
        break;
      case NVJ_FATAL:
        fprintf(stderr, RED "%s\n" RESET,message.c_str()); 
        break;
    }
  }

  /***********************************************************************/
  /**
  *  initialize the logoutput
  */ 

  void LogStdOutput::initialize()
  {
  }


  /***********************************************************************/
  /**
  * LogStdOutput - constructor
  */ 
  
  LogStdOutput::LogStdOutput()
  {
  }

  /***********************************************************************/
  /**
  * ~LogRecorder - destructor
  */   

  LogStdOutput::~LogStdOutput()
  {
  }

  /***********************************************************************/

