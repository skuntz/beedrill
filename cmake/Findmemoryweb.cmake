# - Try to find memoryweb
#  Once done this will define
#  MEMORYWEB_FOUND - System has memoryweb
#  MEMORYWEB_INCLUDE_DIRS - The memoryweb include directories
#  MEMORYWEB_LIBRARIES - The libraries needed to use memoryweb
#  MEMORYWEB_DEFINITIONS - Compiler switches required for using memoryweb

# Find Emu install dir
if (LLVM_CILK)
    set(EMU_PREFIX ${LLVM_CILK})
else()
    set(EMU_PREFIX "/usr/local/emu")
endif()

# Append platform-specific suffix
if (CMAKE_SYSTEM_NAME STREQUAL "Emu1")
  find_path(MEMORYWEB_INCLUDE_DIR
    NAMES memoryweb/memoryweb.h
    HINTS ${EMU_PREFIX}/include
    )
  find_library(MEMORYWEB_LIBRARY
    NAMES memoryweb
    HINTS ${EMU_PREFIX}/lib
    )
  include(FindPackageHandleStandardArgs)
  # handle the QUIETLY and REQUIRED arguments and set memoryweb_FOUND to TRUE
  # if all listed variables are TRUE
  find_package_handle_standard_args(memoryweb
    DEFAULT_MSG
    MEMORYWEB_LIBRARY
    MEMORYWEB_INCLUDE_DIR
    )
  
  mark_as_advanced(MEMORYWEB_INCLUDE_DIR MEMORYWEB_LIBRARY )
  set(MEMORYWEB_LIBRARIES ${MEMORYWEB_LIBRARY} )
  set(MEMORYWEB_INCLUDE_DIRS ${MEMORYWEB_INCLUDE_DIR} )
endif()
