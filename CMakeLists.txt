cmake_minimum_required (VERSION 2.6)
project (CXXLASTFM_NATIONALITIES) 

# Find headers
find_path(SCROBBLER_INCLUDE_DIR scrobbler/library.h /usr/include)
find_path(LIBXML2_INCLUDE_DIR libxml/parser.h /usr/include/libxml2)
include_directories(${SCROBBLER_INCLUDE_DIR} ${LIBXML2_INCLUDE_DIR})

# Find libraries
find_library(CGICC_LIBRARY NAMES cgicc PATH /usr/lib /usr/local/lib) 
find_library(SROBBLER_LIBRARY NAMES scobbler PATH /usr/lib /usr/local/lib)
find_library(LIBXML2_LIBRARY NAMES xml2 PATH /usr/lib /usr/local/lib)
find_library(CURL_LIBRARY NAMES curl PATH /usr/lib /usr/local/lib)
find_library(MYSQL_LIBRARY NAMES mysqlclient PATH /usr/lib /usr/local/lib)

add_executable(generate-stats src/generate-stats.cpp)
target_link_libraries(generate-stats ${CGICC_LIBRARY} ${SROBBLER_LIBRARY}
  ${LIBXML2_LIBRARY} ${CURL_LIBRARY} ${MYSQL_LIBRARY})

