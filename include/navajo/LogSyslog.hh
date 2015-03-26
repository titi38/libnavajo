//------------------------------------------------------------------------------
//
// LogSyslog.hh : use syslogd
//
//------------------------------------------------------------------------------
//
// AUTHOR      : T.DESCOMBES (descombt@ipnl.in2p3.fr)
// PROJECT     : EventBuilder
//
//------------------------------------------------------------------------------
//           Versions and Editions  historic
//------------------------------------------------------------------------------
// Ver      | Ed  |   date   | resp.       | comment
//----------+-----+----------+-------------+------------------------------------
// 1        | 1   | 10/12/02 | T.DESCOMBES | source creation
//----------+-----+----------+-------------+------------------------------------
//
//           Detailed modification operations on this source
//------------------------------------------------------------------------------
//  Date  :                            Author :
//  REL   :
//  Description :
//
//------------------------------------------------------------------------------

#ifndef LOGSYSLOG_HH_
#define LOGSYSLOG_HH_

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>

#include "navajo/LogOutput.hh"

using namespace std;

  /**
  * LogSyslog - LogOutput 
  */
  class LogSyslog : public LogOutput
  {
    public:
      LogSyslog(const char *id="Navajo");
      ~LogSyslog();
  
      void append(const LogSeverity& l, const std::string& m, const std::string& details="");
      void initialize();
  
    private:
      char ident[30];

  
  };
  

#endif
