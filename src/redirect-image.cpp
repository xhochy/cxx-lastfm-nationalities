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
  vector<string> countries3;
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
    cout << max_nat << endl;
    if (countries2.size() < 10 && pc > 0.025) { // || list == 1/true
      playcounts.push_back(pc);
      if (max_nat == "United Kingdom") max_nat = "UK";
      if (max_nat == "United States") max_nat = "USA";
      countries2.push_back(max_nat);
      countries3.push_back((boost::format("%1% (%2$.0f %%)") % max_nat 
        % (pc * 100)).str());
    } else others += pc;
  }
  
  if (others > 0) {
    playcounts.push_back(others);
    countries2.push_back("Others");
    countries3.push_back((boost::format("Others (%1$.0f %%)") % (others * 100)).str());
  }
  string playcounts_str = "";
  for (vector<float>::const_iterator i = playcounts.begin(); i != playcounts.end(); ++i) {
    if (i != playcounts.begin()) playcounts_str += ",";
    playcounts_str += (boost::format("%1%") % *i).str();
  }
  string countries2_str = "";
  for (vector<string>::const_iterator i = countries2.begin(); i != countries2.end(); ++i) {
    if (i != countries2.begin()) countries2_str += "|";
    countries2_str += *i;
  }
  
  // use cURL to escape the strings
  CURL * curl_tmp = curl_easy_init();
  char * playcountsURL = curl_easy_escape(curl_tmp, playcounts_str.c_str(), 0);
  char * countries2URL = curl_easy_escape(curl_tmp, countries2_str.c_str(), 0); 

  cout << playcounts_str << "|" << playcountsURL << endl;
  string url = (boost::format("http://chart.apis.google.com/chart?cht=p&chf=bg,"
    "s,e3e3e300&chs=250x100&chd=t:%1%&chl=%2%") % playcountsURL % countries2URL)
    .str();
  cout << HTTPRedirectHeader(url);
  
  // Cleanup cURL
  curl_free(playcountsURL);
  curl_free(countries2URL);
  curl_easy_cleanup(curl_tmp);
}

