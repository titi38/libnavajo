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

#include "libnavajo/LogSyslog.hh"

#include <syslog.h>
#include <string.h>


  /***********************************************************************/
  /**
  * append - append a message
  * \param l - LogSeverity
  * \param m - message
  */
  void LogSyslog::append(const NvjLogSeverity& l, const std::string& message, const std::string& details)
  {

    int type;
    switch (l)
    {
      case NVJ_DEBUG:
        type=LOG_DEBUG;
        break;
      case NVJ_WARNING:
        type=LOG_WARNING;
        break;
      case NVJ_ALERT:
        type=LOG_ALERT;
        break;
      case NVJ_ERROR:
        type=LOG_ERR;
        break;
      case NVJ_FATAL:
        type=LOG_EMERG;
        break;
      case NVJ_INFO:
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

