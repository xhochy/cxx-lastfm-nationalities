#ifndef __LASTFM_ARTISTDATA_H__
#define __LASTFM_ARTISTDATA_H__

#include <string>
#include <scrobbler/artist.h>

class ArtistData {
public:
  ArtistData(const Scrobbler::Artist &artist, const std::string &nation);
  void writeXml(xmlTextWriterPtr writer) const;
  std::string toJSON() const;
  std::string Nation() const;
  int Playcount() const;
  static ArtistData parse(xmlNodePtr node);
private:
  ArtistData();
  int m_playcount;
  std::string m_nation;
  std::string m_name;
  std::string m_url;
};

#endif // __LASTFM_ARTISTDATA_H__s
