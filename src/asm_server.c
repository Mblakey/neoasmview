
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

#include <string.h>

#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <libgen.h>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "cJSON.h"

#include "asm_instance.h"

#define HT_SIZE 512
#define PAGE_SIZE 4096
#define ASM_WINDOW 4*PAGE_SIZE

#define SERVER_TYPE_C    0
#define SERVER_TYPE_RUST 1

const char *cargo_toml_fname = "Cargo.toml"; 
const char *compile_commands_fname = "compile_commands.json"; 

char project_dir[PATH_MAX] = {0}; // reuse for compile_commands.json path
char socket_path[PATH_MAX] = {0}; 

cJSON *compile_commands_json; 

static volatile sig_atomic_t exit_flag = 0; 

static void exit_from_signal(int signum)
{
  exit_flag = 1; 
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


static bool find_cargo_toml() {

  strcat(project_dir, "/"); 
  if (search_file(project_dir, cargo_toml_fname)) {
    strcat(project_dir, cargo_toml_fname); 
    return true; 
  }

  strcat(project_dir, "../"); 
  if (search_file(project_dir, cargo_toml_fname)) {
    strcat(project_dir, cargo_toml_fname); 
    return true; 
  }

  return false; 
}


static cJSON* parse_project_commands() 
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
    fprintf(stderr, "Error: project file is not a regular file\n"); 
    return NULL; 
  }

  const size_t json_mmap_size = sb.st_size;
  if (json_mmap_size == 0) {
    fprintf(stderr, "Error: project file is empty\n");
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


struct hash_entry {
  AsmInstance *inst;
  struct hash_entry *next; 
} strhash_entry; 


struct hash_entry* hash_entry_alloc() 
{
  struct hash_entry *hte = (struct hash_entry*)malloc(sizeof(struct hash_entry)); 
  memset(hte, 0, sizeof(struct hash_entry)); 
  return hte; 
}


/*
 * this algorithm (k=33) was first reported by dan bernstein many years ago 
 * in comp.lang.c. another version of this algorithm (now favored by bernstein) 
 * uses XOR: hash(i) = hash(i - 1) * 33 ^ str[i]; 
 * the magic of number 33 (why it works better than many other constants, 
 * prime or not) has never been adequately explained. 
 */
static uint16_t string_hash(const char *str)  
{
  int c;
  unsigned long hash = 5381;
  c = *str++;
  while ((c = *str++)) 
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

  return hash % HT_SIZE;
}


static AsmInstance* get_asm_instance(struct hash_entry *hash_table[], 
                                     size_t ht_size, 
                                     char *key)
{
  char expand_key[PATH_MAX]; 
  if (!realpath(key, expand_key)) 
    return NULL; 

  uint16_t hash_idx = string_hash(key);    
  struct hash_entry *slot = hash_table[hash_idx]; 
  
  while (slot) {
    if (strcmp(expand_key, AsmInstance_get_filename(slot->inst)) == 0) 
      return slot->inst; 
    slot = slot->next;
  }

  AsmInstance *inst = AsmInstance_alloc(key); 

  if (!inst) {
    fprintf(stderr, "[asm viewer] error - failed to create asm instance\n");  
    return NULL; 
  }

  if (AsmInstance_set_compile_node_gcc(inst, compile_commands_json) != ASM_INST_OK) {
    fprintf(stderr, "[asm viewer] error - file %s not found in parsed compile_commands.json\n", inst->infile); 
    free(inst); 
    return NULL; 
  }

  /* from this filenode, we can extract that commands used to 
   * create the object file */  
  if (AsmInstance_command_C(inst) != ASM_INST_OK) {
    fprintf(stderr, "[asm viewer] error - could not create rebuild cmd\n");
    free(inst);
    return NULL; 
  }

  slot = hash_table[hash_idx]; 
  if (!slot)
    slot = hash_table[hash_idx] = hash_entry_alloc(); 
  else {
    while (slot->next) 
      slot = slot->next; 
    slot->next = hash_entry_alloc(); 
    slot = slot->next; 
  }

  slot->inst = inst; 
  return inst; 
}


/* for now not needed, as server quits on stop */
static void free_hash_table(struct hash_entry *hash_table[], size_t ht_size)
{
  for (unsigned int i = 0; i < HT_SIZE; i++) {
    struct hash_entry *slot = hash_table[i]; 
    struct hash_entry *prev; 
    while (slot) {
      prev = slot; 
      slot = slot->next; 
      free(prev); 
    }
  }
}


/* move to a nonblocking model using poll */
int process_client_requests(int client_fd) 
{
  struct hash_entry *hash_table[HT_SIZE] = {NULL}; 

  char buffer[PATH_MAX+1]; 
  size_t bytes = read(client_fd, buffer, PATH_MAX); 
  
  if (bytes == -1) {
    // no data ready, just return to poll loop
    if (errno == EAGAIN || errno == EWOULDBLOCK) 
      return ASM_INST_OK;
    else {
      fprintf(stderr, "Error: [libc] read - %s\n", strerror(errno)); 
      return ASM_INST_FAIL; 
    }
  }

  if (bytes == 0) {
    fprintf(stderr, "[asm viewer] client disconneted\n");  
    return ASM_INST_OK;
  }

  buffer[bytes] = '\0'; 
  char *newline = strchr(buffer, '\n'); 
  if (newline)
    *newline = '\0'; 
  
  /* the server expects 1-2 arguments per assembly output
   * seperated by a space and terminated by an optional newline */
  char *label; 
  char *file_name = buffer; 
  char *space = strchr(buffer, ' '); 
  if (space) {
    label = space+1; 
    *space = '\0'; 
  }

  AsmInstance *inst = get_asm_instance(hash_table, HT_SIZE, file_name);  

  if (!inst) {
    fprintf(stderr, "[asm viewer] error - failed to create asm instance\n");  
    return ASM_INST_FAIL; 
  }

  /* that command gets parsed into our asm viewer */
  if (AsmInstance_compile_C(inst) != ASM_INST_OK)  {
    fprintf(stderr, "[asm viewer] error - failed to compile filtered assembly\n");
    return ASM_INST_FAIL; 
  }
  
  AsmInstance_write_header(inst, client_fd); 

  if (space) {
    if (AsmInstance_write_label(inst, label, client_fd) != ASM_INST_OK) {
      fprintf(stderr, "[asm viewer] error - failed to stream label assembly\n");
      return ASM_INST_FAIL; 
    }
  }
  else {
    if (AsmInstance_write_all(inst, client_fd) != ASM_INST_OK) {
      fprintf(stderr, "[asm viewer] error - failed to stream assembly\n");
      return ASM_INST_FAIL; 
    }
  }

  return ASM_INST_OK; 
}


int main(int argc, char *argv[])
{
  setlinebuf(stdout);
  process_cml(argc, argv); 

  struct sigaction sa = {0};
  sa.sa_handler = exit_from_signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;  // important: no SA_RESTART
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  signal(SIGPIPE, SIG_IGN);

  char server_type; 
  
  if (find_compile_commands()) {
    server_type = SERVER_TYPE_C;  
    fprintf(stderr, "[asm viewer] compile_commands.json found\n"); 
  }
  else if (find_cargo_toml()) {
    server_type = SERVER_TYPE_RUST;  
    fprintf(stderr, "[asm viewer] Cargo.toml found\n"); 
  }
  else {
    fprintf(stderr, "Error: could not find project commands\n"); 
    return 1;
  }
  
  compile_commands_json = parse_project_commands(); 
  if (!compile_commands_json) {
    fprintf(stderr, "Error: failed to parse compile_commands.json\n"); 
    return 1; 
  }

  pid_t pid = getpid(); 
  const char *tmp_dir = get_tmp_dir(); 

  snprintf(socket_path, sizeof(socket_path), "%s/vimasm_%u.sock", tmp_dir, pid); 
  
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
  
  // might change this to a umask in future
  if (chmod(socket_path, 0600) != 0) {
    fprintf(stderr, "Error: [libc] chmod - %s\n", strerror(errno)); 
    unlink(socket_path); 
    return 1; 
  }

  if (listen(server_fd, 1) != 0) {
    fprintf(stderr, "Error: [libc] listen - %s\n", strerror(errno)); 
    unlink(socket_path); 
    return 1; 
  }
  
  printf("%s\n", socket_path); 

  fprintf(stderr, "[asm viewer] listening...\n"); 
  
  int client_fd = accept(server_fd, NULL, NULL); 
  if (client_fd == -1) {
    fprintf(stderr, "Error: [libc] accept - %s\n", strerror(errno)); 
    unlink(socket_path); 
    return 1; 
  }

  /* 
   * non blocking read calls, allow signal to pass through to 
   * kill process
   */
  int flags = fcntl(client_fd, F_GETFL, 0); 
  if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) != 0) {
    fprintf(stderr, "Error: [libc] fcntl - %s\n", strerror(errno)); 
    unlink(socket_path); 
  }

  fprintf(stderr, "[asm viewer] client connected\n"); 

  struct pollfd fds[1]; 
  fds[0].fd = client_fd; 
  fds[0].events = POLLIN; 
  
  fprintf(stderr, "[asm viewer] polling client...\n"); 

  while (!exit_flag) {
    int ret = poll(fds, 1, 500); 
    if (ret == -1) {
      fprintf(stderr, "Error: [libc] poll - %s\n", strerror(errno)); 
      break; 
    }

    if (ret == 0) continue;

    if (fds[0].revents & POLLIN) 
      process_client_requests(client_fd); 

    // Client closed connection or error
    if (fds[0].revents & (POLLHUP | POLLERR)) 
      break;
  }
  
  close(client_fd); 
  close(server_fd); 

  unlink(socket_path); 

  cJSON_free(compile_commands_json); 
  return 0; 
}

