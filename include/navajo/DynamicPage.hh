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

  public:
    DynamicPage() {};
    virtual bool getPage(HttpRequest* request, HttpResponse *response) = 0;
    

    /**********************************************************************/
     
    template<class T> static inline T getValue (string s)
        { if (!s.length()) return (T)-1; 
	     std::istringstream iss(s); T tmp; iss>>tmp;  return tmp; };
    
    /**********************************************************************/


    inline bool fromString( const string& resultat, HttpResponse *response )
    {
      size_t webpageLen;
      unsigned char *webpage;
      if ( (webpage = (unsigned char *)malloc( resultat.size()+1 * sizeof(char))) == NULL )
          return false;
      webpageLen=resultat.size();
      strcpy ((char *)webpage, resultat.c_str());
      response->setContent (webpage, webpageLen);
      return true;
    }

  
};

#endif
