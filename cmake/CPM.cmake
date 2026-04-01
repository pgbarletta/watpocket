# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2019-2023 Lars Melchior and contributors

set(CPM_DOWNLOAD_VERSION 0.42.1)
set(CPM_HASH_SUM "f3a6dcc6a04ce9e7f51a127307fa4f699fb2bade357a8eb4c5b45df76e1dc6a5")

if(CPM_SOURCE_CACHE)
  set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
elseif(DEFINED ENV{CPM_SOURCE_CACHE})
  set(CPM_DOWNLOAD_LOCATION "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
else()
  set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
endif()

# Expand relative path. This is important if the provided path contains a tilde (~)
get_filename_component(CPM_DOWNLOAD_LOCATION ${CPM_DOWNLOAD_LOCATION} ABSOLUTE)

if(EXISTS "${CPM_DOWNLOAD_LOCATION}")
  include("${CPM_DOWNLOAD_LOCATION}")
  return()
endif()

file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
  ${CPM_DOWNLOAD_LOCATION}
  EXPECTED_HASH SHA256=${CPM_HASH_SUM}
  STATUS CPM_DOWNLOAD_STATUS
  LOG CPM_DOWNLOAD_LOG)

list(GET CPM_DOWNLOAD_STATUS 0 CPM_DOWNLOAD_RESULT)
if(NOT CPM_DOWNLOAD_RESULT EQUAL 0)
  if(NOT BUILD_TESTING AND NOT WATPOCKET_BUILD_CLI)
    message(WARNING "CPM.cmake download failed; using offline no-op fallback for wheel builds")

    function(cpmaddpackage)
      message(FATAL_ERROR "CPMAddPackage is unavailable in the offline fallback")
    endfunction()

    return()
  endif()

  message(FATAL_ERROR "Failed to download CPM.cmake: ${CPM_DOWNLOAD_LOG}")
endif()

include(${CPM_DOWNLOAD_LOCATION})
