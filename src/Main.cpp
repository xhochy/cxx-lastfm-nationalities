#include "Main.h"
#include "base64.h"

#include <boost/format.hpp>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <libxml/parser.h>
#include <scrobbler/base.h>
#include <scrobbler/library.h>
#include <stdexcept>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

using namespace LastFM;
using namespace Scrobbler;
using namespace std;

Main::Main() :
  m_artist_sel_stmt(NULL)
{
  LIBXML_TEST_VERSION
  this->LoadAPIKey();
  this->InitMySQL();
}

void Main::LoadAPIKey()
{
  // Read api-key
  ifstream api_file;
  api_file.open("/etc/cxx-lastfm-nationalities/api-key.txt", ifstream::in);
  char key[256];
  api_file.getline(key, 256);
  scobbler_api_key = string(key);
  api_file.close();
}

Main::~Main()
{
  this->CleanupMySQL();

  // Cleanup LibXML
  xmlCleanupParser();
}

std::vector<Artist> Main::renewArtistsCache(string username, string cache_file)
{
  std::vector<Artist> artists = Library(username).artists();

  // write to cache
  // Create a new XML buffer, to which the XML document will be written
  xmlBufferPtr buf = xmlBufferCreate();
  if (buf == NULL)
    throw runtime_error("Error creating the XML buffer");
  // Create a new XmlWriter for memory, with no compression.
  xmlTextWriterPtr writer = xmlNewTextWriterMemory(buf, 0);
  if (writer == NULL)
    throw runtime_error("Error creating the xml writer");
  // Start the document with the xml default for the version, encoding 
  // UTF-8 and the default for the standalone declaration.
  int rc = xmlTextWriterStartDocument(writer, NULL, "UTF-8", NULL);
  if (rc < 0)
    throw runtime_error("Error at xmlTextWriterStartDocument");
  rc = xmlTextWriterStartElement(writer, BAD_CAST "artists");
  if (rc < 0) throw runtime_error("Error at xmlTextWriterStartElement");
 
  for (vector<Artist>::const_iterator i = artists.begin(); i != artists.end(); ++i) {
    i->writeXml(writer);
  }
  
  rc = xmlTextWriterEndDocument(writer);
  if (rc < 0) throw runtime_error("Error at xmlTextWriterEndDocument");
  xmlFreeTextWriter(writer);

  gzFile zipfile = gzopen(cache_file.c_str(), "wb9");
  gzputs(zipfile, (const char *)buf->content);
  gzclose(zipfile);
  
  return artists;
}

