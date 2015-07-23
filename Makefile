#############################
# LIBNAVAJO                 #
# created by T.DESCOMBES    #
#                    2015   #
#############################

LIBNAVAJO_MVERSION= "1"
LIBNAVAJO_VERSION=  "\"1.0.0\""
LIBNAVAJO_BUILD_DATE=`/bin/date +%s`


UNAME := $(shell uname)
LBITS := $(shell getconf LONG_BIT)

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
LIB_SHARED_NAME  = libnavajo.so.$(LIBNAVAJO_MVERSION)
LIB_SHARED_ALIAS  = libnavajo.so
LIB_STATIC_NAME  = libnavajo.a

EXAMPLE_DIR     = examples

PREFIX       = /usr/local/libnavajo
LIB_DIR      = lib
INC_DIR  = include
BIN_DIR  = bin
MAN_DIR  = man/man3

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
ifeq ($(LBITS),64)
  LIB_DIR=lib64
endif

endif

CPPFLAGS	= -I. \
                  -I$(LIBSSL_DIR)/include \
                  -Iinclude
		   
LD		=  g++

LDFLAGS         = -Wall -Wno-unused -O3

LIBTOOL = libtool
AR = ar
RANLIB=ranlib


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

all:: 	$(LIB_SHARED_NAME) $(LIB_STATIC_NAME) $(PRECOMPILER_NAME) $(EXAMPLE_DIR) docs

clean::
	@echo Remove files...
	@rm -f $(LIB_SHARED_NAME) $(LIB_STATIC_NAME) $(PRECOMPILER_NAME) $(EXAMPLE_NAME) $(EXAMPLE2_NAME)
	@find . \( -name "*.bak" -o -name "*.class" -o -name "*.o" -o -name "core*" -o -name "*~" \) -exec rm -f '{}' \;
	@for i in $(LIBNAVAJO_OBJS); do  rm -f $$i ; done
	$(MAKE) -C examples -f Makefile $@

docs::
	@mkdir -p docs
	@doxygen doxygen.conf

$(LIB_SHARED_NAME): $(LIBNAVAJO_OBJS)
	@rm -f $(LIB_DIR)/$@;
	@[[ -d $(LIB_DIR) ]] || mkdir -p $(LIB_DIR)
	$(LD) $(LDFLAGS) -o $(LIB_DIR)/$@ -shared $(LIBNAVAJO_OBJS) $(LIBS) ; \
        cd $(LIB_DIR); ln -sf $@ $(LIB_SHARED_ALIAS)

$(LIB_STATIC_NAME): $(LIBNAVAJO_OBJS) 	 
	@rm -f $(LIB_DIR)/$@;
	@[[ -d $(LIB_DIR) ]] || mkdir -p $(LIB_DIR)
	$(AR) -rvs $(LIB_DIR)/$@ $(LIBNAVAJO_OBJS)

$(PRECOMPILER_NAME): $(PRECOMPILER_OBJS)
	@rm -f $@
	@[[ -d bin ]] || mkdir -p bin
	${LD} ${LDFLAGS} -o $@ $(PRECOMPILER_OBJS)

$(EXAMPLE_DIR)::
	$(MAKE) -C examples -f Makefile

install: $(LIB_SHARED_NAME) $(LIB_STATIC_NAME) $(PRECOMPILER_NAME)
	-@if [ ! -d $(PREFIX)/$(LIB_DIR) ]; then mkdir -p $(PREFIX)/$(LIB_DIR); fi
	-@if [ ! -d $(PREFIX)/$(INC_DIR) ]; then mkdir -p $(PREFIX)/$(INC_DIR); fi
	-@if [ ! -d $(PREFIX)/$(BIN_DIR) ]; then mkdir -p $(PREFIX)/$(BIN_DIR); fi
	-@if [ ! -d $(PREFIX)/$(MAN_DIR) ]; then mkdir -p $(PREFIX)/$(MAN_DIR); fi
	cp -r $(BIN_DIR) $(PREFIX); chmod 755 $(PREFIX)/$(BIN_DIR)/* 
	cp -r $(INC_DIR) $(PREFIX); find $(PREFIX)/$(INC_DIR)/libnavajo -type f -exec chmod 644 '{}' \; 
	chmod 755 $(PREFIX)/$(INC_DIR)/navajo
	cp -r $(LIB_DIR) $(PREFIX); chmod 755 $(PREFIX)/$(LIB_DIR)/$(LIB_SHARED_NAME) $(PREFIX)/$(LIB_DIR)/$(LIB_SHARED_ALIAS); 
	chmod 644 $(PREFIX)/$(LIB_DIR)/$(LIB_STATIC_NAME); 
	-@(cd $(PREFIX)/$(LIB_DIR); $(RANLIB) $(LIB_STATIC_NAME) || true) >/dev/null 2>&1
	-@(cd $(PREFIX)/$(LIB_DIR); (ldconfig || true)  >/dev/null 2>&1 )
#	cp libnavajo.3 $(man3dir)
#	chmod 644 $(man3dir)/libnavajo.3

uninstall:
	@rm -rf $(PREFIX)/$(INC_DIR)/libnavajo \
	@rm -f $(PREFIX)/$(LIB_DIR)/$(LIB_SHARED_NAME) $(PREFIX)/$(LIB_DIR)/$(LIB_SHARED_ALIAS) $(PREFIX)/$(LIB_DIR)/$(LIB_STATIC_NAME); \
#	cd $(man3dir); rm -f libnavajo.3

