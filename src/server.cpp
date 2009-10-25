#include <hochheim/Server.h>
#include <hochheim/FullPathHandler.h>

using namespace Hochheim;

void jsonStats(const request&, Response&)
{

}

int main(int argc, char * argv[])
{
  Hochheim::Server<FullPathHandler> server("127.0.0.1", "8585", 20);
  server.requestHander().addHandler("/statistics.json", jsonStats);
  server.requestHander().addHandler("/user-info.json", jsonStats);
  server.requestHander().addHandler("/big-chart.png", jsonStats);
  server.requestHander().addHandler("/small-chart.png", jsonStats);
  server.requestHander().addHandler("/list-list.png", jsonStats);
  server.run();
}
