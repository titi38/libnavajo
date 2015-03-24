//------------------------------------------------------------------------------
//
// LogRecorder.hh : Store Log File
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

#ifndef LOGRECORDER_HH_
#define LOGRECORDER_HH_

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <list>
#include <set>
#include "cheetah/LogOutput.hh"

#define LOG LogRecorder::getInstance()

using namespace std;


  /**
  * LogRecorder - generic class to handle log trace
  */
  class LogRecorder
  {

     pthread_mutex_t log_mutex;

     std::set<string> uniqLog; // Only one entry !
     
    public:

      /**
      * getInstance - return/create a static logRecorder object
      * \return theLogRecorder - static log recorder
      */   
      inline static LogRecorder *getInstance()
      {
	      if (theLogRecorder == NULL)
		      theLogRecorder = new LogRecorder;
	      return theLogRecorder;
      };

      /**
      * freeInstance - free the static logRecorder object
      */
      
      static void freeInstance()
      {  
	      if (theLogRecorder != NULL)
	        delete theLogRecorder;
		
	      theLogRecorder=NULL;
      }
      
      void addLogOutput(LogOutput *);
      void removeLogOutputs();
      void append(const LogSeverity& l, const std::string& msg, const std::string& details="");
      inline void appendUniq(const LogSeverity& l, const std::string& msg, const std::string& details="")
      { 
	      set<string>::iterator it;
	      it=uniqLog.find(msg+details);
	      if (it==uniqLog.end())
	      {
	        uniqLog.insert(msg+details);
	        append(l, msg, details);
	      }
      };
      inline void initUniq() { uniqLog.clear(); } ;

    protected:
      LogRecorder();
      ~LogRecorder();
      std::string getDateStr();

      std::list<LogOutput *> logOutputsList_;

      static LogRecorder *theLogRecorder;
            
  };
  

#endif
