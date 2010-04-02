#include "Main.h"
#include "base64.h"

#include <boost/format.hpp>
#include <libxml/parser.h>
#include <scrobbler/base.h>
#include <scrobbler/library.h>
#include <stdexcept>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

// Standard C/C++ includes
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>

using namespace LastFM;
using namespace Scrobbler;
using namespace std;

Main::Main() :
  m_artist_sel_stmt(NULL), m_trigger_chk_stmt(NULL), m_trigger_ins_stmt(NULL),
  m_mysql(NULL)
{
  LIBXML_TEST_VERSION;
  this->LoadAPIKey();
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
  rc = xmlTextWriterStartElement(writer, BAD_CAST("artists"));
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
  // Cache library for *30 days*
  if (errno == ENOENT || fileinfo.st_mtime + 30*24*60*60 < time(NULL)) {
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
      if (xmlStrEqual(node->name, BAD_CAST("artist"))) {
        Artist artist = Artist::parse(node);
        if (artist.Playcount() > 20) {
          artists.push_back(Artist::parse(node));
        }
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
      if (*(this->m_artist_sel_timestamp) < time(NULL) - 30*24*60*60)
        trigger = true;
      nation = this->m_artist_sel_nation;
    }
    this->SelectArtistCleanup();

    // If we decied that the result is outdated, sent a trigger to the DB
    if (trigger) {
      // Check if there is already a trigger for this artist in the DB
      if (this->TriggerCheck(i->Name()) == 0)
        trigger = false;
      this->TriggerCheckCleanup();
      // If we still should trigger, so post the tigger
      if (trigger) this->InsertTrigger(i->Name());
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
  rc = xmlTextWriterStartElement(writer, BAD_CAST("data"));
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
  if (errno == ENOENT || fileinfo.st_mtime + 2*24*60*60 < time(NULL)) {
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
      if (xmlStrEqual(node->name, BAD_CAST("a"))) {
        artists.push_back(ArtistData::parse(node));
      }
    }
    xmlFreeDoc(doc);
  }
  return artists;
}
