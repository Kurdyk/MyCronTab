#ifndef HELPERS_H_
#define HELPERS_H_

#include <unistd.h>
#include "timing-text-io.h"




/////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// Gestion des Pipes //////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

typedef struct
{
  int bonny; // Pipe cassini -> starurnd
  int clyde; // Pipe saturnd -> cassini
} PIPES;

PIPES *init_pipes(char *pipes_directory);



////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// Gestion des contenus //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////


//////////////// Général ////////////////
typedef struct struct_string
{
  uint32_t length; // Length of string
  char *content;   // Content of string
} STRING;

STRING *get_string(PIPES *pipes);



//////////////// Specifique option -l ////////////////
typedef struct list_headers_stuct
{
  uint16_t reptype;
  uint32_t nbtasks; // Total 6 octets
} LIST_HEADERS;

LIST_HEADERS *get_list_headers(PIPES *pipes);



typedef struct struct_commandline
{
  uint32_t argc;
  STRING **arguments;
} COMMANDLINE;

COMMANDLINE *get_commandline(PIPES *pipes);



typedef struct task_struct
{
  uint64_t taskid;
  TIMING timing;
  COMMANDLINE *commandline;
} TASK;

TASK *get_task(PIPES *pipes);



typedef struct tasks_struct
{
  int nbtasks;
  TASK **tasks;
} TASKS;

TASKS *get_list_answer(PIPES *pipes);



void get_timing(PIPES *pipes, TIMING *timing);
#endif