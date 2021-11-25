#include <fcntl.h>

#include "helpers.h"
#include "server-reply.h"


/////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// Gestion des Pipes //////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

PIPES *init_pipes(char *pipes_directory)
{
  char *bonny_name = malloc(sizeof(char) * 110);
  char *clyde_name = malloc(sizeof(char) * 110);
  sprintf(bonny_name, "%s/bonny.pipe", pipes_directory);
  sprintf(clyde_name, "%s/clyde.pipe", pipes_directory);
  int fdwrite = open(bonny_name, O_WRONLY);
  int fdread = open(clyde_name, O_RDONLY);
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


