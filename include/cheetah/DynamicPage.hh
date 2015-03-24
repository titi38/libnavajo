
//********************************************************
/**
 * @file  DynamicPage.hh
 *
 * @brief Get json files to webserver
 *
 * @author T.Descombes (descombes@lpsc.in2p3.fr)
 *
 * @version 1  
 * @date 19/02/10 
 */
//********************************************************

#ifndef DYNAMICPAGE_HH_
#define DYNAMICPAGE_HH_

#include <string>


class DynamicPage
{
  typedef std::map <string, string> ReqParametersMap;
  ReqParametersMap parameters;

  inline void decodParams( const string& p )
  {
    size_t start = 0, end = 0;
    string paramstr=p;

    while ((end = paramstr.find_first_of("%+", start)) != string::npos) 
    {
      switch (paramstr[end])
      {
        case '%':
          if (paramstr[end+1]=='%')
            paramstr=paramstr.erase(end+1,1);
          else
          {
            unsigned int specar;
            string hexChar=paramstr.substr(end+1,2);
            std::stringstream ss; ss << std::hex << hexChar.c_str();
            ss >> specar;
            paramstr[end] = (char)specar;
            paramstr=paramstr.erase(end+1,2);
          }
          break;

        case '+':
          paramstr[end]=' ';
          break;
      }   
      
      start=end+1;
    }
    
    start = 0; end = 0;
    bool islastParam=false;
    while (!islastParam) 
    {
      islastParam= (end = paramstr.find('&', start)) == string::npos;
      if (islastParam) end=paramstr.size();
      
      string theParam=paramstr.substr(start, end - start);
      
      size_t posEq=0;
      if ((posEq = theParam.find('=')) == string::npos) 
        parameters[theParam]="";
      else
        parameters[theParam.substr(0,posEq)]=theParam.substr(posEq+1);
       
      start = end + 1;
    }
  };

  public:
    DynamicPage() {};
    virtual bool getPage(unsigned char **webpage, size_t *webpageLen) = 0;
    inline bool getPage(const string& param, unsigned char **webpage, size_t *webpageLen) 
      { parameters.clear(); decodParams(param); return getPage(webpage, webpageLen); };

    /**********************************************************************/
     
    template<class T> static inline T getValue (string s)
        { if (!s.length()) return (T)-1; 
	     std::istringstream iss(s); T tmp; iss>>tmp;  return tmp; };
    
    /**********************************************************************/

    inline bool getParameter( const string& name, string &value )
    {
      if(!parameters.empty())
      {
        ReqParametersMap::const_iterator it;
        if((it = parameters.find(name)) != parameters.end())
        {
          value=(*it).second;
          return true;
        }
      }
      return false;
    }

    /**********************************************************************/

    inline std::string getParameter( const string& name )
    {
      string res="";
      getParameter(name, res);
      return res;
    }

    /**********************************************************************/

    inline bool pageFromString( const string& resultat, unsigned char **webpage, size_t *webpageLen )
    {
      if ( (*webpage = (unsigned char *)malloc( resultat.size()+1 * sizeof(char))) == NULL )
          return false;
      *webpageLen=resultat.size();
      strcpy ((char *)*webpage, resultat.c_str());
      return true;
    }

  
};

#endif
