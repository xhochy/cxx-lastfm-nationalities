#include <string>
#include <scrobbler/artist.h>

class ArtistData {
public:
  ArtistData(const Scrobbler::Artist &artist, const std::string &nation);
  std::string toJSON() const;
private:
  int m_playcount;
  std::string m_nation;
  std::string m_name;
  std::string m_url;
};
