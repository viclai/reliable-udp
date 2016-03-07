#include "noise.h"

#include <stdio.h> // fprintf

#include <cstdlib> // srand, rand
#include <ctime>   // time
#include <sstream>
#include <sys/time.h>
#include <time.h>
#include <string>

bool simulatePacketLossCorruption(double prob)
{
  int x, r;

  if (prob < 0 || prob > 100)
    return false;

  struct timeval tv;
  time_t long_time;
  struct tm *newtime;

  gettimeofday(&tv,0);
  time(&long_time);
  newtime = localtime(&long_time);
  std::ostringstream os;
  os << newtime->tm_hour << newtime->tm_min << newtime->tm_sec
     << tv.tv_usec;
  srand(std::stoul(os.str(), nullptr));
  //srand(time(NULL));

  x = (int)prob;
  r = rand() % 100;
  //fprintf(stdout, "* Prob = %d, Rand = %d\n", x, r);

  if (r <= x - 1)
    return true;
  else
    return false;
}
