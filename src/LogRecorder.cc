//------------------------------------------------------------------------------
//
// LogRecorder.cc : Store Log File
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



#include <time.h>
#include "navajo/LogRecorder.hh"


  /**
  * LogRecorder - static and unique log recorder object
  */
  LogRecorder * LogRecorder::theLogRecorder = NULL;

  /***********************************************************************/
  /**
  * getDateStr - return a string with the formatted date
  * \return string - formatted date
  */
  std::string LogRecorder::getDateStr()
  {
    struct tm today;
    char tmpbuf[128];
    time_t ltime;

    time( &ltime );
    gmtime_r(&ltime, &today);
  
    std::string ret_str;
    strftime( tmpbuf, 128, "[%Y-%m-%d %H:%M:%S] >  ", &today );
    ret_str=tmpbuf;
    return ret_str;

  }

  /***********************************************************************/
  /**
  * append - append an entry to the LogRecorder
  * \param l - type of entry
  * \param m - message
  */
  void LogRecorder::append(const LogSeverity& l, const std::string& m, const std::string& details)
  {
    pthread_mutex_lock( &log_mutex );

    if (l != _DEBUG_)
    {
      for( std::list<LogOutput *>::iterator it=logOutputsList_.begin(); 
           it!=logOutputsList_.end(); 
	   it++ )
      {
	std::string msg;

	if ((*it)->isWithDateTime())
	  msg=getDateStr() + m;
	else msg=m;

	if ((*it)->isWithEndline())
	   msg+= std::string("\n") ;
	   
        (*it)->append(l, msg, details);
      }
    }   
    
    pthread_mutex_unlock( &log_mutex );

  }

  /***********************************************************************/
  /**
  * addLogOutput - ajout d'une sortie LogOutput où imprimer les logs
  */

  void LogRecorder::addLogOutput(LogOutput *output)
  {
    output->initialize();
    logOutputsList_.push_back(output);
  }

  /***********************************************************************/
  /**
  * removeLogOutputs - supprime toutes les sorties LogOutput
  */
  void LogRecorder::removeLogOutputs()
  {
    for( std::list<LogOutput *>::iterator it=logOutputsList_.begin(); 
           it!=logOutputsList_.end(); 
	   it++ )
      delete *it;

    logOutputsList_.clear();
  }

  /***********************************************************************/
  /**
  * LogRecorder - base constructor
  */ 

  LogRecorder::LogRecorder()
  {
    pthread_mutex_init(&log_mutex, NULL);
  }
  
  /***********************************************************************/
  /**
  * ~LogRecorder - destructor
  */   
  LogRecorder::~LogRecorder()
  {
    removeLogOutputs();
  }

  /***********************************************************************/


