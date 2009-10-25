#include <hochheim/Server.h>
#include <hochheim/FullPathHandler.h>

using namespace Hochheim;

void jsonStats(const request&, Response&)
{

}

int main(int argc, char * argv[])
{
  Hochheim::Server<FullPathHandler> server("127.0.0.1", "8585", 20);
  //server::requestHander().addHandler("/statistics.json", jsonStats);
  server.run();
}
