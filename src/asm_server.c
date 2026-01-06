
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
#include <signal.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "cJSON.h"

#include "asm_instance.h"

#define PAGE_SIZE 4096
#define ASM_WINDOW 4*PAGE_SIZE

const char *infile; 
const char *function_label; 
cJSON *compile_commands_json; 

const char *compile_commands_fname = "compile_commands.json"; 
char project_dir[PATH_MAX] = {0}; // reuse for compile_commands.json path
char socket_path[PATH_MAX] = {0}; 


static void handle_sigint(int signum)
{
  if (*socket_path)
    unlink(socket_path); 
  exit(1); 
}


static const char *get_tmp_dir(void) {
  const char *dir;
  dir = getenv("XDG_RUNTIME_DIR");
  if (dir && *dir) 
    return dir;

  dir = getenv("TMPDIR");
  if (dir && *dir) 
    return dir;

  struct stat st;
  if (stat("/tmp", &st) == 0 && S_ISDIR(st.st_mode))
      return "/tmp";
  if (stat("/var/tmp", &st) == 0 && S_ISDIR(st.st_mode))
      return "/var/tmp";
  return "./"; // last resort
}


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
  fprintf(stderr, "asm-server [project dir]\n"); 
  exit(1); 
}


static void process_cml(int argc, char *argv[])
{
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

      default: display_usage(); 
    }
  }

  if (!j && !realpath("./", project_dir)) {
    fprintf(stderr, "Error: [libc] realpath - %s\n", strerror(errno)); 
    display_usage(); 
  }
}


int main(int argc, char *argv[])
{
  process_cml(argc, argv); 

  signal(SIGINT, handle_sigint);
  
  if (!find_compile_commands()) {
    fprintf(stderr, "Error: could not find compile_commands.json\n"); 
    return 1;
  }
  
  compile_commands_json = parse_compile_commands(); 
  if (!compile_commands_json) {
    fprintf(stderr, "Error: failed to parse compile_commands.json\n"); 
    return 1; 
  }

  pid_t pid = getpid(); 
  const char *tmp_dir = get_tmp_dir(); 

  snprintf(socket_path, sizeof(socket_path), "%s/vimasm_%u.sock", tmp_dir, pid); 
  
  fprintf(stderr, "[asm viewer] compile_commands.json found\n"); 
  fprintf(stderr, "[asm viewer] tmp directory for socket %s\n", tmp_dir); 
  fprintf(stderr, "[asm viewer] creating socket vimasm_%u.sock\n", pid); 

  struct sockaddr_un addr; 
  memset(&addr, 0, sizeof(struct sockaddr_un)); 

  addr.sun_family = AF_UNIX; 
  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)); 

  int server_fd = socket(AF_UNIX, SOCK_STREAM, 0); 
  if (server_fd == -1) {
    fprintf(stderr, "Error: [libc] socket - %s\n", strerror(errno)); 
    return 1; 
  }
  
  if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
    fprintf(stderr, "Error: [libc] bind - %s\n", strerror(errno)); 
    return 1; 
  }

  if (chmod(socket_path, 0600) != 0) {
    fprintf(stderr, "Error: [libc] chmod - %s\n", strerror(errno)); 
    return 1; 
  }

  if (listen(server_fd, 1) != 0) {
    fprintf(stderr, "Error: [libc] listen - %s\n", strerror(errno)); 
    return 1; 
  }

  fprintf(stderr, "[asm viewer] listening...\n"); 
  
  int client_fd = accept(server_fd, NULL, NULL); 
  if (client_fd == -1) {
    fprintf(stderr, "Error: [libc] accept - %s\n", strerror(errno)); 
    return 1; 
  }
  
  fprintf(stderr, "[asm viewer] client connected\n"); 
  
  size_t bytes;
  char buffer[PATH_MAX]; 
  while ((bytes = read(client_fd, buffer, PATH_MAX)) > 0) {
    buffer[bytes] = '\0'; 
    char *newline = strchr(buffer, '\n'); 
    if (newline)
      *newline = '\0'; 
    
    /* the server expects 2 arguments per assembly output
     * seperated by a space and terminated by an optional newline */

    char *space = strchr(buffer, ' '); 
    if (!space) {
      fprintf(stderr, "[asm viewer] error - invalid request format\n"); 
      continue;
    }

    char *file_name = buffer; 
    char *label = space+1; 
    *space = '\0'; 

    AsmInstance *inst = AsmInstance_alloc(file_name); 

    if (!inst) {
      fprintf(stderr, "[asm viewer] error - failed to create asm instance\n");  
      continue;
    }

    if (AsmInstance_set_compile_node(inst, compile_commands_json) != ASM_INST_OK) {
      fprintf(stderr, "[asm viewer] error - file %s not found in parsed compile_commands.json\n", inst->infile); 
      goto clean_up_instance;
    }

    /* from this filenode, we can extract that commands used to 
     * create the object file */  
    if (AsmInstance_create_rebuild_cmd(inst) != ASM_INST_OK) {
      fprintf(stderr, "[asm viewer] error - could not create rebuild cmd\n");
      goto clean_up_instance;
    }

    /* that command gets parsed into our asm viewer */
    if (AsmInstance_compile_assembly(inst) != ASM_INST_OK)  {
      fprintf(stderr, "[asm viewer] error - failed to compile filtered assembly\n");
      goto clean_up_instance;
    }

    if (AsmInstance_write_label(inst, label, client_fd) != ASM_INST_OK) {
      fprintf(stderr, "[asm viewer] error - failed to stream label assembly\n");
      goto clean_up_instance;
    }
   
clean_up_instance:
    AsmInstance_free(inst); 
  }
 
  close(client_fd); 
  close(server_fd); 

  unlink(socket_path); 

#if 0
#endif
      
  cJSON_free(compile_commands_json); 
  return 0; 
}

