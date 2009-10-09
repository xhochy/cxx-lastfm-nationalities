// STL
#include <iostream>
#include <fstream>

// Cgicc
#include <cgicc/Cgicc.h>
#include <cgicc/HTTPContentHeader.h>

// Scrobbler
#include <scrobbler/library.h>

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
  cout << HTTPContentHeader("text/x-json") << endl;
  
  vector<ArtistData> valuableData = LastFM::Main().getData(username);

  cout << "[";
  for (vector<ArtistData>::const_iterator i = valuableData.begin(); i != valuableData.end(); i++) {
    if (i != valuableData.begin()) cout << ",";
    cout << i->toJSON();
  }
  cout << "]" << endl;
}

