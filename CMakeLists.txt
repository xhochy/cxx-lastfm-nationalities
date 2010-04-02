cmake_minimum_required (VERSION 2.6)
project (CXXLASTFM_NATIONALITIES) 

###############
## scrobbler ##
###############

find_library(SCROBBLER_LIBRARY NAMES scrobbler PATH /usr/lib /usr/local/lib)
if(NOT SCROBBLER_LIBRARY)
    message(FATAL_ERROR "Could not find scrobbler library.")
endif(NOT SCROBBLER_LIBRARY)

find_path(SCROBBLER_INCLUDE_DIR scrobbler/library.h /usr/include /usr/local/include)
if(NOT SCROBBLER_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find scrobbler headers.")
endif(NOT SCROBBLER_INCLUDE_DIR)
include_directories(${SCROBBLER_INCLUDE_DIR})

#############
## LibXML2 ##
#############

find_library(LIBXML2_LIBRARY NAMES xml2 PATH /usr/lib /usr/local/lib)
if(NOT LIBXML2_LIBRARY)
    message(FATAL_ERROR "Could not find libxml2 library.")
endif(NOT LIBXML2_LIBRARY)

find_path(LIBXML2_INCLUDE_DIR libxml/parser.h /usr/include/libxml2)
if(NOT LIBXML2_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find libxml2 headers.")
endif(NOT LIBXML2_INCLUDE_DIR)
include_directories(${LIBXML2_INCLUDE_DIR})

##########
## cURL ##
##########

find_package(CURL REQUIRED)

if(NOT CURL_FOUND)
    message(FATAL_ERROR "CURL library/headers not found")
endif(NOT CURL_FOUND)
include_directories(${CURL_INCLUDE_DIR})

###########
## MySQL ##
###########

find_library(MYSQL_LIBRARY NAMES mysqlclient PATH /usr/lib /usr/local/lib)
if(NOT MYSQL_LIBRARY)
    message(FATAL_ERROR "MySQL library not found")
endif(NOT MYSQL_LIBRARY)

find_path(MYSQL_INCLUDE_DIR mysql/mysql.h)
if(NOT MYSQL_INCLUDE_DIR)
    mesaage(FATAL_ERROR "MySQL headers not found")
endif(NOT MYSQL_INCLUDE_DIR)

###########
## BOOST ##
###########

find_package(Boost 1.34.0)
include_directories(${Boost_INCLUDE_DIR})

# Find libraries
find_library(CGICC_LIBRARY NAMES cgicc PATH /usr/lib /usr/local/lib) 

add_library(lastfm-nationalities src/base64.cpp src/ArtistData.cpp src/Main.cpp
  src/Main-mysql.cpp src/unicode.cpp)

add_executable(generate-stats.cgi src/generate-stats.cpp)
add_executable(redirect-image.cgi src/redirect-image.cpp)
add_executable(redirect-image-list.cgi src/redirect-image-list.cpp)
add_executable(redirect-image-big.cgi src/redirect-image-big.cpp)
target_link_libraries(generate-stats.cgi lastfm-nationalities ${CGICC_LIBRARY} 
  ${SCROBBLER_LIBRARY} ${LIBXML2_LIBRARY} ${CURL_LIBRARY} ${MYSQL_LIBRARY})
target_link_libraries(redirect-image.cgi lastfm-nationalities ${CGICC_LIBRARY} 
  ${SCROBBLER_LIBRARY} ${LIBXML2_LIBRARY} ${CURL_LIBRARY} ${MYSQL_LIBRARY})
target_link_libraries(redirect-image-list.cgi lastfm-nationalities ${CGICC_LIBRARY} 
  ${SCROBBLER_LIBRARY} ${LIBXML2_LIBRARY} ${CURL_LIBRARY} ${MYSQL_LIBRARY})
target_link_libraries(redirect-image-big.cgi lastfm-nationalities ${CGICC_LIBRARY} 
  ${SCROBBLER_LIBRARY} ${LIBXML2_LIBRARY} ${CURL_LIBRARY} ${MYSQL_LIBRARY})

INSTALL(TARGETS generate-stats.cgi redirect-image.cgi redirect-image-list.cgi 
  redirect-image-big.cgi DESTINATION bin)

