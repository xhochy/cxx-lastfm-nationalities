#include "ArtistData.h"
#include "unicode.h"
#include <boost/format.hpp>

using namespace Scrobbler;
using namespace std;

ArtistData::ArtistData(const Artist &artist, const string &nation) :
  m_playcount(artist.Playcount()), m_nation(nation), m_name(artist.Name()),
  m_url(artist.Url())
{
}

string ArtistData::toJSON() const
{
  return (boost::format("{\"name\":\"%1%\",\"playcount\":%2%,\"country\":\"%3%\","
    "\"url\":\"%4%\"}") % convert_UTF8_to_JSON(this->m_name.c_str()) 
    % this->m_playcount % this->m_nation % this->m_url).str();
}

std::string ArtistData::Nation() const
{
  return this->m_nation;
}

int ArtistData::Playcount() const
{
  return this->m_playcount;
}
