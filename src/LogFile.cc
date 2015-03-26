//------------------------------------------------------------------------------
//
// LogFile.cc : write message to Log File
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

#include <stdlib.h>
#include <string.h>

#include "navajo/LogFile.hh"

  /***********************************************************************/
  /**
  * add - add an entry to the LogRecorder
  * \param l - type of entry
  * \param m - message
  */
  void LogFile::append(const LogSeverity& l, const std::string& message, const std::string& details)
  {
    if (file!=NULL)
      (*file) << message << endl;
  }

  /***********************************************************************/
  /**
  * LogFile - initialize
  */ 

  void LogFile::initialize()
  {
    file=new ofstream;
    file->open(filename, ios::out | ios::app);

    if (file->fail())
    {
    	cerr <<"Can't open " << filename << endl;
    	exit(1);
    }
  }


  /***********************************************************************/
  /**
  * LogFile - constructor
  */ 
  
  LogFile::LogFile(const char *f)
  {
    strncpy (filename, f, 30);
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

