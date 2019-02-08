##############################
# EXAMPLE                    #
# created by T.DESCOMBES     #
# created by J.COURTAT       #
#                    2017    #
##############################

UNAME := $(shell uname)

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
LIBS       = -lnavajo -L../../$(LIB_DIR) -lz $(shell pkg-config --libs-only-L libjwt) $(shell pkg-config --libs-only-l libjwt)
DEFS            =   -D__darwin__ -D__x86__ -fPIC -fno-common -D_REENTRANT
CXXFLAGS        =  -O3  -Wdeprecated-declarations
else
LIBS       = -lnavajo -L../../$(LIB_DIR) -lz $(shell pkg-config --libs-only-L libjwt) $(shell pkg-config --libs-only-l libjwt) -pthread
DEFS            =  -DLINUX -Wall -Wno-unused -fexceptions -fPIC -D_REENTRANT
CXXFLAGS        =  -O3  -Wdeprecated-declarations
endif

CPPFLAGS	= -I. \
                   $(shell pkg-config --cflags libjwt)\
		  -I../../include         \
                  -I../../MPFDParser-1.1.1  

LD		=  g++

LDFLAGS        =  -Wall -Wno-unused -O4   

EXAMPLE_NAME     = example

EXAMPLE_OBJS = \
		  example.o

#######################
# DEPENDENCE'S RULES  #
#######################

%.o: %.cc
	$(CXX) -c $< -o $@ $(CXXFLAGS) $(CPPFLAGS) $(DEFS) 

all: $(EXAMPLE_NAME)

$(EXAMPLE_NAME): $(EXAMPLE_OBJS) $(LIB_STATIC_NAME)
	rm -f $@
	$(LD) $(LDFLAGS) -o $@ $(EXAMPLE_OBJS) $(LIB_STATIC_NAME) $(LIBS) 

clean:
	@for i in $(EXAMPLE_OBJS); do  rm -f $$i ; done
