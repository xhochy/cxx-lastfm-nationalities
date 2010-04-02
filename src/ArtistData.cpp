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

const char * getNodeContent(xmlNodePtr node)
{
  if (node->children == NULL)
    return "";
  if (node->children->content == NULL)
    return "";
  return reinterpret_cast<const char *>(node->children->content);
}

/**
 * Write the ArtistData into as XML.
 *
 * Shortnamed tags are used to keep the data as small as possible.
 */
void ArtistData::writeXml(xmlTextWriterPtr writer) const
{
  // <a>
  if (xmlTextWriterStartElement(writer, BAD_CAST "a") < 0)
    throw runtime_error("Error at xmlTextWriterStartElement");

  // playcount as <p>..</p>
  if (xmlTextWriterWriteFormatElement(writer, BAD_CAST "p", "%d",
    this->m_playcount) < 0)
    throw runtime_error("Error at xmlTextWriterWriteFormatElement");

  // url as <u>..</u>
  if (xmlTextWriterWriteFormatElement(writer, BAD_CAST "u", "%s",
    this->m_url.c_str()) < 0)
    throw runtime_error("Error at xmlTextWriterWriteFormatElement");

  // name as <n>..</n>
  if (xmlTextWriterWriteFormatElement(writer, BAD_CAST "n", "%s",
    this->m_name.c_str()) < 0)
    throw runtime_error("Error at xmlTextWriterWriteFormatElement");

  // country as <c>..</c>
  if (xmlTextWriterWriteFormatElement(writer, BAD_CAST "c", "%s",
    this->m_nation.c_str()) < 0)
    throw runtime_error("Error at xmlTextWriterWriteFormatElement");

  // </a>
  if (xmlTextWriterEndElement(writer) < 0)
    throw runtime_error("Error at xmlTextWriterEndElement");
}

ArtistData ArtistData::parse(xmlNodePtr node)
{
  ArtistData result;
  for (xmlNodePtr cur_node = node->children; cur_node; cur_node = cur_node->next) {
    if (xmlStrEqual(cur_node->name, BAD_CAST "p"))
      result.m_playcount = atoi(getNodeContent(cur_node));
    else if (xmlStrEqual(cur_node->name, BAD_CAST "u"))
      result.m_url = getNodeContent(cur_node);
    else if (xmlStrEqual(cur_node->name, BAD_CAST "n"))
      result.m_name = getNodeContent(cur_node);
    else if (xmlStrEqual(cur_node->name, BAD_CAST "c"))
      result.m_nation = getNodeContent(cur_node);
  }
  return result;
}

ArtistData::ArtistData() {}

std::string ArtistData::Nation() const
{
  return this->m_nation;
}

int ArtistData::Playcount() const
{
  return this->m_playcount;
}
