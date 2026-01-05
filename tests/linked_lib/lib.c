
#include "lib.h"
#include "curl/curl.h"

int foo(int x, int y, int z) {
  return x + y + z; 
}

int bar(int x, int y, int z) {
  return x - y - z; 
}

int foobar(int x, int y, int z) {
  curl_global_init(CURL_GLOBAL_ALL);
  CURL *eh = curl_easy_init();
  curl_easy_cleanup(eh);
  curl_global_cleanup();
  return foo(x,y,z) - bar(x,y,z); 
}
