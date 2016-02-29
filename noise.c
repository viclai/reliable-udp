#include "noise.h"

#include <cstdlib> // srand, rand
#include <ctime>   // time

bool simulatePacketLossCorruption(double prob)
{
  int x, r;

  if (prob < 0 || prob > 100)
    return false;
  srand(time(NULL));

  x = (int)prob;
  r = rand() % 100;

  if (r <= x - 1)
    return true;
  else
    return false;
}
