#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Check::checkShared" for configuration "Release"
set_property(TARGET Check::checkShared APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(Check::checkShared PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/checkDynamic.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/checkDynamic.dll"
  )

list(APPEND _cmake_import_check_targets Check::checkShared )
list(APPEND _cmake_import_check_files_for_Check::checkShared "${_IMPORT_PREFIX}/lib/checkDynamic.lib" "${_IMPORT_PREFIX}/bin/checkDynamic.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
