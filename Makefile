#############################
# LIBNAVAJO                 #
# created by T.DESCOMBES     #
#                    2015    #
##############################

LIBNAVAJO_VERSION=  "\"1.0.0a\""
LIBNAVAJO_BUILD_DATE=`/bin/date +%s`

UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
OS = LINUX
else ifeq ($(UNAME), Darwin)
OS = MACOSX
else
OS = OTHER
endif

DLOPEN_PAM=0
ifeq ($(DLOPEN_PAM),1)
        LIBPAM=-ldl
else
        LIBPAM=-lpam
endif

PRECOMPILER_NAME = bin/navajoPrecompiler
LIB_SHARED_NAME  = lib/libnavajo.so
LIB_STATIC_NAME  = lib/libnavajo.a
EXAMPLE_NAME     = example/example

CXX 	=  g++

ifeq ($(OS),MACOSX)
LIBSSL_DIR = /usr/local/Cellar/openssl/1.0.1j
LIBS 	   = -lz  -L$(LIBSSL_DIR)/lib -lssl -lcrypto $(LIBPAM)
DEFS		=   -D__darwin__ -D__x86__ -fPIC -fno-common -D_REENTRANT -DLIBNAVAJO_SOFTWARE_VERSION=$(LIBNAVAJO_VERSION) -DLIBNAVAJO_BUILD_DATE=$(LIBNAVAJO_BUILD_DATE) 
CXXFLAGS        =  -O3  -Wdeprecated-declarations
else
LIBS 	   = -lz -lssl -lcrypto -pthread $(LIBPAM)
DEFS		=  -DLINUX -Wall -Wno-unused -fexceptions -fPIC -D_REENTRANT -DLIBNAVAJO_SOFTWARE_VERSION=$(LIBNAVAJO_VERSION) -DLIBNAVAJO_BUILD_DATE=$(LIBNAVAJO_BUILD_DATE) 
CXXFLAGS        =   -O4  -Wdeprecated-declarations
endif


CPPFLAGS	= -I. \
                  -I$(LIBSSL_DIR)/include \
                  -Iinclude
		   
LD		=  g++

LDFLAGS         = -Wall -Wno-unused -O3

LIBTOOL = libtool
AR = ar

LIBNAVAJO_OBJS 	= \
		  src/AuthPAM.o \
		  src/LocalRepository.o \
		  src/LogRecorder.o			\
		  src/LogFile.o				\
		  src/LogSyslog.o			\
		  src/LogStdOutput.o		\
		  src/WebServer.o        

PRECOMPILER_OBJS = src/navajoPrecompiler.o


#######################
# DEPENDENCE'S RULES  #
#######################

%.o: %.cc
	$(CXX) -c $< -o $@ $(CXXFLAGS) $(CPPFLAGS) $(DEFS) 

all:: 	$(LIB_SHARED_NAME) $(LIB_STATIC_NAME) $(PRECOMPILER_NAME) $(EXAMPLE_NAME)

clean::
	@echo Remove files...
	@rm -f $(LIB_SHARED_NAME) $(LIB_STATIC_NAME) $(PRECOMPILER_NAME) $(EXAMPLE_NAME)
	@find . \( -name "*.bak" -o -name "*.class" -o -name "*.o" -o -name "core*" -o -name "*~" \) -exec rm -f '{}' \;
	@for i in $(LIBNAVAJO_OBJS); do  rm -f $$i ; done
	$(MAKE) -C example -f Makefile $@

$(LIB_SHARED_NAME): $(LIBNAVAJO_OBJS)
	@rm -f $@;
	@[[ -d lib ]] || mkdir -p lib
	$(LD) $(LDFLAGS) -o $@ -shared $(LIBNAVAJO_OBJS) $(LIBS)

$(LIB_STATIC_NAME): $(LIBNAVAJO_OBJS) 	 
	@rm -f $@
	$(AR) -rvs $@ $(LIBNAVAJO_OBJS)

$(PRECOMPILER_NAME): ${PRECOMPILER_OBJS}
	@rm -f $@
	@[[ -d bin ]] || mkdir -p bin
	${LD} ${LDFLAGS} -o $@ ${PRECOMPILER_OBJS}

$(EXAMPLE_NAME)::
	$(MAKE) -C example -f Makefile