vector<ArtistData> Main::renewResultCache(string username, string result_cache_file)
{
  vector<ArtistData> valuableData;
  struct stat fileinfo;
  string username_encoded = base64_encode(reinterpret_cast<const unsigned char*>(username.c_str()), username.length());
  
  // Check for cached artists
  string cache_file = string(cache_dir) + "/library-artists-" + username_encoded + ".xml.gz";
  int errcode = stat(cache_file.c_str(), &fileinfo);
  if (errno != ENOENT && errcode == -1)
    throw runtime_error("Something went wrong in the caching area.");
  std::vector<Artist> artists;
  // Cache library for *7 days*
  if (errno == ENOENT || fileinfo.st_mtime + 7*24*60*60 < time(NULL)) {
    // Cache is outdated
    artists = this->renewArtistsCache(username, cache_file);
  } else {
    // Read from cache
    gzFile zipfile = gzopen(cache_file.c_str(), "rb");
    string input;
    char buffer[1025];
    int count;
    while((count = gzread(zipfile, buffer, 1024)) > 0)
      input.append(buffer, count);
    xmlDocPtr doc = xmlReadDoc(reinterpret_cast<const xmlChar *>(input.c_str()), NULL, NULL, 0);
    xmlNodePtr root = xmlDocGetRootElement(doc);
    for (xmlNodePtr node = root->children; node; node = node->next) {
      if (xmlStrEqual(node->name, BAD_CAST "artist")) {
        artists.push_back(Artist::parse(node));
      }
    }
    xmlFreeDoc(doc);
  }
  
  for (vector<Artist>::const_iterator i = artists.begin(); i != artists.end(); ++i) {
    bool trigger = false;
    string nation = "Unknown";
    if (this->SelectArtist(i->Name())) {
      trigger = true;
    } else {
      // Is classified, get result
      if (*(this->m_artist_sel_timestamp) < time(NULL) - 14*24*60*60) 
        trigger = true;
      nation = this->m_artist_sel_nation;
    }
    this->SelectArtistCleanup();
    
    // If we decied that the result is outdated, sent a trigger to the DB
    if (trigger) {
      strcpy(this->m_trigger_chk_string, i->Name().c_str());
      *(this->m_trigger_chk_string_len) = i->Name().length();
      if (mysql_stmt_execute(this->m_trigger_chk_stmt))
        throw runtime_error((boost::format("mysql_stmt_execute() failed: %1%") 
          % mysql_stmt_error(this->m_trigger_chk_stmt)).str());
      // Check if already a trigger exists
      if (mysql_stmt_fetch(this->m_trigger_chk_stmt) == 0) {
        // there is already a trigger in the DB
        trigger = false;
      }
      if (mysql_stmt_free_result(this->m_trigger_chk_stmt)) 
        throw runtime_error((boost::format("mysql_stmt_free_result() failed: %1%") 
          % mysql_stmt_error(this->m_trigger_chk_stmt)).str());
      // If we still should trigger, so post the tigger
      if (trigger) {
        strcpy(this->m_trigger_ins_string, i->Name().c_str());
        *(this->m_trigger_ins_string_len) = i->Name().length();
        if (mysql_stmt_execute(this->m_trigger_ins_stmt))
          throw runtime_error((boost::format("mysql_stmt_execute() failed: %1%") 
            % mysql_stmt_error(this->m_trigger_ins_stmt)).str());
      }
    }
    valuableData.push_back(ArtistData(*i, nation));
  }
  
  // write to cache
  // Create a new XML buffer, to which the XML document will be written
  xmlBufferPtr buf = xmlBufferCreate();
  if (buf == NULL)
    throw runtime_error("Error creating the XML buffer");
  // Create a new XmlWriter for memory, with no compression.
  xmlTextWriterPtr writer = xmlNewTextWriterMemory(buf, 0);
  if (writer == NULL)
    throw runtime_error("Error creating the xml writer");
  // Start the document with the xml default for the version, encoding 
  // UTF-8 and the default for the standalone declaration.
  int rc = xmlTextWriterStartDocument(writer, NULL, "UTF-8", NULL);
  if (rc < 0)
    throw runtime_error("Error at xmlTextWriterStartDocument");
  rc = xmlTextWriterStartElement(writer, BAD_CAST "data");
  if (rc < 0) throw runtime_error("Error at xmlTextWriterStartElement");
 
  for (vector<ArtistData>::const_iterator i = valuableData.begin(); i != valuableData.end(); ++i) {
    i->writeXml(writer);
  }
  
  rc = xmlTextWriterEndDocument(writer);
  if (rc < 0) throw runtime_error("Error at xmlTextWriterEndDocument");
  xmlFreeTextWriter(writer);

  gzFile zipfile = gzopen(result_cache_file.c_str(), "wb9");
  gzputs(zipfile, (const char *)buf->content);
  gzclose(zipfile);
  
  return valuableData;
}

const char * LastFM::cache_dir = "/var/cache/lastfm-nationalities";

std::vector<ArtistData> Main::getData(std::string username)
{
  vector<ArtistData> artists;
  string username_encoded = base64_encode(reinterpret_cast<const unsigned char*>(username.c_str()), username.length());
  
  struct stat fileinfo;
  string cache_file = string(cache_dir) + "/result-" + username_encoded + ".xml.gz";
  int errcode = stat(cache_file.c_str(), &fileinfo);
  if (errno != ENOENT && errcode == -1)
    throw runtime_error("Something went wrong in the caching area.");
  // Cache result for 3.5 days
  if (errno == ENOENT || fileinfo.st_mtime + 3.5*24*60*60 < time(NULL)) {
    // result cache is outdated
    artists = this->renewResultCache(username, cache_file);
  } else {
    // Read from cache
    // Read from cache
    gzFile zipfile = gzopen(cache_file.c_str(), "rb");
    string input;
    char buffer[1025];
    int count;
    while((count = gzread(zipfile, buffer, 1024)) > 0)
      input.append(buffer, count);
    xmlDocPtr doc = xmlReadDoc(reinterpret_cast<const xmlChar *>(input.c_str()), NULL, NULL, 0);
    xmlNodePtr root = xmlDocGetRootElement(doc);
    for (xmlNodePtr node = root->children; node; node = node->next) {
      if (xmlStrEqual(node->name, BAD_CAST "a")) {
        artists.push_back(ArtistData::parse(node));
      }
    }
    xmlFreeDoc(doc);
  }
  return artists;
}
