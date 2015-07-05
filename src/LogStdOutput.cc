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
      case NVJ_WARNING:
      case NVJ_ALERT:
      case NVJ_INFO:
        fprintf(stdout,"%s\n",message.c_str()); fflush(NULL);
        break;
      case NVJ_ERROR:
      case NVJ_FATAL:
      default:
        fprintf(stderr,"%s\n",message.c_str()); 
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

