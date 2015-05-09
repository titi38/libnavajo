//********************************************************
/**
 * @file  LogSyslog.cc
 *
 * @brief Write log messages to syslog
 *
 * @author T.Descombes (thierry.descombes@gmail.com)
 *
 * @version 1        
 * @date 19/02/15
 */
//********************************************************

#include "navajo/LogSyslog.hh"

#include <syslog.h>
#include <string.h>


  /***********************************************************************/
  /**
  * append - append a message
  * \param l - LogSeverity
  * \param m - message
  */
  void LogSyslog::append(const LogSeverity& l, const std::string& message, const std::string& details)
  {

    int type;
    switch (l)
    {
      case _DEBUG_:
        type=LOG_DEBUG;
        break;
      case _WARNING_:
        type=LOG_WARNING;
        break;
      case _ALERT_:
        type=LOG_ALERT;
        break;
      case _ERROR_:
        type=LOG_ERR;
        break;
      case _FATAL_:
        type=LOG_EMERG;
        break;
      case _INFO_:
      default:
        type=LOG_INFO;
        break;
    }
    syslog(type, "%s", message.c_str());

  }

  /***********************************************************************/
  /**
  *  initialize the logoutput
  */ 

  void LogSyslog::initialize()
  {
    openlog(ident,LOG_PID,LOG_USER);
    setWithDateTime(false);
  }


  /***********************************************************************/
  /**
  * LogSyslog - constructor
  */ 
  
  LogSyslog::LogSyslog(const char *id)
  {
    strncpy (ident, id, 30);
  }

  /***********************************************************************/
  /**
  * ~LogRecorder - destructor
  */   

  LogSyslog::~LogSyslog()
  {
    closelog();
  }

  /***********************************************************************/

