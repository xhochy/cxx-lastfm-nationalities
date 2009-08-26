// STL
#include <cstring>
#include <exception>
#include <iostream>
#include <fstream>

// Cgicc
#include <cgicc/Cgicc.h>
#include <cgicc/HTTPContentHeader.h>

// Scrobbler
#include <scrobbler/library.h>

// MySQL
#include <mysql/mysql.h>

using namespace cgicc;
using namespace std;
using namespace Scrobbler;

int main(int argc, char **argv)
{
  // Read api-key
  ifstream api_file;
  api_file.open("api-key.txt", ifstream::in);
  char * key = new char[256];
  api_file.getline(key, 256);
  scobbler_api_key = string(key);
  api_file.close();

  // Read mysql-key
  ifstream mysql_file;
  mysql_file.open("mysql-key.txt", ifstream::in);
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
    fprintf(stderr, "Failed to connect to database: Error: %s\n", 
      mysql_error(&mysql));
  }
  
  // Prepare the mysql stmts
  MYSQL_STMT * artist_sel_stmt = mysql_stmt_init(&mysql);
  const char * artist_sel_stmt_str = "SELECT nation, UNIX_TIMESTAMP(updated_at) FROM lastfmnations_artists WHERE artist = ?";
  mysql_stmt_prepare(artist_sel_stmt, artist_sel_stmt_str, strlen(artist_sel_stmt_str));
  MYSQL_STMT * trigger_chk_stmt = mysql_stmt_init(&mysql);
  const char * trigger_chk_stmt_str = "SELECT * FROM lastfmnations_trigger WHERE string = ?";
  mysql_stmt_prepare(trigger_chk_stmt, trigger_chk_stmt_str, strlen(trigger_chk_stmt_str));
  MYSQL_STMT * trigger_ins_stmt = mysql_stmt_init(&mysql);
  const char * trigger_ins_stmt_str = "INSERT INTO lastfmnations_trigger (string) VALUES (?)";
  mysql_stmt_prepare(trigger_ins_stmt, trigger_ins_stmt_str, strlen(trigger_ins_stmt_str));
  
  // Start output
  Cgicc cgi;
  std::string username = cgi("username");
  cout << HTTPContentHeader("text/x-json") << endl;

  // toolbox.rb begin
  // // TODO caching in gzipped XML
  std::vector<Artist> artists = Library(username).artists();
  cout << artists.size() << endl;
  for (vector<Artist>::const_iterator i = artists.begin(); i != artists.end(); ++i) {
  }
  /********
                data = []
                  username = Base64.encode64(user.name).gsub("\n",'').gsub('/','-')
                  # Calculate the user's result not every time this code is run
                  data = Cache.file(File.join(@@cache_dir, 'user-result-' + username + '.yml'), 4*24*60*60) do
                    artists = Cache.file(File.join([@@cache_dir, 'user-library-artists-' + username + '.yml']), 20*24*60*60) do
                      Scrobbler::Library.new(user.name).artists 
                    end
                    data1 = []
                    artists.each do |t| 
                      # Only check the artists nation classified by last.fm every week
                      @@artist_sel_stmt.execute(t.name)
                      data = @@artist_sel_stmt.fetch
                      if data.nil?
                        country = 'Unknown'
                        self.trigger(t.name)
                      else
                        country = data[0]
                        timestamp = data[1].to_i
                        # Update artist every 2 weeks = 14*24*60*50 seconds
                        if Time.now.to_i - 14*24*60*60 > timestamp
                          self.trigger(t.name)
                        end
                      end
                      data1 << { 
                        'playcount' => t.playcount, 
                        'name' => t.name, 
                        'country' => country,  
                        'url' => t.url
                      }
                    end
                    data1
                  end
                  return data
  ********/
  // toolbox.rb end
    /*
user = Scrobbler::User.new(cgi['username'])
data = LastFM::get_user_data(user)

# Return output
cgi.out('type' => 'text/x-json') { data.to_json }
*/

  // Cleanup MySQL
  mysql_stmt_close(artist_sel_stmt);
  mysql_stmt_close(trigger_chk_stmt);
  mysql_stmt_close(trigger_ins_stmt);
  mysql_close(&mysql);
}

