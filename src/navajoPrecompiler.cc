//********************************************************
/**
 * @file  navajoPrecompiler.cc 
 *
 * @brief Tool to generate a C++ PrecompiledRepository class
 *
 * @author T.Descombes (thierry.descombes@gmail.com)
 *
 * @version 1        
 * @date 19/02/15
 */
//********************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <vector>
#include <string>
#include <algorithm>

void dump_buffer(FILE *f, unsigned n, const unsigned char* buf)
{
  int cptLine = 0;
  fputs("    ", f);
  while (n-- > 0)
  {
    cptLine ++;
    fprintf(f, "0x%02X", *buf++);
    if (n>0) fprintf(f, ", ");
    if (cptLine == 16)
    {
      fputs("\n    ", f);
      cptLine = 0;
    }
  }
}

char * str_replace_first(char * buffer, const char * s, const char * by)
{
  char * p = strstr(buffer, s), * ret = NULL;

  if (p != NULL)
  {
    size_t len_p = strlen(p), len_s = strlen(s), len_by = strlen(by);

    if (len_s != len_by)
    {
      /* ajuster la taille de la partie 's' pur pouvoir placer by */
      memmove(p + len_by, p + len_s, len_p);
    }

    /* rempacer s par by */
    strncpy(p, by, len_by);
    ret = buffer;
  }

  return ret;
}

typedef struct
{
  std::string* URL;
  std::string* varName;
  size_t length;
}  ConversionEntry;

std::vector< std::string > filenamesVec;

/**********************************************************************/

bool loadFilename_dir (const std::string& path, const std::string& subpath="")
{
    struct dirent *entry;
    int ret = 1;
    DIR *dir;
    struct stat s;

    std::string fullPath=path+'/'+subpath;

    std::string spath=subpath;
    if (subpath != "")
      spath+='/';

    dir = opendir (fullPath.c_str());
    if (dir == NULL) return false;
    while ((entry = readdir (dir)) != NULL ) 
    {
      if (!strcmp(entry->d_name,".") || !strcmp(entry->d_name,"..") || !strlen(entry->d_name)) continue;

      std::string filepath=fullPath+'/'+entry->d_name;

      if (stat(filepath.c_str(), &s) == -1) 
      {
        perror("stat");
        exit(EXIT_FAILURE);
      }

      int type=s.st_mode & S_IFMT;
      if (type == S_IFREG || type == S_IFLNK)
        filenamesVec.push_back(spath+entry->d_name);

      if (type == S_IFDIR)
        loadFilename_dir(path, spath+entry->d_name);
    }
    return true;
}

/**********************************************************************/

void parseDirectory( const std::string& dirPath )
{
  char resolved_path[4096];
  
  if (realpath(dirPath.c_str(), resolved_path) == NULL)
    return ;

  if (!loadFilename_dir(resolved_path))
    return ;
}

/**********************************************************************/
/**
* @brief  Main function
* @param argc the number of web files
* @param argv the filenames (with absolute path)
* @return 0 upon exit success
*/ 
int main (int argc, char *argv[])
{
  if (argc <= 1)
  {
    printf("Usage: %s [dir ...]\n", argv[0]);
//    printf("   ex: %s `find . -type f | cut -c 3-` > PrecompiledRepository.cc\n\n",  argv[0]);
    fflush(NULL);
    exit(EXIT_FAILURE);
  }

  std::string directory=argv[1];
  while (directory.length() && directory[directory.length()-1] == '/')
    directory = directory.substr(0,directory.length()-1);
  parseDirectory(directory); 
  if (!filenamesVec.size())
  {
    fprintf(stderr, "ERROR: The directory '%s' is empty or not found !\n", directory.c_str()); 
    exit(EXIT_FAILURE);
  }

  ConversionEntry* conversionTable= (ConversionEntry*) malloc( filenamesVec.size() * sizeof(ConversionEntry));

  fprintf (stdout, "#include \"libnavajo/PrecompiledRepository.hh\"\n\n");
  fprintf (stdout, "namespace webRepository\n{\n");

  for (size_t i = 0; i < filenamesVec.size(); i++)
  {

    FILE * pFile;
    size_t lSize;
    unsigned char * buffer;

    std::string filename=directory + '/' + filenamesVec[i];

    pFile = fopen ( filename.c_str() , "rb" );
    if (pFile==NULL)
    { fprintf(stderr, "ERROR: can't read file: %s\n", filenamesVec[i].c_str()); exit (1); }

    // obtain file size.
    fseek (pFile , 0 , SEEK_END);
    lSize = ftell (pFile);
    rewind (pFile);

    // allocate memory to contain the whole file.
    buffer = (unsigned char*) malloc ((lSize+1)*sizeof(unsigned char));
    if (buffer == NULL)
     { fprintf(stderr, "ERROR: can't malloc reading file: %s\n", filenamesVec[i].c_str()); exit (2); }

    // copy the file into the buffer.
    if (fread (buffer,1,lSize,pFile) != lSize)
    {
      fprintf(stderr,"\nCan't read file %s ... ABORT !\n", filenamesVec[i].c_str() );
      exit(1);
    };

    std::string outFilename = filenamesVec[i];
    std::replace( outFilename.begin(), outFilename.end(), '.', '_'); 
    std::replace( outFilename.begin(), outFilename.end(), '/', '_'); 
    std::replace( outFilename.begin(), outFilename.end(), ' ', '_'); 
    std::replace( outFilename.begin(), outFilename.end(), '-', '_'); 

    fprintf (stdout, "  static const unsigned char %s[] =\n", outFilename.c_str());
    fprintf (stdout, "  {\n" );
    dump_buffer(stdout,lSize, const_cast<unsigned char*>(buffer));
    fprintf (stdout, "\n  };\n\n");
    fclose (pFile);
    free (buffer);

    (*(conversionTable+i)).URL = new std::string(filenamesVec[i]);
    (*(conversionTable+i)).varName = new std::string(outFilename);
    (*(conversionTable+i)).length = lSize;
  }
  
  fprintf (stdout, "}\n\n");
  
  fprintf (stdout, "PrecompiledRepository::IndexMap PrecompiledRepository::indexMap;\n");
  fprintf (stdout, "std::string PrecompiledRepository::location;\n");
  fprintf (stdout,"\nvoid PrecompiledRepository::initIndexMap()\n{\n");

  for (size_t i = 0; i < filenamesVec.size(); i++)
  {
    fprintf (stdout,"    indexMap.insert(IndexMap::value_type(\"%s\",PrecompiledRepository::WebStaticPage((const unsigned char*)&webRepository::%s, sizeof webRepository::%s)));\n", (*(conversionTable+i)).URL->c_str(), (*(conversionTable+i)).varName->c_str(), (*(conversionTable+i)).varName->c_str() );
    delete (*(conversionTable+i)).URL;
    delete (*(conversionTable+i)).varName;
  }
  fprintf (stdout,"}\n");
  free (conversionTable);

  return (EXIT_SUCCESS);
}

