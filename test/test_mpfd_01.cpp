#if 0
#include "../tmp/MPFDParser-1.1.1/Exception.h"
#include "../tmp/MPFDParser-1.1.1/Field.h"
#include "../tmp/MPFDParser-1.1.1/Parser.h"

#include "../tmp/MPFDParser-1.1.1/Parser.cpp"
#include "../tmp/MPFDParser-1.1.1/Field.cpp"
#include "../tmp/MPFDParser-1.1.1/Exception.cpp"

#else

#include "../include/MPFDParser/Exception.h"
#include "../include/MPFDParser/Field.h"
#include "../include/MPFDParser/Parser.h"

#include "../src/MPFDParser/Parser.cc"
#include "../src/MPFDParser/Field.cc"
#include "../src/MPFDParser/Exception.cc"
#endif

int main()
{
  try {
    auto POSTParser = new MPFD::Parser();
    POSTParser->SetTempDirForFileUpload( "/tmp" );
    POSTParser->SetMaxCollectedDataLength( 20 * 1024 );

    //POSTParser->SetContentType( /* Here you know the Content-type: string. And you pass it.*/ );
    POSTParser->SetContentType( "multipart/form-data; boundary=xxx---------------------6d8cd9b0969641f9" );

    const int ReadBufferSize = 1 * 1024;

    char input[ReadBufferSize];

    do {
      // Imagine that you redirected std::cin to accept POST data.
      std::cin.read( input, ReadBufferSize );
      int read = std::cin.gcount();
      std::cout << "Lendo " << read << " bytes" << std::endl;
      if( read ) {
        POSTParser->AcceptSomeData( input, read );
      }

    } while( !std::cin.eof() );

    // Now see what we have:
    std::map<std::string, MPFD::Field *> fields = POSTParser->GetFieldsMap();

    std::cout << "Have " << fields.size() << " fields\n\r";

    std::map<std::string, MPFD::Field *>::iterator it;
    for( it = fields.begin(); it != fields.end(); it++ ) {
      if( fields[it->first]->GetType() == MPFD::Field::TextType ) {
        std::cout << "Got text field: [" << it->first << "], value: [" << fields[it->first]->GetTextTypeContent()
                  << "]\n";
      }
      else {
        std::cout << "Got file field: [" << it->first << "] Filename:[" << fields[it->first]->GetFileName() << "] \n";
      }
    }
  }
  catch( MPFD::Exception e ) {
    std::cout << "Exception " << e.GetError() << std::endl;
    // Parsing input error
    //FinishConnectionProcessing();
    return false;
  }
}
