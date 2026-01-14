

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

static void stream_function_names(FILE* fp)
{
  size_t bytes; 
  const size_t buflen = 128;
  char buffer[buflen]; 

  int func_len = 0; 
  char function_name[8192]; 
    
  unsigned int state = 0; 
  const char *type = ".type";
  const char *label = "function";

  int i = 0; 

  while((bytes = fread(buffer, 1, sizeof(buffer), fp))) {
    i = 0; 
jmp_type_state:
    for (; i<bytes; i++) {
      unsigned char ch = buffer[i];
      if (type[state] == ch)
        state++;
      else 
        state = 0; 

      if (state == 5) {
        func_len = 0;
        i++; 
        goto jmp_read_state;
      }
    } 
  }
  return; 

  while((bytes = fread(buffer, 1, sizeof(buffer), fp))) {
    i = 0; 
jmp_read_state:
    for (; i<bytes; i++) {
      unsigned char ch = buffer[i];
      if (ch == '@') {
        state = 0; 
        i++; 
        goto jmp_label_state; 
      }
      function_name[func_len++] = ch;
    }
  }
  return;


  while((bytes = fread(buffer, 1, sizeof(buffer), fp))) {
    i = 0; 
jmp_label_state:
    for (; i<bytes; i++) {
      unsigned char ch = buffer[i];
      if (label[state] == ch)
        state++;
      else {
        goto jmp_type_state;
        state = 0; 
      }

      if (state == 8) {
        for (; func_len >= 0; func_len--) {
          if (function_name[func_len] == ',')
            break;
        }

        function_name[func_len] = '\0';

        char *p = function_name; 
        while (p && (*p == ' ' || *p == '\t'))
          p++; 
        printf("%s\n", p); 
        goto jmp_type_state;
      }
    } 
  }
  return; 
}


int main(int argc, char *argv[])

{
  stream_function_names(stdin); 
  return 0; 
}
