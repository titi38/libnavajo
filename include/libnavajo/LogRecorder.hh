//********************************************************
/**
 * @file  LogRecorder.hh 
 *
 * @brief Log Manager class
 *
 * @author T.Descombes (thierry.descombes@gmail.com)
 *
 * @version 1        
 * @date 19/02/15
 */
//********************************************************

#ifndef LOGRECORDER_HH_
#define LOGRECORDER_HH_

#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <list>
#include <set>
#include "libnavajo/LogOutput.hh"
#include "libnavajo/nvjThread.h"

#define NVJ_LOG LogRecorder::getInstance()
#define NVJ_printf LogRecorder::getInstance()->printf

  /**
  * LogRecorder - generic class to handle log trace
  */
  class LogRecorder
  {

     pthread_mutex_t log_mutex;
     bool debugMode;
     std::set<std::string> uniqLog; // Only one entry !


    public:

      /**
      * getInstance - return/create a static logRecorder object
      * \return theLogRecorder - static log recorder
      */   
      inline static LogRecorder *getInstance()
      {
        static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_lock(&init_mutex);
        if (theLogRecorder == NULL)
          theLogRecorder = new LogRecorder;
        pthread_mutex_unlock(&init_mutex);
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
      void setDebugMode(bool d=true) { debugMode=d; };
      void addLogOutput(LogOutput *);
      void removeLogOutputs();

      void append(const NvjLogSeverity& l, const std::string& msg, const std::string& details="");
      inline void appendUniq(const NvjLogSeverity& l, const std::string& msg, const std::string& details="")
      {
        bool shouldAppend = false;
        const std::string key = msg + details;

        pthread_mutex_lock(&log_mutex);
        if (uniqLog.find(key) == uniqLog.end())
        {
          uniqLog.insert(key);
          shouldAppend = true;
        }
        pthread_mutex_unlock(&log_mutex);

        if (shouldAppend)
          append(l, msg, details);
      };

			inline void printf(const NvjLogSeverity severity, const char *fmt, ...)
			{
				char buff[4096];
				va_list argptr;
				va_start(argptr, fmt);
				vsnprintf(buff, sizeof(buff), fmt, argptr);
				va_end(argptr);

				append(severity, buff);
			}

			inline void initUniq()
      {
        pthread_mutex_lock(&log_mutex);
        uniqLog.clear();
        pthread_mutex_unlock(&log_mutex);
      } ;


    protected:

      LogRecorder();
      ~LogRecorder();
      std::string getDateStr();

      std::list<LogOutput *> logOutputsList_;

      static LogRecorder *theLogRecorder;
            
  };
  

#endif
