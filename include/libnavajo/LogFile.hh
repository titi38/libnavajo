//********************************************************
/**
 * @file  LogFile.hh
 *
 * @brief Write log messages to a file
 *
 * @author T.Descombes (thierry.descombes@gmail.com)
 *
 * @version 1        
 * @date 19/02/15
 */
//********************************************************

#ifndef LOGFILE_HH_
#define LOGFILE_HH_

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>

#include "libnavajo/LogOutput.hh"


  /**
  * LogFile - LogOutput 
  */
  class LogFile : public LogOutput
  {
    public:
      LogFile(const char *filename);
      ~LogFile();
  
      void append(const NvjLogSeverity& l, const std::string& m, const std::string& details="");
      void initialize();
  
    private:
      char filename[30];
      std::ofstream *file;
  
  };
  

#endif
