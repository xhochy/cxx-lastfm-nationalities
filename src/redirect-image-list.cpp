// STL
#include <cstring>
#include <iostream>
#include <cstdio>
#include <cerrno>
#include <stdexcept>
#include <map>

// Boost
#include <boost/format.hpp>

// Cgicc
#include <cgicc/Cgicc.h>
#include <cgicc/HTTPRedirectHeader.h>

// Scrobbler
#include <scrobbler/library.h>

// MySQL
#include <mysql/mysql.h>

// libcurl
#include <curl/curl.h>

#include "ArtistData.h"
#include "Main.h"

using namespace cgicc;
using namespace std;
using namespace Scrobbler;

int main(int argc, char **argv)
{
  // Start output
  Cgicc cgi;
  std::string username = cgi["username"]->getValue();
  
  vector<ArtistData> valuableData = LastFM::Main().getData(username);
  map<std::string, int> countries;
  int overall = 0;
  for (vector<ArtistData>::const_iterator i = valuableData.begin(); i != valuableData.end(); i++) {
    if (countries.count(i->Nation()))
      countries[i->Nation()] += i->Playcount();
    else 
      countries[i->Nation()] = i->Playcount();
    overall += i->Playcount();
  }
  vector<float> playcounts;
  vector<string> countries2;
  float others = 0;
  while (!countries.empty()) {
    map<std::string, int>::const_iterator i = countries.begin();
    string max_nat = i->first;
    int max = i->second;
    for (; i != countries.end(); ++i) {
      if (i->second > max) {
        max = i->second;
        max_nat = i->first;
      }
    }
    countries.erase(max_nat);
    float pc = (float)max / (float)overall;
    if (countries2.size() < 10) {
      playcounts.push_back(pc);
      if (max_nat == "United Kingdom") max_nat = "UK";
      if (max_nat == "United States") max_nat = "USA";
      countries2.push_back(max_nat);
    } else others += pc;
  }
  
  if (others > 0) {
    playcounts.push_back(others);
    countries2.push_back("Others");
  }
  string playcounts_str = "";
  for (vector<float>::const_iterator i = playcounts.begin(); i != playcounts.end(); ++i) {
    if (i != playcounts.begin()) playcounts_str += ",";
    playcounts_str += (boost::format("%1%") % *i).str();
  }
  string countries2_str = "";
  for (vector<string>::const_reverse_iterator i = countries2.rbegin(); i != countries2.rend(); ++i) {
    if (i != countries2.rbegin()) countries2_str += "|";
    countries2_str += *i;
  }
  
  // use cURL to escape the strings
  CURL * curl_tmp = curl_easy_init();
  char * playcountsURL = curl_easy_escape(curl_tmp, playcounts_str.c_str(), 0);
  char * countries2URL = curl_easy_escape(curl_tmp, countries2_str.c_str(), 0); 

  string url = (boost::format("http://chart.apis.google.com/chart?cht=bhs&chf=bg"
    ",s,e3e3e300&chtt=%1%'s|artists+by+nations&chs=300x%2%&chco=80C65A&chds=0,"
    "%3%&chd=t:%4%&chxt=y&chxl=0:|%5%") % username % (75+ countries2.size()*25)
    % playcounts[0] % playcountsURL % countries2URL).str();
  cout << HTTPRedirectHeader(url);
  
  // Cleanup cURL
  curl_free(playcountsURL);
  curl_free(countries2URL);
  curl_easy_cleanup(curl_tmp);
}

