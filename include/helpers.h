#ifndef HELPERS_H_
#define HELPERS_H_

#include <unistd.h>

#include "timing-text-io.h"
#include "saturnd.h"



/////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// Gestion des Pipes //////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

typedef struct
{
  int bonny; // Pipe cassini -> starurnd
  int clyde; // Pipe saturnd -> cassini
} PIPES;

PIPES *init_pipes(char *pipes_directory);

void open_read(PIPES * pipes, char *pipes_directory);



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

//////////////   Pour l'option -x    ////////////


char* time_output_from_int64(int64_t sec);


//////////////  Pour saturnd   ////////////////////

STRING* read_string(int request_pipe);
void read_timing(int request_pipe, TIMING *timing);
COMMANDLINE* read_commandline(int request_pipe);
TASK* read_task(int request_pipe);
u_int64_t int64_output_from_timestamp(char* timestamp);
void free_task(TASK* task);


#endif
