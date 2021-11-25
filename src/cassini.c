#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#if __APPLE__
#include "endianmac.h"
#else
#include <endian.h>
#endif

#include "cassini.h"
#include "timing.h"
#include "timing-text-io.h"
#include "helpers.h"

const char usage_info[] = "\
   usage: cassini [OPTIONS] -l -> list all tasks\n\
      or: cassini [OPTIONS]    -> same\n\
      or: cassini [OPTIONS] -q -> terminate the daemon\n\
      or: cassini [OPTIONS] -c [-m MINUTES] [-H HOURS] [-d DAYSOFWEEK] COMMAND_NAME [ARG_1] ... [ARG_N]\n\
          -> add a new task and print its TASKID\n\
             format & semantics of the \"timing\" fields defined here:\n\
             https://pubs.opengroup.org/onlinepubs/9699919799/utilities/crontab.html\n\
             default value for each field is \"*\"\n\
      or: cassini [OPTIONS] -r TASKID -> remove a task\n\
      or: cassini [OPTIONS] -x TASKID -> get info (time + exit code) on all the past runs of a task\n\
      or: cassini [OPTIONS] -o TASKID -> get the standard output of the last run of a task\n\
      or: cassini [OPTIONS] -e TASKID -> get the standard error\n\
      or: cassini -h -> display this message\n\
\n\
   options:\n\
     -p PIPES_DIR -> look for the pipes in PIPES_DIR (default: /tmp/<USERNAME>/saturnd/pipes)\n\
";

void process_task(uint16_t operation, PIPES *pipes)
{
  if (operation == CLIENT_REQUEST_LIST_TASKS)
  {
    uint16_t converti = htons(operation);
    write(pipes->bonny, &converti, sizeof(operation));
    TASKS *tasks = get_list_answer(pipes);
    char * string_rep = malloc(sizeof(char) * 100);
    for (int i = 0; i < tasks->nbtasks; i ++){
      timing_string_from_timing(string_rep, &(tasks->tasks[i]->timing));
      printf("%ld: %s ", tasks->tasks[i]->taskid, string_rep);
      for (int j = 0; j < tasks->tasks[i]->commandline->argc; j++){
        printf("%s ", tasks->tasks[i]->commandline->arguments[j]->content);
      }
      printf("\n");
    }
    printf("On fait planter le test\n");
  }
}

int main(int argc, char *argv[])
{
  errno = 0;
  PIPES *pipes = NULL;

  char *minutes_str = "*";
  char *hours_str = "*";
  char *daysofweek_str = "*";
  char *username = malloc(sizeof(char) * 50);
  getlogin_r(username, 50);
  char *pipes_directory = malloc(sizeof(char) * 100);
  sprintf(pipes_directory, "/tmp/%s/saturnd/pipes", username);

  uint16_t operation = CLIENT_REQUEST_LIST_TASKS;
  uint64_t taskid;

  int opt;
  char *strtoull_endp;
  while ((opt = getopt(argc, argv, "hlcqm:H:d:p:r:x:o:e:")) != -1)
  {
    switch (opt)
    {
    case 'm':
      minutes_str = optarg;
      break;
    case 'H':
      hours_str = optarg;
      break;
    case 'd':
      daysofweek_str = optarg;
      break;
    case 'p':
      pipes_directory = strdup(optarg);
      if (pipes_directory == NULL)
        goto error;
      break;
    case 'l':
      //printf("On a l'option -l\n");
      operation = CLIENT_REQUEST_LIST_TASKS;
      break;
    case 'c':
      operation = CLIENT_REQUEST_CREATE_TASK;
      break;
    case 'q':
      operation = CLIENT_REQUEST_TERMINATE;
      break;
    case 'r':
      operation = CLIENT_REQUEST_REMOVE_TASK;
      taskid = strtoull(optarg, &strtoull_endp, 10);
      if (strtoull_endp == optarg || strtoull_endp[0] != '\0')
        goto error;
      break;
    case 'x':
      operation = CLIENT_REQUEST_GET_TIMES_AND_EXITCODES;
      taskid = strtoull(optarg, &strtoull_endp, 10);
      if (strtoull_endp == optarg || strtoull_endp[0] != '\0')
        goto error;
      break;
    case 'o':
      operation = CLIENT_REQUEST_GET_STDOUT;
      taskid = strtoull(optarg, &strtoull_endp, 10);
      if (strtoull_endp == optarg || strtoull_endp[0] != '\0')
        goto error;
      break;
    case 'e':
      operation = CLIENT_REQUEST_GET_STDERR;
      taskid = strtoull(optarg, &strtoull_endp, 10);
      if (strtoull_endp == optarg || strtoull_endp[0] != '\0')
        goto error;
      break;
    case 'h':
      printf("%s", usage_info);
      return 0;
    case '?':
      fprintf(stderr, "%s", usage_info);
      goto error;
    }
  }

  // --------
  // | TODO |
  // --------

  pipes = init_pipes(pipes_directory);
  if (pipes == NULL)
  {
    goto error;
  }

  process_task(operation, pipes);
  return EXIT_SUCCESS;

error:
  if (errno != 0)
    perror("main");
  free(pipes_directory);
  if (pipes != NULL)
  {
    close(pipes->bonny);
    close(pipes->clyde);
  }
  free(pipes);
  pipes_directory = NULL;
  return EXIT_FAILURE;
}
