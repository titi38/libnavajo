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



MACRO (MACRO_ADD_INTERFACES _precompil_repo)

  FOREACH(_in_repo ${ARGN})

	
    SET(_precompile_name "PrecompiledRepository_${_in_repo}.cc" )

    file(GLOB htmlfiles ${_in_repo}/* )

    ADD_CUSTOM_COMMAND(
      OUTPUT ${_precompile_name}
      COMMAND rm -f ${_precompile_name}
      COMMAND ${libnavajo_BIN} ${_in_repo} >> ${_precompile_name}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      MAIN_DEPENDENCY ${htmlfiles}
    )

    SET(${_precompil_repo} ${${_precompil_repo}} ${_precompile_name})

  ENDFOREACH(_in_repo ${ARGN})



ENDMACRO (MACRO_ADD_INTERFACES)
