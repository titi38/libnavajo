##############################
# EXAMPLE                    #
# created by T.DESCOMBES     #
#                    2015    #
##############################

OUTPUTFILENAME = PrecompiledRepository.cc
NAVAJO_PRECOMPILER_EXEC = ../bin/navajoPrecompiler

UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
OS = LINUX
else ifeq ($(UNAME), Darwin)
OS = MACOSX
else
OS = OTHER
endif

CXX 	=  g++

ifeq ($(OS),MACOSX)
LIBSSL_DIR = /usr/local/Cellar/openssl/1.0.1j
LIBS       = -lnavajo -L../lib -lz  -L$(LIBSSL_DIR)/lib -lssl -lcrypto $(LIBPAM)
DEFS            =   -D__darwin__ -D__x86__ -fPIC -fno-common -D_REENTRANT
CXXFLAGS        =  -O3  -Wdeprecated-declarations
else
LIBS       = -lnavajo -L../lib -lz -lssl -lcrypto -pthread $(LIBPAM)
DEFS            =  -DLINUX -Wall -Wno-unused -fexceptions -fPIC -D_REENTRANT
CXXFLAGS        =  -O4  -Wdeprecated-declarations
endif


CPPFLAGS	= -I. \
       -I$(LIBSSL_DIR)/include \
		   -I../include

LD		=  g++

LDFLAGS        =  -Wall -Wno-unused -O3   

EXAMPLE_NAME     = example

EXAMPLE_OBJS = \
		  PrecompiledRepository.o \
		  example.o


#######################
# DEPENDENCE'S RULES  #
#######################

%.o: %.cc
	$(CXX) -c $< -o $@ $(CXXFLAGS) $(CPPFLAGS) $(DEFS) 
	
all: $(EXAMPLE_NAME)

PrecompiledRepository.o:
	@echo Generate Cpp files from HTML repository
	@rm -f $(OUTPUTFILENAME)
	@find . \( -name "*~" -o -name "*.old" -o -name "*.bak" \) -exec rm -f '{}' \;
	@cd exampleRepository; ../$(NAVAJO_PRECOMPILER_EXEC) `find . -type f | cut -c 3-` >> ../$(OUTPUTFILENAME) ; cd ..
	$(CXX) -c PrecompiledRepository.cc -o $@ $(CXXFLAGS) $(CPPFLAGS) $(DEFS)
	
$(EXAMPLE_NAME): $(EXAMPLE_OBJS) $(LIB_STATIC_NAME)
	rm -f $@
	$(LD) $(LDFLAGS) -o $@ $(EXAMPLE_OBJS) $(LIB_STATIC_NAME) $(LIBS) 

clean:
	@rm -f $(OUTPUTFILENAME) 
	@for i in $(EXAMPLE_OBJS); do  rm -f $$i ; done






