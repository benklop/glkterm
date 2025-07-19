#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "glkterm" for configuration ""
set_property(TARGET glkterm APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(glkterm PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "C"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib64/libglkterm.a"
  )

list(APPEND _cmake_import_check_targets glkterm )
list(APPEND _cmake_import_check_files_for_glkterm "${_IMPORT_PREFIX}/lib64/libglkterm.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
