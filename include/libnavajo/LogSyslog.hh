//********************************************************
/**
 * @file  LogSyslog.hh
 *
 * @brief write log messages to syslog
 *
 * @author T.Descombes (thierry.descombes@gmail.com)
 *
 * @version 1        
 * @date 19/02/15
 */
//********************************************************

#ifndef LOGSYSLOG_HH_
#define LOGSYSLOG_HH_

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>

#include "libnavajo/LogOutput.hh"


  /**
  * LogSyslog - LogOutput 
  */
  class LogSyslog : public LogOutput
  {
    public:
      LogSyslog(const char *id="Navajo");
      ~LogSyslog();
  
      void append(const NvjLogSeverity& l, const std::string& m, const std::string& details="");
      void initialize();
  
    private:
      char ident[30];

  
  };
  

#endif
