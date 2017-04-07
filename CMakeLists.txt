###############  CMakeLists for libnavajo build   ######################
#  Written by T. DESCOMBES (thierry.descombes@gmail.com)               #
#                                                                      #
#  To create an Xcode project:                                         #
#         cmake -G Xcode .                                             #
###############             cmake version          #####################

cmake_minimum_required(VERSION 2.6)

###############              Project name          #####################
project(libnavajo)

###############            version variables           #####################
set(LIBNAVAJO_MVERSION "1")
set(LIBNAVAJO_VERSION "1.5.0")
set(LIBNAVAJO_BUILD_DATE `/bin/date +%s`)

###############            Minimal flags           #####################
IF(${UNIX})
  set (CMAKE_CXX_FLAGS " -Wall -O4 -DLINUX -Wall -Wno-unused -fexceptions -fPIC -D_REENTRANT -DLIBNAVAJO_SOFTWARE_VERSION=\"\\\"${LIBNAVAJO_VERSION}\\\"\" -DDEBUG_TRACES")
ENDIF(${UNIX})

IF(${APPLE})
  set (CMAKE_CXX_FLAGS " -Wall -fPIC -fno-common -O3 -D__darwin__ -D__x86__ -fPIC -fno-common -D_REENTRANT -DLIBNAVAJO_SOFTWARE_VERSION=\"\\\"${LIBNAVAJO_VERSION}\\\"\" ")
ENDIF(${APPLE})

if(NOT DEFINED CMAKE_MACOSX_RPATH)
  set(CMAKE_MACOSX_RPATH 0)
endif()

#######   Check the compiler and set the compile and link flags  #######
set(CMAKE_BUILD_TYPE Debug)

###############          openssl include/lib dir        #####################
find_package(OpenSSL REQUIRED) 
include_directories (${OPENSSL_INCLUDE_DIR})

###############          zlib include/lib dir        #####################
find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})


###############      library extension  #####################
IF(${UNIX})
  SET(LIBRARY_PROPERTIES ${LIBRARY_PROPERTIES}
    SUFFIX ".so"
    )
ENDIF(${UNIX})

IF(${APPLE})
  SET(LIBRARY_PROPERTIES ${LIBRARY_PROPERTIES}
    VERSION "${LIB_VERSION}"
    SOVERSION "${MAJOR_VERSION}"
    SUFFIX ".dylib"
    )
ENDIF(${APPLE})

###############             Library files           #####################

file(GLOB sources_lib
  ${PROJECT_SOURCE_DIR}/src/LocalRepository.cc
  ${PROJECT_SOURCE_DIR}/src/LogRecorder.cc
  ${PROJECT_SOURCE_DIR}/src/LogFile.cc
  ${PROJECT_SOURCE_DIR}/src/LogSyslog.cc
  ${PROJECT_SOURCE_DIR}/src/LogStdOutput.cc
  ${PROJECT_SOURCE_DIR}/src/WebServer.cc
  ${PROJECT_SOURCE_DIR}/src/WebSocketClient.cc
  ${PROJECT_SOURCE_DIR}/src/MPFDParser/Parser.cc
  ${PROJECT_SOURCE_DIR}/src/MPFDParser/Field.cc
  ${PROJECT_SOURCE_DIR}/src/MPFDParser/Exception.cc
  )

 file(GLOB headers_lib ${PROJECT_SOURCE_DIR}/include/libnavajo/* )
 file(GLOB mpfdHeaders_lib 
		      ${PROJECT_SOURCE_DIR}/include/MPFDParser/Field.h 
		      ${PROJECT_SOURCE_DIR}/include/MPFDParser/Exception.h 
		      ${PROJECT_SOURCE_DIR}/include/MPFDParser/Parser.h
 )

set(INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")

###############          Output directory          #####################
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/lib")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin")

###############      build the library     #####################

add_library(navajo SHARED ${sources_lib} ${headers_lib} )
add_library(navajoStatic STATIC ${sources_lib} ${headers_lib} )

set_target_properties (navajo PROPERTIES VERSION ${LIBNAVAJO_VERSION} SOVERSION ${LIBNAVAJO_MVERSION})
set_target_properties(navajoStatic PROPERTIES OUTPUT_NAME navajo)

include_directories (${PROJECT_SOURCE_DIR}/include)

target_link_libraries(navajo ${OPENSSL_LIBRARIES})
target_link_libraries(navajo ${ZLIB_LIBRARIES})

############### install the library ###################
#install(TARGETS navajo DESTINATION lib)

install(FILES ${headers_lib} DESTINATION include/libnavajo COMPONENT headers)
install(FILES ${mpfdHeaders_lib} DESTINATION include/MPFDParser COMPONENT headers)
install(TARGETS navajo LIBRARY DESTINATION lib COMPONENT libraries)

install(TARGETS navajoStatic ARCHIVE DESTINATION lib LIBRARY DESTINATION lib COMPONENT libraries)

############### navajoPrecompiler binary generation ###################
file( GLOB  navajoPrecompiler_source ${PROJECT_SOURCE_DIR}/src/navajoPrecompiler.cc )

add_executable(navajoPrecompiler ${navajoPrecompiler_source})

target_link_libraries(navajoPrecompiler ${OPENSSL_LIBRARIES})
target_link_libraries(navajoPrecompiler ${ZLIB_LIBRARIES})

install(TARGETS navajoPrecompiler DESTINATION bin COMPONENT headers)


############### document file generation ###################
find_package(Doxygen)
if(DOXYGEN_FOUND)
add_custom_target(doc
${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
COMMENT "Generating API documentation with Doxygen" VERBATIM
)
endif(DOXYGEN_FOUND)

