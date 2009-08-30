// STL
#include <cstring>
#include <exception>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <cerrno>
#include <stdexcept>

// Boost
#include <boost/format.hpp>

// POSIX/UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// Cgicc
#include <cgicc/Cgicc.h>
#include <cgicc/HTTPContentHeader.h>

// Scrobbler
#include <scrobbler/library.h>

// MySQL
#include <mysql/mysql.h>

// zlib
#include <zlib.h>

#include "ArtistData.h"
#include "base64.h"

using namespace cgicc;
using namespace std;
using namespace Scrobbler;

int main(int argc, char **argv)
{
  LIBXML_TEST_VERSION
  
  // Read api-key
  ifstream api_file;
  api_file.open("/etc/cxx-lastfm-nationalities/api-key.txt", ifstream::in);
  char * key = new char[256];
  api_file.getline(key, 256);
  scobbler_api_key = string(key);
  api_file.close();

  // Read mysql-key
  ifstream mysql_file;
  mysql_file.open("/etc/cxx-lastfm-nationalities/mysql-key.txt", ifstream::in);
  mysql_file.getline(key, 256);
  string mysql_host = key;
  mysql_file.getline(key, 256);
  string mysql_user = key;
  mysql_file.getline(key, 256);
  string mysql_db = key;
  mysql_file.getline(key, 256);
  string mysql_pw = key;
  delete key;
  mysql_file.close();

  // Connect to the mysql database
  MYSQL mysql;
  mysql_init(&mysql);
  mysql_options(&mysql, MYSQL_READ_DEFAULT_GROUP, "lastfm-nationalities");
  if (!mysql_real_connect(&mysql, mysql_host.c_str(), mysql_user.c_str(), 
    mysql_pw.c_str(), mysql_db.c_str(), 0, NULL, 0)) {
    throw runtime_error((boost::format("Failed to connect to database: Error: %s") 
      % mysql_error(&mysql)).str());
  }
  
  // Prepare the mysql stmts
  MYSQL_STMT * artist_sel_stmt = mysql_stmt_init(&mysql);
  if (artist_sel_stmt == NULL)
    throw runtime_error((boost::format("mysql_stmt_init() failed: %1%") 
      % mysql_stmt_error(artist_sel_stmt)).str());
  const char * artist_sel_stmt_str = "SELECT nation, UNIX_TIMESTAMP(updated_at) FROM lastfmnations_artists WHERE artist = ?";
  if (mysql_stmt_prepare(artist_sel_stmt, artist_sel_stmt_str, strlen(artist_sel_stmt_str)))
    throw runtime_error((boost::format("mysql_stmt_prepare() failed: %1%") 
      % mysql_stmt_error(artist_sel_stmt)).str());
  MYSQL_STMT * trigger_chk_stmt = mysql_stmt_init(&mysql);
  const char * trigger_chk_stmt_str = "SELECT * FROM lastfmnations_trigger WHERE string = ?";
  mysql_stmt_prepare(trigger_chk_stmt, trigger_chk_stmt_str, strlen(trigger_chk_stmt_str));
  MYSQL_STMT * trigger_ins_stmt = mysql_stmt_init(&mysql);
  const char * trigger_ins_stmt_str = "INSERT INTO lastfmnations_trigger (string) VALUES (?)";
  mysql_stmt_prepare(trigger_ins_stmt, trigger_ins_stmt_str, strlen(trigger_ins_stmt_str));
  
  // Start output
  Cgicc cgi;
  std::string username = cgi["username"]->getValue();
  cout << HTTPContentHeader("text/x-json");

  // toolbox.rb begin
  string cache_dir = "/var/cache/lastfm-nationalities";
  string username_encoded = base64_encode(reinterpret_cast<const unsigned char*>(username.c_str()), username.length());
  vector<ArtistData> valuableData;
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
      MYSQL_RES * prepare_meta_result = mysql_stmt_result_metadata(artist_sel_stmt);
      if (!prepare_meta_result)
        throw runtime_error((boost::format("mysql_stmt_result_metadata() "
          "returned no meta information: %1%") % mysql_stmt_error(artist_sel_stmt)).str());
      MYSQL_BIND bind[1];
      memset(bind, 0, sizeof(bind));
      bind[0].buffer_type= MYSQL_TYPE_STRING;
      bind[0].buffer = strdup(i->Name().c_str());
      bind[0].buffer_length= i->Name().length();
      bind[0].is_null = 0;
      unsigned long * l = new unsigned long;
      *l = i->Name().length();
      bind[0].length = l;
      mysql_stmt_bind_param(artist_sel_stmt, bind);
      if (mysql_stmt_execute(artist_sel_stmt))
        throw runtime_error((boost::format("mysql_stmt_execute() failed: %1%") 
          % mysql_stmt_error(artist_sel_stmt)).str());
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
      if (mysql_stmt_bind_result(artist_sel_stmt, bind2))
        throw runtime_error((boost::format("mysql_stmt_bind_param() failed: %1%")
          % mysql_stmt_error(artist_sel_stmt)).str());
      if (mysql_stmt_store_result(artist_sel_stmt))
        throw runtime_error((boost::format("mysql_stmt_store_result() failed: %1%")
          % mysql_stmt_error(artist_sel_stmt)).str());
      bool trigger = false;
      string nation = "Unknown";
      if (mysql_stmt_fetch(artist_sel_stmt)) {
        trigger = true;
        throw runtime_error((boost::format("mysql_stmt_fetch() failed: %1%")
          % mysql_stmt_error(artist_sel_stmt)).str());
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
  
  cout << "[";
  for (vector<ArtistData>::const_iterator i = valuableData.begin(); i != valuableData.end(); i++) {
    if (i != valuableData.begin()) cout << ",";
    cout << i->toJSON();
  }
  cout << "]" << endl;

  // Cleanup MySQL
  mysql_stmt_close(artist_sel_stmt);
  mysql_stmt_close(trigger_chk_stmt);
  mysql_stmt_close(trigger_ins_stmt);
  mysql_close(&mysql);
  
  // Cleanup LibXML
  xmlCleanupParser();
}

