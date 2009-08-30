// STL
#include <cstring>
#include <exception>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <cerrno>
#include <stdexcept>

// Boost
#include <boost/format.hpp>

// POSIX/UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// Cgicc
#include <cgicc/Cgicc.h>
#include <cgicc/HTTPContentHeader.h>

// Scrobbler
#include <scrobbler/library.h>

// MySQL
#include <mysql/mysql.h>

// zlib
#include <zlib.h>

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

