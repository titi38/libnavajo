//********************************************************
/**
 * @file  LogOutput.hh 
 *
 * @brief Generic log output (abstract class)
 *
 * @author T.Descombes (thierry.descombes@gmail.com)
 *
 * @version 1        
 * @date 19/02/15
 */
//********************************************************

#ifndef LOGOUTPUT_HH_
#define LOGOUTPUT_HH_

#include <string>

  typedef enum 
  {
        NVJ_DEBUG,
        NVJ_INFO,
        NVJ_WARNING,
        NVJ_ALERT,
        NVJ_ERROR,
        NVJ_FATAL
  } NvjLogSeverity;


  class LogOutput
  {
    bool withDateTime;
    bool withEndline;

    protected:
      inline void setWithDateTime(bool b) { withDateTime=b; };
      inline void setWithEndline(bool b) { withEndline=b; };

 
    public:
      LogOutput(): withDateTime(true),withEndline(false) { };
      virtual void initialize() = 0;
      virtual void append(const NvjLogSeverity& l, const std::string& m, const std::string &details) = 0;
      virtual ~LogOutput() {};
      inline bool isWithDateTime() { return withDateTime; };
      inline bool isWithEndline() { return withEndline; };

  };


#endif
