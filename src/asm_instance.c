

#include "asm_instance.h"


static bool check_tool(const char *cmd) {
  char *path = getenv("PATH");
  if (!path) 
    return false;

  char buf[PATH_MAX];
  char *paths = strdup(path);
  char *saveptr = NULL;
  
  char *p = strtok_r(paths, ":", &saveptr);
  for (; p; p = strtok_r(NULL, ":", &saveptr)) {
    snprintf(buf, sizeof(buf), "%s/%s", p, cmd);
    if (access(buf, X_OK) == 0) {
      free(paths);
      return true; 
    }
  }

  free(paths);
  return false;
}


AsmInstance* AsmInstance_alloc(char *fname) 
{
  AsmInstance *inst = (AsmInstance*)malloc(sizeof(AsmInstance)); 
  memset(inst, 0, sizeof(AsmInstance)); 
  if (!realpath(fname, inst->infile)) {
    free(inst); 
    return NULL; 
  }
  return inst; 
}


void AsmInstance_free(AsmInstance *inst) 
{
  if (inst->asm_buffer)
    free(inst->asm_buffer); 
  if (inst->rebuild_command)
    free(inst->rebuild_command); 
  free(inst); 
}


cJSON* AsmInstance_get_compile_node(AsmInstance *inst) 
{
  return inst->compile_node; 
}


char* AsmInstance_get_filename(AsmInstance *inst) 
{
  return inst->infile; 
}


char* AsmInstance_get_cmd(AsmInstance *inst) 
{
  return inst->rebuild_command; 
}


char* AsmInstance_get_asm(AsmInstance *inst) 
{
  char *asm_buffer = inst->asm_buffer; 
  if (!asm_buffer || !inst->asm_buflen)
    return NULL; 
  return asm_buffer; 
}


const char* AsmInstance_get_filetype(AsmInstance *inst) 
{
  switch (inst->ft) {
    case FILE_TYPE_C: return "C";
    case FILE_TYPE_CPP: return "CPP";
    case FILE_TYPE_RUST: return "RS";
  }
  return NULL; 
}


int AsmInstance_parse_command_C(AsmInstance *inst, cJSON *root) 
{
  /* 
   * compile_commands.json has a specific structure
   * this works for the minimal project 
   */
  char *filename = AsmInstance_get_filename(inst);

  if (*filename == '\0')
    return ASM_INST_FAIL; 
  
  cJSON *compile_node = NULL;
  for (cJSON *node = root->child; node; node = node->next) {
    cJSON *name_node = cJSON_GetObjectItemCaseSensitive(node, "file");
    char *str = cJSON_GetStringValue(name_node); 
    if (name_node && strcmp(str, filename) == 0) {
      compile_node = node; 
      break;
    }
  }
  
  if (!compile_node)
    return ASM_INST_FAIL; 

  inst->compile_node = compile_node;
  cJSON *command_node = cJSON_GetObjectItemCaseSensitive(compile_node, 
                                                         "command");
  if (!command_node) 
    return ASM_INST_FAIL; 
  
  /* 
   * parse keeping all the arguments apart from the -o path, 
   * which we turn into -
   */
  char *str = cJSON_GetStringValue(command_node); 
  if (!str)
    return ASM_INST_FAIL; 

  const size_t len = strlen(str); 
  inst->rebuild_command = (char*)malloc(PATH_MAX + len); 
  
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

      if (i==len-1) 
        return ASM_INST_FAIL; 
    }

    inst->rebuild_command[j++] = str[i]; 
  }
  inst->rebuild_command[j] = '\0'; 

  strcat(inst->rebuild_command, " -S -g1 -fno-inline -fcf-protection=none -fno-unwind-tables -fno-asynchronous-unwind-tables -masm=intel -o - 2> /dev/null"); 

  char *ext = strrchr(filename, '.'); 
  if (ext) {
    ext++; 
    /* demangler for C++ */
    if ((strcmp(ext, "cpp")==0 || strcmp(ext, "hpp")==0) &&
         check_tool("c++filt")) 
    {
      /* do we have c++filt avaliable */
      strcat(inst->rebuild_command, " | c++filt"); 
    }
  }

  return ASM_INST_OK; 
}


