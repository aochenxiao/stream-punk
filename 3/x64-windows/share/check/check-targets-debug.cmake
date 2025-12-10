#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Check::checkShared" for configuration "Debug"
set_property(TARGET Check::checkShared APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(Check::checkShared PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/debug/lib/checkDynamic.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/bin/checkDynamic.dll"
  )

list(APPEND _cmake_import_check_targets Check::checkShared )
list(APPEND _cmake_import_check_files_for_Check::checkShared "${_IMPORT_PREFIX}/debug/lib/checkDynamic.lib" "${_IMPORT_PREFIX}/debug/bin/checkDynamic.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
