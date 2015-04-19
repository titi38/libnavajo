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
#include "navajo/LogStdOutput.hh"



  /***********************************************************************/
  /**
  * append - append a message
  * \param l - LogSeverity
  * \param m - message
  */
  void LogStdOutput::append(const LogSeverity& l, const std::string& message, const std::string& details)
  {
    switch (l)
    {
	case _DEBUG_:
        case _WARNING_:
	case _ALERT_:
        case _INFO_:
	  fprintf(stdout,"%s\n",message.c_str()); fflush(NULL);
	  break;

        case _ERROR_:
        case _FATAL_:
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