int AsmInstance_parse_command_RUST(AsmInstance *inst, cJSON *root)
{
  /* 
   * compile_commands.json has a specific structure that comes from 
   * my internal tooling bear_cargo
   * 
   */
  char *filename = AsmInstance_get_filename(inst);
  if (*filename == '\0')
    return ASM_INST_FAIL; 
  
  cJSON *compile_node = NULL;
  for (cJSON *node = root->child; node; node = node->next) {
    cJSON *name_node = cJSON_GetObjectItemCaseSensitive(node, "file");
    char *str = cJSON_GetStringValue(name_node); 
    if (name_node && strcmp(str, filename) == 0) {
      compile_node = node; 
      break;
    }
  }
  
  if (!compile_node)
    return ASM_INST_FAIL; 

  inst->compile_node = compile_node;
  cJSON *command_node = cJSON_GetObjectItemCaseSensitive(compile_node, 
                                                         "command");
  if (!command_node) 
    return ASM_INST_FAIL; 
  
  /* 
   * parse keeping all the arguments apart from the -o path, 
   * which we turn into -
   */
  char *str = cJSON_GetStringValue(command_node); 
  if (!str)
    return ASM_INST_FAIL; 

  const size_t len = strlen(str); 
  inst->rebuild_command = (char*)malloc(PATH_MAX + len); 

  unsigned int state = 0; 
  const char *emit_flag = "--emit="; 
  const size_t emit_len = 7; 
  
  int j = 0; 
  for (unsigned int i=0; i<len; i++) {
    unsigned char ch = str[i]; 
    if (emit_flag[state] == ch) 
      state++; 
    else 
      state = 0; 
    
    inst->rebuild_command[j++] = str[i]; 

    if (state == emit_len) {
      memcpy(inst->rebuild_command+j, "asm ", 4); 
      j += 4;

      /* skip the rest */
      while (i < len) {
        if (str[i] == ' ')
          break;
        else i++; 
      }
      state = 0; 
    }
  }
  inst->rebuild_command[j] = '\0'; 

  strcat(inst->rebuild_command, " -o - -C opt-level=3 2> /dev/null");  
  if (check_tool("rustfilt")) 
    strcat(inst->rebuild_command, " | rustfilt"); 

  fprintf(stderr, "%s\n", inst->rebuild_command); 
  return ASM_INST_OK; 
}

int AsmInstance_compile(AsmInstance *inst) 
{
  char *cmd = AsmInstance_get_cmd(inst); 
  if (!cmd)
    return ASM_INST_FAIL; 

  /* first check if there has been a modification */
  const char *file = AsmInstance_get_filename(inst); 
  if (*file == '\0')
    return ASM_INST_FAIL; 

  struct stat sb; 
  if (lstat(file, &sb) != 0) 
    return ASM_INST_FAIL;

  /* assembly will still be valid */
  if (sb.st_mtime == inst->time_changed) 
    return ASM_INST_OK;  

  FILE *p = popen(cmd,"r");
  if (!p) {
    fprintf(stderr, "Error: [libc] popen - %s\n", strerror(errno)); 
    return ASM_INST_FAIL; 
  }

  size_t buf_max = ASM_WINDOW;
  if (!inst->asm_buffer) 
    inst->asm_buffer = (char*)malloc(buf_max); 

  unsigned long long asm_len = 0; 
  char *asm_buffer = inst->asm_buffer; 

  char line_buffer[4096];
  while (fgets(line_buffer, sizeof(line_buffer), p)) {

    unsigned int state = 0; 
    const size_t len = strlen(line_buffer);
    
    /*
     * keep jmp labels and their assembly
     * e.g .L183: <asm> 
     */
    if (len > 2 && 
        line_buffer[0] == '.' && 
        line_buffer[1] == 'L') 
    {
      if (line_buffer[2] >= '0' &&
          line_buffer[2] <= '9')
      {
        asm_buffer[asm_len++] = '\n'; 
        state = 1; 
      }
    }
    
    unsigned int i; 
    for (i=0; i<len && !state; i++) {
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
      if (asm_len + len > buf_max) {
        buf_max *= 2;
        char *new_buffer = (char*)realloc(asm_buffer, buf_max); 
        if (!new_buffer) {
          fprintf(stderr, "Error: [libc] realloc\n");
          free(asm_buffer);
          return ASM_INST_FAIL;
        }
        asm_buffer = new_buffer; 
      }
      
      if (i==1) // label
        asm_buffer[asm_len++] = '\n'; 

      memcpy(asm_buffer+asm_len, line_buffer, len); 
      asm_len += len; 
    }
  }
  
  asm_buffer[asm_len] = '\0'; // safety for strchr and ptr return
  pclose(p);
  
  inst->asm_buflen   = asm_len; 
  inst->time_changed = sb.st_mtime; 
  inst->asm_buffer   = asm_buffer; 
  return ASM_INST_OK; 
}


