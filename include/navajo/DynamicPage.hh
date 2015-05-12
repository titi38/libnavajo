//********************************************************
/**
 * @file  DynamicPage.hh
 *
 * @brief Dynamic page définition (abstract class)
 *
 * @author T.Descombes (descombes@lpsc.in2p3.fr)
 *
 * @version 1  
 * @date 19/02/15
 */
//********************************************************

#ifndef DYNAMICPAGE_HH_
#define DYNAMICPAGE_HH_

#include <string>
#include <typeinfo> 

class DynamicPage
{

  public:
    DynamicPage() {};
    virtual bool getPage(HttpRequest* request, HttpResponse *response) = 0;
    

    /**********************************************************************/
     
    template<class T> static inline T getValue (string s)
    { 
      if (!s.length())
       throw std::bad_cast();
       
	     std::istringstream iss(s); 
	     T tmp; iss>>tmp;   
	       
	     if(iss.fail())
	       throw std::bad_cast();
	      
	      return tmp;

	  };
    
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
