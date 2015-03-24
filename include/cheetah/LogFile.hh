//------------------------------------------------------------------------------
//
// LogFile.hh : write message to Log File
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

#ifndef LOGFILE_HH_
#define LOGFILE_HH_

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>

#include "cheetah/LogOutput.hh"

using namespace std;

  /**
  * LogFile - LogOutput 
  */
  class LogFile : public LogOutput
  {
    public:
      LogFile(const char *filename);
      ~LogFile();
  
      void append(const LogSeverity& l, const std::string& m, const std::string& details="");
      void initialize();
  
    private:
      char filename[30];
      ofstream *file;
  
  };
  

#endif
