//------------------------------------------------------------------------------
//
// LogStdOutput.cc : display message to console
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

#include <stdio.h>
#include "cheetah/LogStdOutput.hh"



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

