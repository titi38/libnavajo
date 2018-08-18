##############################
# EXAMPLE 5                  #
# created by T.DESCOMBES     #
#                    2016    #
##############################

NAVAJO_PRECOMPILER_EXEC = ../build/bin/navajoPrecompiler

UNAME := $(shell uname)
LBITS := $(shell getconf LONG_BIT)

ifeq ($(UNAME), Linux)
OS = LINUX
else ifeq ($(UNAME), Darwin)
OS = MACOSX
else
OS = OTHER
endif

LIB_DIR      = lib
CXX 	=  g++

ifeq ($(OS),MACOSX)
LIBSSL_DIR = /usr/local/Cellar/openssl/1.0.1j
LIBS       = -lnavajo -L../build/$(LIB_DIR) -lz  -L$(LIBSSL_DIR)/lib -lssl -lcrypto 
DEFS       =   -D__darwin__ -D__x86__ -fPIC -fno-common -D_REENTRANT
CXXFLAGS   =  -O3  -Wdeprecated-declarations
else
#ifeq ($(LBITS),64)
#  LIB_DIR=lib64
#endif
LIBS       = -lnavajo -L../build/$(LIB_DIR) -lz -lssl -lcrypto -pthread 
DEFS            =  -DLINUX -Wall -Wno-unused -fexceptions -fPIC -D_REENTRANT
CXXFLAGS        =  -O4  -Wdeprecated-declarations
endif


CPPFLAGS	= -I. \
            -I$(LIBSSL_DIR)/include \
            -I../include
#            -I../../MPFDParser-1.1.1

LD = g++
LDFLAGS        =  -Wall -Wno-unused -O3
EXAMPLE_NAME   = example

EXAMPLE_OBJS = \
		  example.o


#######################
# DEPENDENCE'S RULES  #
#######################

%.o: %.cc
	$(CXX) -c $< -o $@ $(CXXFLAGS) $(CPPFLAGS) $(DEFS) 

all: $(EXAMPLE_NAME) test_mpfd_01

$(EXAMPLE_NAME): $(EXAMPLE_OBJS) $(LIB_STATIC_NAME)
	rm -f $@
	mkdir -p upload
	$(LD) $(LDFLAGS) -o $@ $(EXAMPLE_OBJS) $(LIB_STATIC_NAME) $(LIBS) 

clean:
	@rm -f $(EXAMPLE_NAME) 
	@for i in $(EXAMPLE_OBJS); do  rm -f $$i ; done

test2:
	$(CXX) test_mpfd_01.cpp -o test_mpfd_01 $(CXXFLAGS) $(CPPFLAGS) $(DEFS) 
	./test_mpfd_01 < curl/form1.multipart

run: clean $(EXAMPLE_NAME)
	LD_LIBRARY_PATH=../build/lib/:$LD_LIBRARY_PATH ./$(EXAMPLE_NAME) | tee log

rebuild:
	make -C ../build clean all




