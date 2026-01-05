
#include <stdlib.h>
#include <stdio.h>

#include "foobar.h"

int foo(int x, int y) {
  return x*y; 
}

int bar(int x, int y, int z)
{
  return foo(x,y) + foo(y,z); 
}

int main(void) {
  int x = mega_foobar(2, 3, 4, 1);  
  return x; 
}
