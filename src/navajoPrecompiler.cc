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
  char *URL;
  char *varName;
  size_t length;
}  ConversionEntry;


/**
* @brief  Main function
* @param argc the number of web files
* @param argv the filenames (with absolute path)
* @return 0 upon exit success
*/ 
int main (int argc, char *argv[])
{
	int count;

	if (argc <= 1)
	{
		printf("Usage: %s [file ...]\n", argv[0]);
		printf("   ex: %s `find . -type f | cut -c 3-` > PrecompiledRepository.cc\n\n",  argv[0]);
		fflush(NULL);
		exit(EXIT_FAILURE);
	}

	ConversionEntry* conversionTable= (ConversionEntry*) malloc( (argc-1) * sizeof(ConversionEntry));

	fprintf (stdout, "#include \"navajo/PrecompiledRepository.hh\"\n\n");

	fprintf (stdout, "namespace webRepository\n{\n");
	for (count = 1; count < argc; count++)
	{

		FILE * pFile;
		size_t lSize;
		unsigned char * buffer;

		pFile = fopen ( argv[count] , "rb" );
		if (pFile==NULL)
		{ fprintf(stderr, "ERROR: can't read file: %s\n", argv[count]); exit (1); }

		// obtain file size.
		fseek (pFile , 0 , SEEK_END);
		lSize = ftell (pFile);
		rewind (pFile);

		// allocate memory to contain the whole file.
		buffer = (unsigned char*) malloc (lSize);
		if (buffer == NULL)
 		{ fprintf(stderr, "ERROR: can't malloc reading file: %s\n", argv[count]); exit (2); }

		// copy the file into the buffer.
		if (fread (buffer,1,lSize,pFile) != lSize)
		{
		  fprintf(stderr,"\nCan't read file %s ... ABORT !\n", argv[count] );
		  exit(1);
		};

		/*** the whole file is loaded in the buffer. ***/

		/*** write it in the C++ format ***/
		char outFilename[100];
		snprintf( outFilename, 100, "%s", argv[count] );
		FILE *outFile = stdout; //fopen ( outFilename  , "w" );
		while (str_replace_first(outFilename, ".", "_") != NULL);
		while (str_replace_first(outFilename, "/", "_") != NULL);
		while (str_replace_first(outFilename, " ", "_") != NULL);
		while (str_replace_first(outFilename, "-", "_") != NULL);
		fprintf (stdout, "  static const unsigned char %s[] =\n", outFilename);
		fprintf (stdout, "  {\n" );
		dump_buffer(stdout,lSize, const_cast<unsigned char*>(buffer));
		fprintf (stdout, "\n  };\n\n");
		fclose (pFile);
		free (buffer);

		(*(conversionTable+count-1)).URL = argv[count];
		(*(conversionTable+count-1)).varName = (char*) malloc ((strlen(outFilename)+1)*sizeof(char));
		(*(conversionTable+count-1)).length = lSize;
		strcpy ((*(conversionTable+count-1)).varName, outFilename);
	}
	
  fprintf (stdout, "}\n\n");
  
  fprintf (stdout, "PrecompiledRepository::IndexMap PrecompiledRepository::indexMap;\n");
  fprintf (stdout,"\nvoid PrecompiledRepository::initIndexMap()\n{\n");
  
	for (count = 1; count < argc; count++)
	{
		fprintf (stdout,"    indexMap.insert(IndexMap::value_type(\"%s\",PrecompiledRepository::WebStaticPage((const unsigned char*)&webRepository::%s, sizeof webRepository::%s)));\n", (*(conversionTable+count-1)).URL, (*(conversionTable+count-1)).varName, (*(conversionTable+count-1)).varName );
		free ((*(conversionTable+count-1)).varName);
	}
	fprintf (stdout,"}\n");
  free (conversionTable);

	return (EXIT_SUCCESS);
}

