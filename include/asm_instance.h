#ifndef ASM_INSTANCE_H
#define ASM_INSTANCE_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <string.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/stat.h>

#include "cJSON.h"

#define ASM_INST_OK    0
#define ASM_INST_FAIL -1

#define PAGE_SIZE 4096
#define ASM_WINDOW 4*PAGE_SIZE

#define ASM_INST_HEADER "VIMASM"

#define FILE_TYPE_C    0
#define FILE_TYPE_CPP  1
#define FILE_TYPE_RUST 2

typedef struct AsmInstance {
  char infile[PATH_MAX];          
  char *rebuild_command;
  cJSON *compile_node; 
  char  *asm_buffer; 
  unsigned long long time_changed; 
  unsigned long long asm_buflen;
  unsigned short ft;  
} AsmInstance; 


AsmInstance* AsmInstance_alloc(char *fname) __nonnull((1)); 
void         AsmInstance_free(AsmInstance*) __nonnull((1)); 

cJSON* AsmInstance_get_compile_node(AsmInstance *inst) __nonnull((1)); 
char*  AsmInstance_get_filename(AsmInstance *inst) __nonnull((1)); 
char*  AsmInstance_get_cmd(AsmInstance *inst) __nonnull((1)); 
char*  AsmInstance_get_asm(AsmInstance *inst) __nonnull((1)); 
const char*  AsmInstance_get_filetype(AsmInstance *inst) __nonnull((1)); 

int    AsmInstance_parse_command_C(AsmInstance*, cJSON*) __nonnull((1,2)); 
int    AsmInstance_parse_command_RUST(AsmInstance*, cJSON*) __nonnull((1,2)); 

int    AsmInstance_assembly_message(AsmInstance*, int) __nonnull((1)); 
int    AsmInstance_function_message(AsmInstance*, int) __nonnull((1));


#endif