int AsmInstance_function_message(AsmInstance *inst, int client_fd)
{
  size_t bytes; 
  char buffer[ASM_WINDOW]; 

  int i = 0; 
  int func_len = 0; 
  char function_name[8192]; 
    
  unsigned int state = 0; 
  const char *type = "\t.type";
  const char *label = "@function\n";

  const size_t type_chars = strlen(type);
  const size_t label_chars = strlen(label);

  size_t buf_len = 0; 
  size_t buf_max = 16384;
  char *msg_buffer = (char*)malloc(buf_max); 

  char *cmd = AsmInstance_get_cmd(inst); 
  if (!cmd)
    return ASM_INST_FAIL; 

  char *filename = AsmInstance_get_filename(inst); 
  if (!*filename) 
    return ASM_INST_FAIL; 

  FILE *fp = popen(cmd,"r");
  if (!fp) {
    fprintf(stderr, "Error: [libc] popen - %s\n", strerror(errno)); 
    return ASM_INST_FAIL; 
  }
  
  while((bytes = fread(buffer, 1, sizeof(buffer), fp))) {
    i = 0; 
jmp_type_state:
    for (; i<bytes; i++) {
      unsigned char ch = buffer[i];
      if (type[state] == ch)
        state++;
      else 
        state = 0; 

      if (state == type_chars) {
        func_len = 0; state = 0; i++; 
        goto jmp_read_state;
      }
    } 
  }
  goto jmp_write_message;

  while((bytes = fread(buffer, 1, sizeof(buffer), fp))) {
    i = 0; 
jmp_read_state:
    for (; i<bytes; i++) {
      unsigned char ch = buffer[i];
      if (ch == '@') {
        state = 0; 
        goto jmp_label_state; 
      }
      function_name[func_len++] = ch;
    }
  }
  goto jmp_write_message;

  while((bytes = fread(buffer, 1, sizeof(buffer), fp))) {
    i = 0; 
jmp_label_state:
    for (; i<bytes; i++) {
      unsigned char ch = buffer[i];
      if (label[state] == ch)
        state++;
      else {
        state = 0; 
        goto jmp_type_state;
      }

      if (state == label_chars) {
        for (; func_len >= 0; func_len--) {
          if (function_name[func_len] == ',')
            break;
        }

        function_name[func_len] = '\0';

        char *p = function_name; 
        while (p && (*p == ' ' || *p == '\t'))
          p++; 
        size_t flen = strlen(p);
          
        if (buf_len + flen+1 > buf_max) {
          buf_max *= 2; 
          char *new_buffer = realloc(msg_buffer, buf_max);
          if (!new_buffer) {
            fprintf(stderr, "Error: [libc] realloc\n");
            fclose(fp); 
            free(msg_buffer);
            return ASM_INST_FAIL;
          }
          msg_buffer = new_buffer; 
        }

        memcpy(msg_buffer+buf_len, p, flen);
        buf_len += flen; 
        msg_buffer[buf_len++] = '\n';
        state = 0;  
        goto jmp_type_state;
      }
    } 
  }


jmp_write_message:
  msg_buffer[buf_len] = '\0';
  if (!buf_len)
    return ASM_INST_FAIL; 

  const size_t brackets_cnt = 2;
  const size_t colon_cnt    = 2;
  const size_t comma_cnt    = 2;
  const size_t quotes_cnt   = 4;
  const size_t field_len    = 8 + 3; // filepath and asm 
  const size_t filename_len = strlen(filename); 

  const uint32_t msg_bytes = brackets_cnt + 
                             colon_cnt +
                             comma_cnt + 
                             quotes_cnt +
                             field_len +
                             filename_len +
                             buf_len;
  
  /* prefix the number of bytes for iterative decoding on the other side 
   * its a shame i cant let lua just look at this memory.. classic IPC */
  if (write(client_fd, &msg_bytes, sizeof(uint32_t)) == -1) {
    fprintf(stderr, "Error [libc] write - %s\n", strerror(errno));
    return ASM_INST_FAIL; 
  } 

  dprintf(client_fd,"{\"filepath\":\"%s\",\"asm\":\"%s\"}", filename, msg_buffer);   
  
  free(msg_buffer); 
  fclose(fp); 
  return ASM_INST_OK; 
}


int AsmInstance_assembly_message(AsmInstance *inst, int client_fd) 
{
  if (AsmInstance_compile(inst) != ASM_INST_OK)  {
    fprintf(stderr, "[asm viewer] error - failed to compile filtered assembly\n");
    return ASM_INST_FAIL; 
  }
 
  /* "small" json responce, vim internals make this an easy parse */
  char *assembly = AsmInstance_get_asm(inst); 
  char *filename = AsmInstance_get_filename(inst); 
  
  const size_t brackets_cnt = 2;
  const size_t colon_cnt    = 2;
  const size_t comma_cnt    = 2;
  const size_t quotes_cnt   = 4;
  const size_t field_len = 8 + 3; // filepath and asm 
  const size_t filename_len = strlen(filename); 

  const uint32_t msg_bytes = brackets_cnt + 
                             colon_cnt +
                             comma_cnt + 
                             quotes_cnt +
                             field_len +
                             filename_len +
                             inst->asm_buflen;
  
  /* prefix the number of bytes for iterative decoding on the other side 
   * its a shame i cant let lua just look at this memory.. classic IPC */
  if (write(client_fd, &msg_bytes, sizeof(uint32_t)) == -1) {
    fprintf(stderr, "Error [libc] write - %s\n", strerror(errno));
    return ASM_INST_FAIL; 
  }

  dprintf(client_fd,"{\"filepath\":\"%s\",\"asm\":\"%s\"}", filename, assembly);   
  return ASM_INST_OK; 
}


