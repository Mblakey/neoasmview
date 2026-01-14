

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <string.h>

#include <sys/wait.h>
#include <sys/types.h>

#include "cJSON.h"

static void display_usage() {
  fprintf(stderr, "usage:\n"
                  "  bear-cargo -- cargo build [flags]\n"
      ); 
  exit(1);
}


int main(int argc, char *argv[])
{
  unsigned int i;

  /* our options */
  for (i=1; i<argc; i++) {
    const char *ptr = argv[i]; 
    if (strcmp(ptr, "--") == 0) {
      break;
    }
  }
 
  if (++i >= argc || strcmp(argv[i],"cargo")!=0) {
    fprintf(stderr, "Error: format is bear-cargo -- cargo build [options]\n");
    display_usage();
    return 1; 
  }

  if (++i >= argc || strcmp(argv[i],"build")!=0) {
    fprintf(stderr, "Error: format is bear-cargo -- cargo build [options]\n");
    display_usage();
    return 1; 
  }
 
  char cargo_cmd[8192];
  cargo_cmd[0] = '\0';
  
  const char *cargo_invoke = "cargo build -v -v 2>&1";
  size_t len = strlen(cargo_invoke);
  
  memcpy(cargo_cmd, cargo_invoke, len); 
  cargo_cmd[len] = '\0';

  for (++i;i<argc; i++) {
    cargo_cmd[len++] = ' ';
    cargo_cmd[len] = '\0';
    strcat(cargo_cmd, argv[i]); 
  }

  FILE *p = popen(cargo_cmd, "r");
  if (!p) {
    fprintf(stderr, "Error: [libc] popen - %s\n", strerror(errno));
    return 1; 
  }

  cJSON *compile_commands = cJSON_CreateArray();
  
  unsigned int dir_len; 
  unsigned int build_len; 
  unsigned int src_len; 

  char directory[PATH_MAX]; 
  char src_file[PATH_MAX];
  char build_command[16384]; 

  const char *rustc_label = "rustc ";  // space avoids conflicts if rustc is in a path
  const char *manifest_dir_label = "CARGO_MANIFEST_DIR=";

  const size_t rustc_label_len    = 6; 
  const size_t manifest_label_len = 19; 

  char buffer[8192];
  while (fgets(buffer, sizeof(buffer), p)) {
    char *line;
    char *end = buffer + strlen(buffer);  
    for (line = buffer; line < end; line++) {
      if (*line != ' ')
        break;
    }
  
    const size_t len = strlen(line); 

    if (len > 8 && memcmp(line, "Running", 7) == 0) {
      dir_len   = 0; 
      build_len = 0; 
      src_len   = 0; 
      char *manifest_point = memmem(line, len, 
                                    manifest_dir_label, 
                                    strlen(manifest_dir_label));  
      if (!manifest_point) {
        fprintf(stderr, "Warning: cargo format command ignored\n"); 
        continue;
      }

      for (char *ptr = manifest_point + manifest_label_len; ptr < end; ptr++) {
        if (*ptr == ' ' || *ptr == '\t')
          break;
        else directory[dir_len++] = *ptr;
      }
      directory[dir_len] = '\0';

      char *command_ptr = memmem(line, len, 
                                 rustc_label, rustc_label_len); 
      if (!command_ptr) {
        fprintf(stderr, "Warning: cargo format command ignored\n"); 
        continue;
      }
     
      /* walk back till a space for a full command path */
      while (command_ptr > line) {
        if (*command_ptr == ' ' || *command_ptr == '\t') 
          break;
        else 
          command_ptr--; 
      }
      
      /* rust puts these weird blocks on the end of the commands */
      const unsigned char rust_term = '`';
      for (char *ptr = ++command_ptr; ptr < end; ptr++) {
        if (*ptr == rust_term || *ptr == '\n')
          break;
        build_command[build_len++] = *ptr;
      }
      build_command[build_len] = '\0';

      /* the src file is a combination of the directory, and the first rust file 
       * that is seen in the build command */
      
      char *src_ptr = memmem(build_command, build_len, 
                             ".rs ", 3); // space is important for conflicts
      if (!src_ptr) {
        fprintf(stderr, "Warning: cargo format command ignored\n"); 
        continue;
      }

      memcpy(src_file, directory, dir_len); 
      src_len = dir_len;
      src_file[src_len++] = '/'; 

      /* walk back till a space for a full command path */
      while (src_ptr > line) {
        if (*src_ptr == ' ' || *src_ptr == '\t') 
          break;
        else 
          src_ptr--; 
      }

      for (char *ptr = ++src_ptr; ptr < (build_command+build_len); ptr++) {
        if (*ptr == ' ' || *ptr == '\n')
          break;
        else 
          src_file[src_len++] = *ptr;
      }
      src_file[src_len++] = '\0';
      
      cJSON *node = cJSON_CreateObject(); 

      cJSON_AddStringToObject(node, "directory", directory); 
      cJSON_AddStringToObject(node, "file", src_file); 
      cJSON_AddStringToObject(node, "command", build_command); 
      cJSON_AddStringToObject(node, "output", ""); 
      cJSON_AddItemToArray(compile_commands, node); 
    }
  }
  
  char *json = cJSON_Print(compile_commands); 
  printf("%s\n", json); 

  cJSON_free(compile_commands); 
  fclose(p); 
  return 0; 
}


