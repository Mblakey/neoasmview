

#ifndef ASM_INSTANCE_H
#define ASM_INSTANCE_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <string.h>
#include <errno.h>
#include <limits.h>

#include "cJSON.h"

#define ASM_INST_OK    0
#define ASM_INST_FAIL -1

#define PAGE_SIZE 4096
#define ASM_WINDOW 4*PAGE_SIZE

typedef struct AsmInstance {
  char infile[PATH_MAX];          
  char rebuild_command[PATH_MAX]; 
   
  struct {

  } flags; 

  cJSON *compile_node; 
  char  *asm_buffer; 
  unsigned long long asm_buflen;
  unsigned long long path_hash; 
  
} AsmInstance; 

AsmInstance* AsmInstance_alloc(); 
void         AsmInstance_free(AsmInstance*) __nonnull((1)); 

cJSON* AsmInstance_get_compile_node(AsmInstance *inst) __nonnull((1)); 
char*  AsmInstance_get_filename(AsmInstance *inst) __nonnull((1)); 
char*  AsmInstance_get_cmd(AsmInstance *inst) __nonnull((1)); 

int    AsmInstance_set_filename(AsmInstance*, char*) __nonnull((1,2)); 
int    AsmInstance_set_compile_node(AsmInstance*, cJSON*) __nonnull((1,2)); 
int    AsmInstance_create_rebuild_cmd(AsmInstance*) __nonnull((1)); 

int    AsmInstance_compile_assembly(AsmInstance*) __nonnull((1));
int    AsmInstance_pipe_label(AsmInstance *inst, char *label, FILE *ofp) __nonnull((1,2,3)); 


#endif
