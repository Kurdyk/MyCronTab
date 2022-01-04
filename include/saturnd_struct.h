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

void create_task_folder(TASK task);
void notify_timing(TASK task);
void create_task(TASK task, u_int64_t* taskid);
int timings_data_file();
void set_taskid(TASK* task, uint64_t* next_id);
char* my_cat(char* start, char* end);
void ensure_directory_exists(const char *path);
void check_exec_time();
void set_next_id(uint64_t* next_id);
void exec_task_from_id(uint64_t task_id);
int line_to_tokens(char *line, char **tokens);
void execute(char** argv, char* ret_file, char* out_file, char* err_file, int flags);


#endif //SY5_PROJET_2021_2022_SATURND_STRUCT_H
