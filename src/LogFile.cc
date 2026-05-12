//*******************************************************
/**
 * @file  LogFile.cc
 *
 * @brief Write log messages to a file
 *
 * @author T.Descombes (thierry.descombes@gmail.com)
 *
 * @version 1        
 * @date 19/02/15
 */
//********************************************************

#include <stdlib.h>
#include <string.h>

#include "libnavajo/LogFile.hh"

  /***********************************************************************/
  /**
  * add - add an entry to the LogRecorder
  * \param l - type of entry
  * \param m - message
  */
  void LogFile::append(const NvjLogSeverity& /*l*/, const std::string& message, const std::string& /*details*/)
  {
    if (file!=NULL)
      (*file) << message << std::endl;
  }

  /***********************************************************************/
  /**
  * LogFile - initialize
  */ 

  void LogFile::initialize()
  {
    file=new std::ofstream;
    file->open(filename.c_str(), std::ios::out | std::ios::app);

    if (file->fail())
    {
        std::cerr <<"Can't open " << filename << std::endl;
        delete file;
        file=NULL;
        return;
    }
  }


  /***********************************************************************/
  /**
  * LogFile - constructor
  */ 
  
  LogFile::LogFile(const char *f)
  {
    filename = (f != NULL) ? f : "";
    file=NULL;
    //setWithEndline(true);
  }

  /***********************************************************************/
  /**
  * ~LogRecorder - destructor
  */   

  LogFile::~LogFile()
  {
    if (file!=NULL)
    {
      file->close();
      delete file;
    }
  }

  /***********************************************************************/

