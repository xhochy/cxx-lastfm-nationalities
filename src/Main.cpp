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

Main::Main()
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

std::vector<ArtistData> Main::getData(std::string username)
{
  vector<ArtistData> valuableData;
  string cache_dir = "/var/cache/lastfm-nationalities";
  string username_encoded = base64_encode(reinterpret_cast<const unsigned char*>(username.c_str()), username.length());
  
  // if not cached result do
    // Check for cached artists
    struct stat fileinfo;
    string cache_file = cache_dir + "/library-artists-" + username_encoded + ".xml.gz";
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
      strcpy(this->m_artist_sel_name, i->Name().c_str());
      *(this->m_artist_sel_name_len) = i->Name().length();
      if (mysql_stmt_execute(this->m_artist_sel_stmt))
        throw runtime_error((boost::format("mysql_stmt_execute() failed: %1%") 
          % mysql_stmt_error(this->m_artist_sel_stmt)).str());
      bool trigger = false;
      string nation = "Unknown";
      if (mysql_stmt_fetch(this->m_artist_sel_stmt)) {
        trigger = true;
      } else {
        // Is classified, get result
        if (*(this->m_artist_sel_timestamp) < time(NULL) - 14*24*60*60) trigger = true;
        nation = this->m_artist_sel_nation;
      }
      if (mysql_stmt_free_result(this->m_artist_sel_stmt)) 
        throw runtime_error((boost::format("mysql_stmt_free_result() failed: %1%") 
          % mysql_stmt_error(this->m_artist_sel_stmt)).str());
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
        }
      }
      valuableData.push_back(ArtistData(*i, nation));
    }
  // else if result cached
  // ... TODO
  //
  /********
                data = []
                  # Calculate the user's result not every time this code is run
                  data = Cache.file(File.join(@@cache_dir, 'user-result-' + username_encoded + '.yml'), 4*24*60*60) do
                    ...
                    ...
                  end
                  return data
  ********/
  return valuableData;
}
