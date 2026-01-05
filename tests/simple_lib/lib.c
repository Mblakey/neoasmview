
#include "lib.h"

int foo(int x, int y, int z) {
  return x + y + z; 
}

int bar(int x, int y, int z) {
  return x - y - z; 
}

int foobar(int x, int y, int z) {
  return foo(x,y,z) - bar(x,y,z); 
}
