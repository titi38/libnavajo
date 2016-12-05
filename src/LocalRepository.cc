//********************************************************
/**
 * @file  LocalRepository.cc 
 *
 * @brief Handles local web repository
 *
 * @author T.Descombes (thierry.descombes@gmail.com)
 *
 * @version 1        
 * @date 19/02/15
 */
//********************************************************


#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <fstream>
#include <streambuf>
#include <sstream>
#include "libnavajo/LogRecorder.hh"
#include "libnavajo/LocalRepository.hh"


/**********************************************************************/

LocalRepository::LocalRepository(const std::string& alias, const std::string& dirPath)
{
  char resolved_path[4096];

  pthread_mutex_init(&_mutex, NULL); 

  aliasName=alias;
  while (aliasName.size() && aliasName[0]=='/') aliasName.erase(0, 1);
  while (aliasName.size() && aliasName[aliasName.size()-1]=='/') aliasName.erase(aliasName.size() - 1);

  if (realpath(dirPath.c_str(), resolved_path) != NULL)
  {
    fullPathToLocalDir=resolved_path;
    loadFilename_dir(aliasName, fullPathToLocalDir);
  }
}

/**********************************************************************/

void LocalRepository::reload()
{
  pthread_mutex_lock( &_mutex);
  filenamesSet.clear();
  loadFilename_dir(aliasName, fullPathToLocalDir);
  pthread_mutex_unlock( &_mutex);
}

/**********************************************************************/

bool LocalRepository::loadFilename_dir (const std::string& alias, const std::string& path, const std::string& subpath)
{
    struct dirent *entry;
    DIR *dir;
    struct stat s;
    std::string fullPath=path+subpath;

    dir = opendir (fullPath.c_str());
    if (dir == NULL) return false;
    while ((entry = readdir (dir)) != NULL ) 
    {
      if (!strcmp(entry->d_name,".") || !strcmp(entry->d_name,"..") || !strlen(entry->d_name)) continue;

      std::string filepath=fullPath+'/'+entry->d_name;

      if (stat(filepath.c_str(), &s) == -1) 
      {
        NVJ_LOG->append(NVJ_ERROR,std::string("LocalRepository - stat error reading file '")+filepath+"': "+std::string(strerror(errno)));
        continue;
      }

      int type=s.st_mode & S_IFMT;
      if (type == S_IFREG || type == S_IFLNK)
      {
        std::string filename=alias+subpath+"/"+entry->d_name;
        while (filename.size() && filename[0]=='/')
          filename.erase(0, 1);
        filenamesSet.insert(filename);
      }

      if (type == S_IFDIR)
        loadFilename_dir(alias, path, subpath+"/"+entry->d_name);
    }

    closedir (dir);

    return true;
}

/**********************************************************************/

bool LocalRepository::fileExist(const std::string& url)
{
  return filenamesSet.find(url) != filenamesSet.end();
}

/**********************************************************************/

bool LocalRepository::getFile(HttpRequest* request, HttpResponse *response)
{
  std::string url = request->getUrl();
  size_t webpageLen;
  unsigned char *webpage;
  pthread_mutex_lock( &_mutex );

  if ( url.compare(0, aliasName.size(), aliasName) || !fileExist(url) )
    { pthread_mutex_unlock( &_mutex); return false; };
    
  pthread_mutex_unlock( &_mutex);

  std::string resultat, filename=url;

  if (aliasName.size())
    filename.replace(0, aliasName.size(), fullPathToLocalDir);
  else
    filename=fullPathToLocalDir+'/'+filename;

  FILE *pFile = fopen ( filename.c_str() , "rb" );
  if (pFile==NULL)
  {
    char logBuffer[150];
    snprintf(logBuffer, 150, "Webserver : Error opening file '%s'", filename.c_str() );
    NVJ_LOG->append(NVJ_ERROR, logBuffer);
    return false;
  }

  // obtain file size.
  fseek (pFile , 0 , SEEK_END);
  webpageLen = ftell (pFile);
  rewind (pFile);

  if ( (webpage = (unsigned char *)malloc( webpageLen+1 * sizeof(char))) == NULL )
    return false;
  size_t nb=fread (webpage,1,webpageLen,pFile);
  if (nb != webpageLen)
  {
    char logBuffer[150];
    snprintf(logBuffer, 150, "Webserver : Error accessing file '%s'", filename.c_str() );
    NVJ_LOG->append(NVJ_ERROR, logBuffer);
    return false;
  }
  
  fclose (pFile);
  response->setContent (webpage, webpageLen);
  return true;
}




