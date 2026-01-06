

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

#include "asm_instance.h"

#define PAGE_SIZE 4096
#define ASM_WINDOW 4*PAGE_SIZE

const char *infile; 
const char *function_label; 
cJSON *compile_commands_json; 

const char *compile_commands_fname = "compile_commands.json"; 
char project_dir[PATH_MAX] = {0}; // reuse for compile_commands

static bool search_file(const char *dirpath, const char *filename) 
{
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

  strcat(project_dir, "/"); 
  if (search_file(project_dir, compile_commands_fname)) {
    strcat(project_dir, compile_commands_fname); 
    return true; 
  }

  strcat(project_dir, "../"); 
  if (search_file(project_dir, compile_commands_fname)) {
    strcat(project_dir, compile_commands_fname); 
    return true; 
  }

  return false; 
}


static cJSON* parse_compile_commands() 
{
  int fd = open(project_dir, O_RDONLY);
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


static void display_usage()
{
  fprintf(stderr, "asm-filter <project dir> <file> <label>\n"); 
  exit(1); 
}


static void process_cml(int argc, char *argv[])
{
  infile = (const char*)NULL; 
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

      case 1: infile = ptr; break; 
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
  
  compile_commands_json = parse_compile_commands(); 
  if (!compile_commands_json) {
    fprintf(stderr, "Error: failed to parse compile_commands.json\n"); 
    return 1; 
  }

  AsmInstance *inst = AsmInstance_alloc(); 

  AsmInstance_set_filename(inst, infile); 
  
  if (AsmInstance_set_compile_node(inst, compile_commands_json) != ASM_INST_OK) {
    fprintf(stderr, "Error: file %s not found in parsed compile_commands.json\n", infile); 
    return 1;
  }

  /* from this filenode, we can extract that commands used to 
   * create the object file */  
  if (AsmInstance_create_rebuild_cmd(inst) != ASM_INST_OK) {
    fprintf(stderr, "Error: could not create rebuild cmd\n");
    return 1; 
  }

  /* that command gets parsed into our asm viewer */
  if (AsmInstance_compile_assembly(inst) != ASM_INST_OK)  {
    fprintf(stderr, "Error: failed to compile filtered assembly\n");
    return 1; 
  }

  if (AsmInstance_pipe_label(inst, function_label, stdout) != ASM_INST_OK) {
    fprintf(stderr, "Error: failed to stream label assembly\n");
    return 1; 
  }
      
  cJSON_free(compile_commands_json); 
  return 0; 
}

