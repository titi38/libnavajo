# - Find libnavajo
#   Find the libnavajo includes and client library
# This module defines
#  libnavajo_INCLUDE_DIRS
#  libnavajo_LIBRARIES
#  libnavajo_BIN
#  libnavajo_FOUND

include (FindPackageHandleStandardArgs)

find_path (libnavajo_INCLUDE_DIRS libnavajo/libnavajo.hh
    NAME
        libnavajo.hh
    PATHS
        /usr/include
        /include
        /usr/local/include
    DOC
        "Directory for libnavajo headers"
)

find_program(libnavajo_BIN libnavajo/navajoPrecompiler
  NAME
      navajoPrecompiler
  PATH
      /usr/bin
      /bin
      /usr/local/bin
)

find_library (libnavajo_LIBRARIES
    NAMES
        navajo
    PATHS
        /usr/lib/libnavajo
        /lib/libnavajo
        /usr/local/lib/libnavajo
)

FIND_PACKAGE_HANDLE_STANDARD_ARGS("libnavajo"
    "libnavajo couldn't be found"
    libnavajo_LIBRARIES
    libnavajo_INCLUDE_DIRS
)

mark_as_advanced (libnavajo_INCLUDE_DIR libnavajo_LIBRARY)



MACRO (MACRO_ADD_INTERFACES _output_list)

  FOREACH(_in_FILE ${ARGN})

    ADD_CUSTOM_COMMAND(
      OUTPUT ${_in_FILE}
      COMMAND rm -f ${_in_FILE}
      COMMAND ${libnavajo_BIN} html >> ${_in_FILE}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    SET(${_output_list} ${${_output_list}} ${_in_FILE})

  ENDFOREACH(_in_FILE ${ARGN})



ENDMACRO (MACRO_ADD_INTERFACES)
