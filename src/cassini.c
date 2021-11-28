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

int main(int argc, char *argv[])
{
  errno = 0;
  PIPES *pipes = NULL;

  int create_nb_args = 1;

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
      create_nb_args += 2;
      break;
    case 'H':
      hours_str = optarg;
      create_nb_args += 2;
      break;
    case 'd':
      daysofweek_str = optarg;
      create_nb_args += 2;
      break;
    case 'p':
      pipes_directory = strdup(optarg);
      create_nb_args += 2;
      if (pipes_directory == NULL)
        goto error;
      break;
    case 'l':
      //printf("On a l'option -l\n");
      operation = CLIENT_REQUEST_LIST_TASKS;
      break;
    case 'c':
      operation = CLIENT_REQUEST_CREATE_TASK;
      create_nb_args += 1;
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

  uint16_t converti = htobe16(operation);
  switch (operation)
  {
  case CLIENT_REQUEST_LIST_TASKS:
  {
    write(pipes->bonny, &converti, sizeof(operation));
    TASKS *tasks = get_list_answer(pipes);
    char *string_rep = malloc(sizeof(char) * 100);
    for (int i = 0; i < tasks->nbtasks; i++)
    {
      timing_string_from_timing(string_rep, &(tasks->tasks[i]->timing));
      printf("%ld: %s ", tasks->tasks[i]->taskid, string_rep);
      for (int j = 0; j < tasks->tasks[i]->commandline->argc; j++)
      {
        printf("%s ", tasks->tasks[i]->commandline->arguments[j]->content);
      }
      printf("\n");
    }
    break;
  }
  case CLIENT_REQUEST_CREATE_TASK:
  {
    /// ENVOI DES DONNEES A TRAVERS BONNY
    STRING *arguments = malloc(sizeof(STRING) * (argc - create_nb_args));
    for (int i = 0; i < (argc - create_nb_args); i++)
    {
      arguments[i].length = strlen(argv[create_nb_args + i]);
      arguments[i].content = argv[create_nb_args + i];
    }
    uint32_t nbargs = argc - create_nb_args;

    TIMING *timing = malloc(sizeof(TIMING));
    timing_from_strings(timing, minutes_str, hours_str, daysofweek_str);
    timing->minutes = htobe64(timing->minutes);
    timing->hours = htobe32(timing->hours);

    write(pipes->bonny, &(converti), sizeof(converti));
    write(pipes->bonny, (&timing->minutes), sizeof(timing->minutes));
    write(pipes->bonny, &(timing->hours), sizeof(timing->hours));
    write(pipes->bonny, &(timing->daysofweek), sizeof(timing->daysofweek));
    uint32_t nbargs_revesed = htobe32(nbargs);
    uint32_t strlen_revesed;
    write(pipes->bonny, &(nbargs_revesed), sizeof(nbargs_revesed));
    for (int i = 0; i < nbargs; i++)
    {
      strlen_revesed = htobe32(arguments[i].length);
      write(pipes->bonny, &strlen_revesed, sizeof(strlen_revesed));
      for (int j = 0; j < arguments[i].length; j++)
      {
        write(pipes->bonny, arguments[i].content + j, 1);
      }
    }

    /// RECEPTION DE LA REPONSE DANS CLYDE
    uint16_t reptype;
    uint64_t taskid;

    read(pipes->clyde, &reptype, sizeof(reptype));
    read(pipes->clyde, &taskid, sizeof(taskid));
    taskid = be64toh(taskid);
    printf("%ld\n", taskid);
    break;
  }

  case CLIENT_REQUEST_REMOVE_TASK:
  {
    write(pipes->bonny, &converti, sizeof(operation));
    uint64_t id = htobe64(taskid);
    write(pipes->bonny, &id, sizeof(uint64_t));

    uint16_t reptype;
    read(pipes->clyde, &reptype, sizeof(uint16_t));
    switch (be16toh(reptype)) {
      case SERVER_REPLY_OK: ;
        STRING *output = get_string(pipes);
        printf("%s\n", output->content);
        free(output);
        break;
      case SERVER_REPLY_ERROR: ;
        exit(1);
        break;
      default:
          printf("Unexpected answer\n");
          goto error;
    }
    break;

  }

  case CLIENT_REQUEST_GET_STDOUT: case CLIENT_REQUEST_GET_STDERR:
      write(pipes->bonny, &converti, sizeof(operation));
      uint64_t id = htobe64(taskid);
      write(pipes->bonny, &id, sizeof(uint64_t));
      uint16_t reptype;
      read(pipes->clyde, &reptype, sizeof(uint16_t));
          switch (be16toh(reptype)) {
              case SERVER_REPLY_OK: ;
                  STRING* output = get_string(pipes);
                  printf("%s", output->content);
                  free(output);
                  break;
              case SERVER_REPLY_ERROR: ;
                  exit(1);
                  break;
              default:
                  printf("Unexpected answer\n");
                  goto error;
          }
          break;

      case CLIENT_REQUEST_GET_TIMES_AND_EXITCODES:
          write(pipes->bonny, &converti, sizeof(operation));
          id = htobe64(taskid);
          write(pipes->bonny, &id, sizeof(uint64_t));
          read(pipes->clyde, &reptype, sizeof(uint16_t));
          switch (be16toh(reptype)) {
              case SERVER_REPLY_OK: ;

                  uint32_t n;
                  int64_t time;
                  uint16_t exitcode;

                  read(pipes->clyde, &n, sizeof(uint32_t));

                  for (int i = 0; i < be32toh(n); i++) {

                      read(pipes->clyde, &time, sizeof(int64_t));
                      char* aff = time_output_from_int64(be64toh(time));
                      read(pipes->clyde, &exitcode, sizeof(uint16_t));
                      printf("%s %d\n", aff, be16toh(exitcode));
                      free(aff);

                  }

                  break;
              case SERVER_REPLY_ERROR:
                  exit(1);
                  break;
              default:
                  printf("Unexpected answer");
                  goto error;
          }
              break;

  case CLIENT_REQUEST_TERMINATE:
  {
    write(pipes->bonny, &converti, sizeof(operation));
    uint16_t reptype;
    read(pipes->clyde, &reptype, sizeof(uint16_t));
    if (be16toh(reptype) == SERVER_REPLY_OK) {
      STRING *output = get_string(pipes);
      printf("%s\n", output->content);
      free(output);
    } else{
      goto error;
    }
  }

  }
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
