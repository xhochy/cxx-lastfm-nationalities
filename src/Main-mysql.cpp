#include "Main.h"

#include <boost/format.hpp>
#include <cstring>
#include <fstream>
#include <iostream>

using namespace LastFM;
using namespace Scrobbler;
using namespace std;

void Main::InitArtistSelStmt()
{
  this->m_artist_sel_stmt = this->CreateStatement("SELECT nation, "
    "UNIX_TIMESTAMP(updated_at) FROM lastfmnations_artists WHERE artist = ?");
  MYSQL_BIND bind[1];
  memset(bind, 0, sizeof(bind));
  bind[0].buffer_type = MYSQL_TYPE_STRING;
  this->m_artist_sel_name = new char[1024];
  bind[0].buffer = this->m_artist_sel_name;
  bind[0].buffer_length = 1024;
  bind[0].is_null = 0;
  this->m_artist_sel_name_len = new unsigned long;
  bind[0].length = this->m_artist_sel_name_len;
  mysql_stmt_bind_param(this->m_artist_sel_stmt, bind);
  MYSQL_BIND bind2[2];
  memset(bind2, 0, sizeof(bind2));
  this->m_artist_sel_nation = new char[1024];
  this->m_artist_sel_nation_len = new unsigned long;
  this->m_artist_sel_nation_is_null = new my_bool;
  this->m_artist_sel_nation_error = new my_bool;
  bind2[0].buffer_type = MYSQL_TYPE_STRING;
  bind2[0].buffer = this->m_artist_sel_nation;
  bind2[0].buffer_length = 1024;
  bind2[0].is_null = this->m_artist_sel_nation_is_null;
  bind2[0].length = this->m_artist_sel_nation_len;
  bind2[0].error = this->m_artist_sel_nation_error;
  this->m_artist_sel_timestamp = new int;
  this->m_artist_sel_timestamp_is_null = new my_bool;
  this->m_artist_sel_timestamp_len = new unsigned long;
  this->m_artist_sel_timestamp_error = new my_bool;
  bind2[1].buffer_type = MYSQL_TYPE_LONG;
  bind2[1].buffer = (char *)this->m_artist_sel_timestamp;
  bind2[1].is_null = this->m_artist_sel_timestamp_is_null;
  bind2[1].length = this->m_artist_sel_timestamp_len;
  bind2[1].error = this->m_artist_sel_timestamp_error;
  if (mysql_stmt_bind_result(this->m_artist_sel_stmt, bind2))
    throw runtime_error((boost::format("mysql_stmt_bind_param() failed: %1%")
      % mysql_stmt_error(this->m_artist_sel_stmt)).str());
}

void Main::InitTriggerChkStmt()
{
  this->m_trigger_chk_stmt = this->CreateStatement("SELECT UNIX_TIMESTAMP(timestamp)"
    " FROM lastfmnations_trigger WHERE string = ?");
  MYSQL_BIND bind[1];
  memset(bind, 0, sizeof(bind));
  bind[0].buffer_type = MYSQL_TYPE_STRING;
  this->m_trigger_chk_string = new char[1024];
  bind[0].buffer = this->m_trigger_chk_string;
  bind[0].buffer_length = 1024;
  bind[0].is_null = 0;
  this->m_trigger_chk_string_len = new unsigned long;
  bind[0].length = this->m_trigger_chk_string_len;
  mysql_stmt_bind_param(this->m_trigger_chk_stmt, bind);
  // result
  memset(bind, 0, sizeof(bind));
  this->m_trigger_chk_timestamp = new int;
  this->m_trigger_chk_timestamp_is_null = new my_bool;
  this->m_trigger_chk_timestamp_len = new unsigned long;
  this->m_trigger_chk_timestamp_error = new my_bool;
  bind[0].buffer_type = MYSQL_TYPE_LONG;
  bind[0].buffer = (char *)this->m_trigger_chk_timestamp;
  bind[0].is_null = this->m_trigger_chk_timestamp_is_null;
  bind[0].length = this->m_trigger_chk_timestamp_len;
  bind[0].error = this->m_trigger_chk_timestamp_error;
  if (mysql_stmt_bind_result(this->m_trigger_chk_stmt, bind))
    throw runtime_error((boost::format("mysql_stmt_bind_param() failed: %1%")
      % mysql_stmt_error(this->m_trigger_chk_stmt)).str());
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
  this->InitArtistSelStmt();
  this->InitTriggerChkStmt();
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

void Main::CleanupMySQL()
{
  delete this->m_artist_sel_name;
  delete this->m_artist_sel_name_len;
  delete this->m_artist_sel_nation;
  delete this->m_artist_sel_nation_len;
  delete this->m_artist_sel_nation_is_null;
  delete this->m_artist_sel_nation_error;
  delete this->m_artist_sel_timestamp;
  delete this->m_artist_sel_timestamp_is_null;
  delete this->m_artist_sel_timestamp_len;
  delete this->m_artist_sel_timestamp_error;
  mysql_stmt_close(this->m_artist_sel_stmt);
  delete this->m_trigger_chk_string;
  delete this->m_trigger_chk_string_len;
  delete this->m_trigger_chk_timestamp;
  delete this->m_trigger_chk_timestamp_is_null;
  unsigned long * m_trigger_chk_timestamp_len;
  my_bool * m_trigger_chk_timestamp_error;
  mysql_stmt_close(this->m_trigger_chk_stmt);
  mysql_stmt_close(this->m_trigger_ins_stmt);
  mysql_close(&this->m_mysql);
}
