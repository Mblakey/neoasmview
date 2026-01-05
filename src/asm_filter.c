

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <string.h>

#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <libgen.h>
#include <dirent.h>
#include <fcntl.h>

#include <sys/mman.h>
#include <sys/stat.h>

#include "cJSON.h"

#define PAGE_SIZE 4096
#define ASM_WINDOW 4*PAGE_SIZE

const char *function_label; 
const char *compile_commands_fname = "compile_commands.json"; 

char infile[PATH_MAX]      = {0}; 
char project_dir[PATH_MAX] = {0}; 
char compile_commands_path[PATH_MAX] = {0};
char rebuild_command[PATH_MAX] = {0}; 

unsigned int asm_len;
char asm_buffer[ASM_WINDOW]; 

cJSON *compile_commands_json; 


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


static bool find_compile_commands() {

  strcpy(compile_commands_path, project_dir); 
  strcat(compile_commands_path, "/"); 
  
  if (search_file(compile_commands_path, compile_commands_fname)) {
    strcat(compile_commands_path, compile_commands_fname); 
    return true; 
  }

  strcat(compile_commands_path, "../"); 
  if (search_file(compile_commands_path, compile_commands_fname)) {
    strcat(compile_commands_path, compile_commands_fname); 
    return true; 
  }

  return false; 
}


static cJSON* parse_compile_commands() 
{
  int fd = open(compile_commands_path, O_RDONLY);
  if (fd == -1) {
    fprintf(stderr, "Error: [libc] open - %s\n", strerror(errno)); 
    return NULL;
  }

  struct stat sb;
  if (fstat(fd, &sb) == -1) {
    fprintf(stderr, "Error: [libc] stat - %s\n", strerror(errno)); 
    close(fd);
    return NULL;
  }
  
  if ((sb.st_mode & S_IFMT) != S_IFREG) {
    fprintf(stderr, "Error: compile commands file path is not a regular file\n"); 
    return NULL; 
  }

  const size_t json_mmap_size = sb.st_size;
  if (json_mmap_size == 0) {
    fprintf(stderr, "Error: compile_commands.json is empty\n");
    close(fd);
    return NULL;
  }

  // Map the file into memory
  char *json_buffer = mmap(NULL, json_mmap_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (json_buffer == MAP_FAILED) {
    fprintf(stderr, "Error: [libc] mmap - %s\n", strerror(errno));
    close(fd);
    return NULL;  
  }

  /* parse the json using the C-JSON parser */
  cJSON *parsed_json = cJSON_Parse(json_buffer);

  if (parsed_json == NULL) {
    fprintf(stderr, "Error before: %s\n", cJSON_GetErrorPtr());
    return NULL; 
  }

  munmap(json_buffer, json_mmap_size); 
  close(fd); 
  return parsed_json; 
}


static cJSON* get_node_of_filename(const char *filename)
{
  /* 
   * compile_commands.json has a specific structure
   * this works for the minimal project 
   */
  cJSON *node = compile_commands_json->child; 
  for (node = compile_commands_json->child; node; node = node->next) {
    cJSON *name_node = cJSON_GetObjectItemCaseSensitive(node, "file");
    char *str = cJSON_GetStringValue(name_node); 
    if (name_node && strcmp(str, filename) == 0)
      return node; 
  }

  return NULL; 
}


static bool create_rebuild_command(cJSON *file_node)
{
  cJSON *command_node = cJSON_GetObjectItemCaseSensitive(file_node, "command");
  if (!command_node) { 
    fprintf(stderr, "Error: could not find command field in compile_commands.json\n"); 
    return false; 
  }
  
  /* 
   * parse keeping all the arguments apart from the -o path, 
   * which we turn into -
   */

  char *str = cJSON_GetStringValue(command_node); 
  const size_t len = strlen(str); 
  
  int j = 0; 
  for (unsigned int i=0; i<len; i++) {
    /* ignore the -o dash */
    if (i<len-3 && 
        str[i] == '-' && 
        str[i+1] == 'o' && 
        str[i+2] == ' ') 
    {
      i+=3; 
      for (; i<len; i++) {
        if (str[i] == ' ') {
          i++; 
          break; 
        }
      }

      if (i==len-1) {
        fprintf(stderr, "Error: command end seen before other flags\n");
        return false; 
      }
    }

    rebuild_command[j++] = str[i]; 
  }

  strcat(rebuild_command, " -S -g1 -fno-inline -fcf-protection=none -fno-unwind-tables -fno-asynchronous-unwind-tables -masm=intel -o -"); 
  return true; 
}


static void filter_asm()
{
  FILE *p = popen(rebuild_command,"r");
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


static void display_usage()
{
  fprintf(stderr, "asm-filter <project dir> <file> <label>\n"); 
  exit(1); 
}


static void process_cml(int argc, char *argv[])
{
  function_label = (const char*)NULL; 

  int j=0;
  for (unsigned int i=1; i<argc; i++) {
    const char *ptr = argv[i]; 
    if (ptr[0] == '-' && ptr[1]) switch (ptr[1]) {
      default: display_usage();  
    }
    else switch (j++) {
      case 0: 
          if (!realpath(ptr, project_dir)) {
            fprintf(stderr, "Error: [libc] realpath - %s\n", strerror(errno)); 
            display_usage(); 
          }
          break; 

      case 1: 
          if (!realpath(ptr, infile)) {
            fprintf(stderr, "Error: [libc] realpath - %s\n", strerror(errno)); 
            display_usage(); 
          }
          break; 

      case 2: 
          function_label = ptr; 
          break; 

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
  
  compile_commands_json = parse_compile_commands(); 
  if (!compile_commands_json) {
    fprintf(stderr, "Error: failed to parse compile_commands.json\n"); 
    return 1; 
  }
  
  cJSON *file_node = get_node_of_filename(infile);
  if (!file_node) {
    fprintf(stderr, "Error: file %s not found in parsed compile_commands.json\n", infile); 
    return 1; 
  }

  /* from this filenode, we can extract that commands used to 
   * create the object file */  
  create_rebuild_command(file_node); 

  /* that command gets parsed into our asm viewer */
  filter_asm();
      
  cJSON_free(compile_commands_json); 
  return 0; 
}

