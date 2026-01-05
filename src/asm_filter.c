

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <string.h>

#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <libgen.h>
#include <dirent.h>

#include "cJSON.h"

#define PAGE_SIZE 4096
#define ASM_WINDOW 4*PAGE_SIZE

const char *project_dir; 
const char *filename; 
const char *function_label; 

unsigned int asm_len;
char asm_buffer[ASM_WINDOW]; 


static bool search_file(const char *dirpath, const char *filename) {
  DIR *dir = opendir(dirpath);
  if (!dir) {
    fprintf(stderr, "Error: [libc] opendir - %s\n", strerror(errno)); 
    return true; // or handle error
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    // Skip "." and ".."
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    if (strcmp(entry->d_name, filename) == 0) {
      closedir(dir);
      return true; 
    }
  }

  closedir(dir);
  return false; // Not found
}


static void filter_asm()
{
  static char command[1024]; 
  snprintf(command, sizeof(command), 
           "gcc -S -g1 -fno-inline -fcf-protection=none -fno-unwind-tables -fno-asynchronous-unwind-tables -masm=intel %s -o -", 
           project_dir); 

  FILE *p = popen(command,"r");
  if (!p) {
    fprintf(stderr, "Error: [libc] popen - %s\n", strerror(errno)); 
    return; 
  }

  asm_len = 0; 

  char line_buffer[4096];
  while (fgets(line_buffer, sizeof(line_buffer), p)) {

    unsigned int state = 0; 
    const size_t len = strlen(line_buffer);

    for (unsigned int i=0; i<len && !state; i++) {
      unsigned char ch = line_buffer[i]; 
      switch (ch) {
        case ' ':
        case '\t':
          break; 

        case '.':
          state = -1; 
          break; 

        default:
          state = 1; 
          break;
      }
    } 
    
    if (state == 1) {
      if (asm_len + len >= ASM_WINDOW) {
        fprintf(stderr, "Error: asm window buffer not large enough\n");
        return; 
      }

      memcpy(asm_buffer+asm_len, line_buffer, len); 
      asm_len += len; 
    }
  }
  
  asm_buffer[asm_len] = '\0'; // safety for strchr
  pclose(p);

  if (!asm_len)
    return; 

  char *prev = asm_buffer;  
  char *ptr = asm_buffer;  

  while ((ptr = strchr(ptr, '\n'))) {
    // does the line start with <label>:
    if (memcmp(function_label, prev, strlen(function_label)) == 0) {
      fwrite(prev, ptr-prev+1, 1, stdout);
      prev = ++ptr; 

      while ((ptr = strchr(ptr, '\n'))) {
        if (*prev != '\t')
          break; 

        fwrite(prev, ptr-prev+1, 1, stdout);
        prev = ++ptr; 
      }
      return; 
    }

    prev = ++ptr; 
  }
  fprintf(stderr, "Error: no label named %s found in assembly output\n", function_label); 
}


static bool find_compile_commands() {
  char path[PATH_MAX];
  strcpy(path, project_dir); 
  dirname(path); // changes the path data itself
  strcat(path, "/"); 

  printf("Checking path: %s\n", path);
  if (search_file(path, "compile_commands.json"))
    return true; 

  strcat(path, "../"); 

  printf("Checking path: %s\n", path);
  if (search_file(path, "compile_commands.json"))
    return true; 

  return false; 
}

static void display_usage()
{
  fprintf(stderr, "asm-filter <project dir> <file> <label>\n"); 
  exit(1); 
}


static void process_cml(int argc, char *argv[])
{
  project_dir    = (const char*)NULL; 
  function_label = (const char*)NULL; 
  filename       = (const char*)NULL; 

  int j=0;
  for (unsigned int i=1; i<argc; i++) {
    const char *ptr = argv[i]; 
    if (ptr[0] == '-' && ptr[1]) switch (ptr[1]) {
      default: display_usage();  
    }
    else switch (j++) {
      case 0: project_dir    = ptr; break; 
      case 1: filename       = ptr; break; 
      case 2: function_label = ptr; break; 
      default: display_usage(); 
    }
  }

  if (j < 3) 
    display_usage(); 
}


int main(int argc, char *argv[])
{
  process_cml(argc, argv); 
  
  if (!find_compile_commands()) {
    fprintf(stderr, "Error: could not find compile_commands.json\n"); 
    return 1;
  }

//  filter_asm(); 
  return 0; 
}

