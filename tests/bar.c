
#include "foobar.h"

int mega_foobar(int x, int y, int z, char invert)
{
  if (!invert)
    return x + y + z; 
  else return ~(x+y+z); 
}
