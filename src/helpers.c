#define _GNU_SOURCE

#include <fcntl.h>
#include <time.h>
#if __APPLE__
#include "endianmac.h"
#else
#include <endian.h>
#endif

#include <time.h>
#include <string.h>


#include "helpers.h"
#include "server-reply.h"


/////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// Gestion des Pipes //////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

PIPES *init_pipes(char *pipes_directory)
{
  char *bonny_name = malloc(sizeof(char) * 110);
  char *clyde_name = malloc(sizeof(char) * 110);
  sprintf(bonny_name, "%s/saturnd-request-pipe", pipes_directory);
  sprintf(clyde_name, "%s/saturnd-reply-pipe", pipes_directory);
  int fdwrite = open(bonny_name, O_WRONLY);
  //int fdread = open(clyde_name, O_RDONLY);
  int fdread = 12;
  if (fdwrite == -1 || fdread == -1)
  {
    return NULL;
  }
  free(bonny_name);
  free(clyde_name);
  PIPES *pipes = malloc(sizeof(PIPES));
  pipes->bonny = fdwrite;
  pipes->clyde = fdread;
  return pipes;
}


void open_read(PIPES * pipes, char *pipes_directory){
  char *clyde_name = malloc(sizeof(char) * 110);
  sprintf(clyde_name, "%s/saturnd-reply-pipe", pipes_directory);
  int fdread = open(clyde_name, O_RDONLY);
  pipes->clyde = fdread;
  free(clyde_name);
}

////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// Gestion des contenus //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////



//////////////// Général ////////////////

STRING *get_string(PIPES *pipes)
{
  STRING *string = malloc(sizeof(STRING));
  read(pipes->clyde, &(string->length), sizeof(string->length));
  string->length = be32toh(string->length);
  string->content = malloc((string->length + 1) * sizeof(char));
  for (int i = 0; i < string->length; i++)
  {
    read(pipes->clyde, (string->content + i), 1);
  }
  *(string->content + string->length) = '\0';
  return string;
}


//////////////// Specifique option -l ////////////////

LIST_HEADERS *get_list_headers(PIPES *pipes)
{
  LIST_HEADERS *headers = malloc(sizeof(LIST_HEADERS));
  read(pipes->clyde, &(headers->reptype), 2);
  read(pipes->clyde, &(headers->nbtasks), 4);
  headers->reptype = be16toh(headers->reptype);
  headers->nbtasks = be32toh(headers->nbtasks);
  return headers;
}


COMMANDLINE *get_commandline(PIPES *pipes)
{
  COMMANDLINE *commandline = malloc(sizeof(COMMANDLINE));
  read(pipes->clyde, &(commandline->argc), sizeof(commandline->argc));
  commandline->argc = be32toh(commandline->argc);
  commandline->arguments = malloc(commandline->argc * sizeof(char *));
  for (int i = 0; i < commandline->argc; i++)
  {
    commandline->arguments[i] = get_string(pipes);
  }
  return commandline;
}



TASK *get_task(PIPES *pipes)
{
  TASK *task = malloc(sizeof(TASK));

  read(pipes->clyde, &(task->taskid), sizeof(task->taskid));
  task->taskid = be64toh(task->taskid);
  get_timing(pipes, &(task->timing));
  task->commandline = get_commandline(pipes);
  return task;
}



TASKS *get_list_answer(PIPES *pipes)
{
  LIST_HEADERS *headers = get_list_headers(pipes);
  TASKS *tasks = malloc(sizeof(TASKS));
  tasks->tasks = malloc(sizeof(TASKS) * headers->nbtasks);
  if (headers->reptype != SERVER_REPLY_OK)
  {
    exit;
  }

  for (int i = 0; i < headers->nbtasks; i++)
  {
    (tasks->tasks)[i] = get_task(pipes);
  }
  tasks->nbtasks = headers->nbtasks;
  free(headers);
  return tasks;
}



void get_timing(PIPES *pipes, TIMING *timing)
{
  read(pipes->clyde, &(timing->minutes), sizeof(timing->minutes));
  read(pipes->clyde, &(timing->hours), sizeof(timing->hours));
  read(pipes->clyde, &(timing->daysofweek), sizeof(timing->daysofweek));
  timing->minutes = be64toh(timing->minutes);
  timing->hours = be32toh(timing->hours);
}

//////////////   Pour l'option -x    ////////////

char* time_output_from_int64(int64_t sec) {
    char* buff = malloc(sizeof(char)*20);
    strftime(buff, 20, "%Y-%m-%d %H:%M:%S", localtime(&sec));
    return buff;
}

//////////////  Pour saturnd   ////////////////////

STRING* read_string(int request_pipe)
{
    STRING *string = malloc(sizeof(STRING));
    read(request_pipe, &(string->length), sizeof(string->length));
    string->length = be32toh(string->length);
    string->content = malloc((string->length + 1) * sizeof(char));
    for (int i = 0; i < string->length; i++)
    {
        read(request_pipe, (string->content + i), 1);
    }
    *(string->content + string->length) = '\0';
    return string;
}

void read_timing(int request_pipe, TIMING *timing)
{
    read(request_pipe, &(timing->minutes), sizeof(timing->minutes));
    read(request_pipe, &(timing->hours), sizeof(timing->hours));
    read(request_pipe, &(timing->daysofweek), sizeof(timing->daysofweek));
    timing->minutes = be64toh(timing->minutes);
    timing->hours = be32toh(timing->hours);
}


COMMANDLINE* read_commandline(int request_pipe)
{
    COMMANDLINE *commandline = malloc(sizeof(COMMANDLINE));
    read(request_pipe, &(commandline->argc), sizeof(commandline->argc));
    commandline->argc = be32toh(commandline->argc);
    commandline->arguments = malloc(commandline->argc * sizeof(char *));
    for (int i = 0; i < commandline->argc; i++)
    {
        commandline->arguments[i] = read_string(request_pipe);
    }
    return commandline;
}

TASK* read_task(int request_pipe) {
    TASK* res = malloc(sizeof(TASK));
    read_timing(request_pipe, &(res->timing));
    res->commandline = read_commandline(request_pipe);
    return res;
}



u_int64_t int64_output_from_timestamp(char* timestamp) {
    struct tm tm = {0};
    memset(&tm, 0, sizeof(struct tm));
    strptime(timestamp, "%Y-%m-%d %H:%M:%S", &tm);
    u_int64_t sec = mktime(&tm);
    return sec;
}