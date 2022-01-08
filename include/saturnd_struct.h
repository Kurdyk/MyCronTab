#ifndef SY5_PROJET_2021_2022_SATURND_STRUCT_H
#define SY5_PROJET_2021_2022_SATURND_STRUCT_H

#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <wait.h>
#include <math.h>

#include "server-reply.h"
#include "helpers.h"
#include "timing.h"
#include "client-request.h"


#define MAX_TOKENS 256
#define FILE_NAME_LENGTH 24

/// Create_task

void create_task_folder(TASK task);
void notify_timing(TASK task);
void create_task(TASK task, u_int64_t* taskid);
void set_taskid(TASK* task, uint64_t* next_id);
void set_next_id(uint64_t* next_id, char* path);
char* my_cat(char* start, char* end);

/// Execution

void ensure_directory_exists(const char *path);
void check_exec_time();
void exec_task_from_id(uint64_t task_id);
int line_to_tokens(char *line, char **tokens);
void execute(char** argv, char* ret_file, char* out_file, char* err_file, int flags);

/// Remove_task

int remove_task(u_int64_t taskid);

/// Get last stdout and last stderr

char* last_exec_name(u_int64_t taskid, int is_delete);
void send_std(char* name, uint64_t taskid);
void send_string(STRING msg);

/// Get time and exit code

void send_time_and_exitcode(u_int64_t taskid);

#endif //SY5_PROJET_2021_2022_SATURND_STRUCT_H
