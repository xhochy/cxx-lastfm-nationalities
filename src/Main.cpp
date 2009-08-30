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

void Main::InitMySQL() 
{
  // Read mysql-key
  ifstream mysql_file;
  mysql_file.open("/etc/cxx-lastfm-nationalities/mysql-key.txt", ifstream::in);
  char key[256];
  mysql_file.getline(key, 256);
  this->m_mysql_host = key;
  mysql_file.getline(key, 256);
  this->m_mysql_user = key;
  mysql_file.getline(key, 256);
  this->m_mysql_db = key;
  mysql_file.getline(key, 256);
  this->m_mysql_pw = key;
  mysql_file.close();
  
  // Connect to the mysql database
  mysql_init(&this->m_mysql);
  mysql_options(&this->m_mysql, MYSQL_READ_DEFAULT_GROUP, "lastfm-nationalities");
  if (!mysql_real_connect(&this->m_mysql, this->m_mysql_host.c_str(), 
    this->m_mysql_user.c_str(), this->m_mysql_pw.c_str(), this->m_mysql_db.c_str(), 
    0, NULL, 0)) {
    throw runtime_error((boost::format("Failed to connect to database: Error: %s") 
      % mysql_error(&this->m_mysql)).str());
  }
  
  // Prepare the mysql stmts
  this->m_artist_sel_stmt = this->CreateStatement("SELECT nation, "
    "UNIX_TIMESTAMP(updated_at) FROM lastfmnations_artists WHERE artist = ?");
  this->m_trigger_chk_stmt = this->CreateStatement("SELECT * FROM "
    "lastfmnations_trigger WHERE string = ?");
  this->m_trigger_ins_stmt = this->CreateStatement("INSERT INTO "
    "lastfmnations_trigger (string) VALUES (?)");
}

MYSQL_STMT * Main::CreateStatement(const char * str)
{
  MYSQL_STMT * stmt = mysql_stmt_init(&this->m_mysql);
  if (stmt == NULL)
    throw runtime_error((boost::format("mysql_stmt_init() failed: %1%") 
      % mysql_stmt_error(stmt)).str());
  if (mysql_stmt_prepare(stmt, str, strlen(str)))
    throw runtime_error((boost::format("mysql_stmt_prepare() failed: %1%") 
      % mysql_stmt_error(stmt)).str());
  return stmt;
}

Main::~Main()
{
  // Cleanup MySQL
  mysql_stmt_close(this->m_artist_sel_stmt);
  mysql_stmt_close(this->m_trigger_chk_stmt);
  mysql_stmt_close(this->m_trigger_ins_stmt);
  mysql_close(&this->m_mysql);

  // Cleanup LibXML
  xmlCleanupParser();
}

std::vector<ArtistData> Main::getData(std::string username)
{
  // toolbox.rb begin
  vector<ArtistData> valuableData;
  string cache_dir = "/var/cache/lastfm-nationalities";
  string username_encoded = base64_encode(reinterpret_cast<const unsigned char*>(username.c_str()), username.length());
  
  // if not cached result do
    // Check for cached artists
    struct stat fileinfo;
    string cache_file = cache_dir + "/library-artists-" + username_encoded + ".xml.gz";
    int errcode = stat(cache_file.c_str(), &fileinfo);
    if (errno != ENOENT && errcode == -1) {
      cerr << "Something went wrong in the caching area." << endl;
      exit(1);
    }
    std::vector<Artist> artists;
    if (errno == ENOENT || fileinfo.st_mtime + 20*24*60*60 < time(NULL)) {
      // Cache is outdated
      artists = Library(username).artists();

      // write to cache
      xmlChar *tmp;
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
      MYSQL_RES * prepare_meta_result = mysql_stmt_result_metadata(this->m_artist_sel_stmt);
      if (!prepare_meta_result)
        throw runtime_error((boost::format("mysql_stmt_result_metadata() "
          "returned no meta information: %1%") % mysql_stmt_error(this->m_artist_sel_stmt)).str());
      MYSQL_BIND bind[1];
      memset(bind, 0, sizeof(bind));
      bind[0].buffer_type= MYSQL_TYPE_STRING;
      bind[0].buffer = strdup(i->Name().c_str());
      bind[0].buffer_length= i->Name().length();
      bind[0].is_null = 0;
      unsigned long * l = new unsigned long;
      *l = i->Name().length();
      bind[0].length = l;
      mysql_stmt_bind_param(this->m_artist_sel_stmt, bind);
      if (mysql_stmt_execute(this->m_artist_sel_stmt))
        throw runtime_error((boost::format("mysql_stmt_execute() failed: %1%") 
          % mysql_stmt_error(this->m_artist_sel_stmt)).str());
      delete l;
      MYSQL_BIND bind2[2];
      my_bool is_null[2];
      my_bool error[2];
      unsigned long length[2];
      int int_data;
      char str_data[1024];
      memset(bind2, 0, sizeof(bind2));
      bind2[0].buffer_type= MYSQL_TYPE_STRING;
      bind2[0].buffer= (char *)str_data;
      bind2[0].buffer_length= 1024;
      bind2[0].is_null= &is_null[0];
      bind2[0].length= &length[0];
      bind2[0].error= &error[0];
      bind2[1].buffer_type= MYSQL_TYPE_LONG;
      bind2[1].buffer= (char *)&int_data;
      bind2[1].is_null= &is_null[1];
      bind2[1].length= &length[1];
      bind2[1].error= &error[1];
      if (mysql_stmt_bind_result(this->m_artist_sel_stmt, bind2))
        throw runtime_error((boost::format("mysql_stmt_bind_param() failed: %1%")
          % mysql_stmt_error(this->m_artist_sel_stmt)).str());
      if (mysql_stmt_store_result(this->m_artist_sel_stmt))
        throw runtime_error((boost::format("mysql_stmt_store_result() failed: %1%")
          % mysql_stmt_error(this->m_artist_sel_stmt)).str());
      bool trigger = false;
      string nation = "Unknown";
      if (mysql_stmt_fetch(this->m_artist_sel_stmt)) {
        trigger = true;
      } else {
        // Is classified, get result
        if (int_data < time(NULL) - 14*24*60*60) trigger = true;
        nation = str_data;
      }
      mysql_free_result(prepare_meta_result);
      if (trigger) {
        // TODO Sent trigger
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
  // toolbox.rb end
  return valuableData;
}
