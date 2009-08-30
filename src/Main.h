#include "ArtistData.h"

#include <mysql/mysql.h>
#include <string>
#include <vector>

namespace LastFM {

class Main {
public:
  Main();
  ~Main();
  std::vector<ArtistData> getData(std::string username);
private:
  void InitMySQL();
  void LoadAPIKey();
  MYSQL_STMT * CreateStatement(const char * str);
  MYSQL m_mysql;
  MYSQL_STMT * m_artist_sel_stmt;
  MYSQL_STMT * m_trigger_chk_stmt;
  MYSQL_STMT * m_trigger_ins_stmt;
  std::string m_mysql_host;
  std::string m_mysql_user;
  std::string m_mysql_db;
  std::string m_mysql_pw;
};

} // LastFM
