

#include "asm_instance.h"

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


int AsmInstance_set_compile_node(AsmInstance *inst, cJSON *root) 
{
  /* 
   * compile_commands.json has a specific structure
   * this works for the minimal project 
   */
  char *filename = AsmInstance_get_filename(inst);

  if (*filename == '\0')
    return ASM_INST_FAIL; 

  for (cJSON *node = root->child; node; node = node->next) {
    cJSON *name_node = cJSON_GetObjectItemCaseSensitive(node, "file");
    char *str = cJSON_GetStringValue(name_node); 
    if (name_node && strcmp(str, filename) == 0) {
      inst->compile_node = node; 
      return ASM_INST_OK;
    }
  }

  return ASM_INST_FAIL; 
}


int AsmInstance_create_rebuild_cmd(AsmInstance *inst) 
{
  cJSON *compile_node = AsmInstance_get_compile_node(inst); 
  if (!compile_node)
    return ASM_INST_FAIL; 

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

  strcat(inst->rebuild_command, " -S -g1 -fno-inline -fcf-protection=none -fno-unwind-tables -fno-asynchronous-unwind-tables -masm=intel -o -"); 
  return ASM_INST_OK; 
}


int AsmInstance_compile_assembly(AsmInstance *inst) 
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

  if (!inst->asm_buffer) 
    inst->asm_buffer = (char*)malloc(ASM_WINDOW); 

  unsigned long long asm_len = 0; 
  char *asm_buffer = inst->asm_buffer; 

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
      if (asm_len + len > ASM_WINDOW) {
        fprintf(stderr, "Error: asm window buffer not large enough\n");
        return ASM_INST_FAIL; 
      }

      memcpy(asm_buffer+asm_len, line_buffer, len); 
      asm_len += len; 
    }
  }
  
  asm_buffer[asm_len] = '\0'; // safety for strchr
  pclose(p);
  
  inst->asm_buflen = asm_len; 
  inst->time_changed = sb.st_mtime; 
  return ASM_INST_OK; 
}


int AsmInstance_pipe_label(AsmInstance *inst, char *label, FILE *ofp) 
{
  char *asm_buffer = inst->asm_buffer; 
  if (!asm_buffer)
    return ASM_INST_FAIL; 

  char *prev = asm_buffer;  
  char *ptr = asm_buffer;  

  const size_t len = strlen(label); 

  while ((ptr = strchr(ptr, '\n'))) {
    // does the line start with <label>:
    if (memcmp(label, prev, len) == 0) {
      fwrite(prev, ptr-prev+1, 1, ofp);
      prev = ++ptr; 

      while ((ptr = strchr(ptr, '\n'))) {
        if (*prev != '\t')
          break; 

        fwrite(prev, ptr-prev+1, 1, ofp);
        prev = ++ptr; 
      }
      return ASM_INST_OK; 
    }

    prev = ++ptr; 
  }
  
  fprintf(stderr, "Error: no label named %s found in assembly output\n", label); 
  return ASM_INST_OK; 
}


int AsmInstance_write_label(AsmInstance *inst, char *label, int fd) 
{
  char *asm_buffer = inst->asm_buffer; 
  if (!asm_buffer)
    return ASM_INST_FAIL; 

  char *prev = asm_buffer;  
  char *ptr = asm_buffer;  

  const size_t len = strlen(label); 

  while ((ptr = strchr(ptr, '\n'))) {
    // does the line start with <label>:
    if (memcmp(label, prev, len) == 0) {
      write(fd, prev, ptr-prev+1);
      prev = ++ptr; 

      while ((ptr = strchr(ptr, '\n'))) {
        if (*prev != '\t')
          break; 
        
        write(fd, prev, ptr-prev+1);
        prev = ++ptr; 
      }
      return ASM_INST_OK; 
    }

    prev = ++ptr; 
  }
  
  dprintf(fd, "Error: no label named %s found in assembly output\n", label); 
  return ASM_INST_OK; 
}

