#include "noise.h"

#include <time.h>     // tm, localtime, time
#include <sys/time.h> // timeval, gettimeofday

#include <cstdlib>    // srand, rand
#include <sstream>    // ostringstream
#include <string>     // stoul

using namespace std;

bool simulatePacketLossCorruption(double prob)
{
  double x;
  int r;
  struct timeval tv;
  time_t long_time;
  struct tm *newtime;

  if (prob < 0 || prob > 1)
    return false;

  gettimeofday(&tv,0);
  time(&long_time);
  newtime = localtime(&long_time);
  ostringstream os;
  os << newtime->tm_hour << newtime->tm_min << newtime->tm_sec
     << tv.tv_usec;
  srand(stoul(os.str(), nullptr));

  x = prob * 100;
  r = rand() % 100;

  if (r <= x - 1)
    return true;
  else
    return false;
}
