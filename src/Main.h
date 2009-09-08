#include "ArtistData.h"

#include <mysql/mysql.h>
#include <scrobbler/artist.h>
#include <string>
#include <vector>

namespace LastFM {

class Main {
public:
  Main();
  ~Main();
  std::vector<ArtistData> getData(std::string username);
  std::vector<Scrobbler::Artist> renewArtistsCache(std::string username, 
    std::string cache_file);
private:
  void InitMySQL();
  void CleanupMySQL();
  void LoadAPIKey();
  void InitArtistSelStmt();
  void InitTriggerChkStmt();
  void InitTriggerInsStmt();
  MYSQL_STMT * CreateStatement(const char * str);
  MYSQL m_mysql;
  // artist_sel_stmt
  MYSQL_STMT * m_artist_sel_stmt;
  char * m_artist_sel_name;
  unsigned long * m_artist_sel_name_len;
  char * m_artist_sel_nation;
  unsigned long * m_artist_sel_nation_len;
  my_bool * m_artist_sel_nation_is_null;
  my_bool * m_artist_sel_nation_error;
  int * m_artist_sel_timestamp;
  my_bool * m_artist_sel_timestamp_is_null;
  unsigned long * m_artist_sel_timestamp_len;
  my_bool * m_artist_sel_timestamp_error;
  // trigger_chk_stmt
  MYSQL_STMT * m_trigger_chk_stmt;
  char * m_trigger_chk_string;
  unsigned long * m_trigger_chk_string_len;
  int * m_trigger_chk_timestamp;
  my_bool * m_trigger_chk_timestamp_is_null;
  unsigned long * m_trigger_chk_timestamp_len;
  my_bool * m_trigger_chk_timestamp_error;
  // trigger_ins_stmt
  MYSQL_STMT * m_trigger_ins_stmt;
  char * m_trigger_ins_string;
  unsigned long * m_trigger_ins_string_len;
  
  std::string m_mysql_host;
  std::string m_mysql_user;
  std::string m_mysql_db;
  std::string m_mysql_pw;
};

} // LastFM
