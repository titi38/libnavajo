//------------------------------------------------------------------------------
//
// LogOutput.hh : Abstract to display log message
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

#ifndef LOGOUTPUT_HH_
#define LOGOUTPUT_HH_

#include <string>

      typedef enum 
      {
		_DEBUG_,
        _INFO_,
        _WARNING_,
		_ALERT_,
        _ERROR_,
        _FATAL_
      } LogSeverity;


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
      virtual void append(const LogSeverity& l, const std::string& m, const std::string &details) = 0;
      virtual ~LogOutput() {};
      inline bool isWithDateTime() { return withDateTime; };
      inline bool isWithEndline() { return withEndline; };

  };


#endif
