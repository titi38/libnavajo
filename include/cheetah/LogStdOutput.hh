//------------------------------------------------------------------------------
//
// LogStdOutput.hh : display message to console
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

#ifndef LOGSTDOUTPUT_HH_
#define LOGSTDOUTPUT_HH_

#include "cheetah/LogOutput.hh"



  /**
  * LogStdOutput - LogOutput 
  */
  class LogStdOutput : public LogOutput
  {
    public:
      LogStdOutput();
      ~LogStdOutput();

      void append(const LogSeverity& l, const std::string& m, const std::string &details="");
      void initialize();
  
  };
  

#endif
