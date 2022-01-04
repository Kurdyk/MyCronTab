#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <endian.h>
#include <dirent.h>
#include <math.h>


#include "../include/timing.h"
#include "../include/timing-text-io.h"
#include "../include/saturnd_struct.h"
#include "../include/helpers.h"
#include "../include/server-reply.h"
#include "../include/saturnd.h"


void create_task_folder(TASK task) {
    char* directory = "daemon_dir";
    char path[128];
    char* files[] = {"/args.txt", "/task_timing.txt", "/return_values", "/standard_out", "/error_out"};
    sprintf(path, "%s/%ld", directory, task.taskid);
    ensure_directory_exists(directory);
    ensure_directory_exists(path);
    for (int i = 0; i < 5; i++) {
        char* file_name = my_cat(path, files[i]);
        int fd;
        if (i < 2) {
            if ((fd = open(file_name, O_RDWR | O_TRUNC | O_CREAT, 0666)) == -1) {
                perror("open");
                exit(1);
            }
        } else {
            ensure_directory_exists(file_name);
        }
        char buf[TIMING_TEXT_MIN_BUFFERSIZE];

        switch(i) {
            case 0: //args*
                ;
                u_int32_t argc = task.commandline->argc;
                for (int i = 0; i < argc; i++) {
                    u_int32_t length = task.commandline->arguments[i]->length;
                    snprintf(buf, length + 1, "%s ", task.commandline->arguments[i]->content);
                    if (write(fd, buf, strlen(buf)) < 0) {
                        perror("write");
                        exit(1);
                    }
                }
                break;
            case 1: //timing
                timing_string_from_timing(buf, &(task.timing));
                if (write(fd, buf, strlen(buf)) < 0) {
                    perror("write");
                    exit(1);
                }
            default: //others
                break;
        }
        free(file_name);
    }
}

int timings_data_file() {
    char* directory = "daemon_dir";
    char path[64];
    ensure_directory_exists(directory);
    sprintf(path, "%s/timings.txt", directory);
    int fd = open(path, O_RDWR | O_APPEND | O_CREAT, 0666);
    if (fd < 0)
    {
        perror("open");
        exit(-1);
    }
    return fd;
}

void notify_timing(TASK task) {
    int fd = timings_data_file(); //open in O_RDWR
    char buf[TIMING_TEXT_MIN_BUFFERSIZE];
    sprintf(buf, "%ld ", task.taskid);
    if (write(fd, buf, strlen(buf)) < 0) {
        perror("write");
        exit(1);
    }
    timing_string_from_timing(buf, &(task.timing));
    if (write(fd, buf, strlen(buf)) < 0) {
        perror("write");
        exit(1);
    }
    if (write(fd, "\n", 1) < 0) {
        perror("write");
        exit(1);
    }
    close(fd);
}

void create_task(TASK task, u_int64_t* taskid) {
    set_taskid(&task, taskid);
    create_task_folder(task);
    notify_timing(task);
    uint16_t rep = htobe16(SERVER_REPLY_OK);
    uint64_t id = htobe64(task.taskid);
    int clyde = open_rep();
    write(clyde, &rep, sizeof(uint16_t));
    write(clyde, &id, sizeof(uint64_t));
    close(clyde);
}


void set_taskid(TASK* task, uint64_t* next_id) {
    task->taskid = *next_id;
    *next_id += 1;
}


int is_number(char* input) {
    for (int i = 0; i < strlen(input); i++) {
        if(!isdigit(*(input + i))) return 0;
    }
    return 1;
}

void set_next_id(uint64_t* next_id) {
    //On conserve comme ça les valeurs de retour en cas de volonté de consultation ultérieure.
    DIR *dirp = opendir("daemon_dir");
    struct dirent *entry;
    while ((entry = readdir(dirp))) {
        if(is_number(entry->d_name)) {
            uint64_t max = (atol(entry->d_name) < *next_id) ? *next_id : atol(entry->d_name) + 1;
            *next_id = max;
        }
    }
}


void ensure_directory_exists(const char *path){
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0777);
        chmod(path, 0777);
    }
}


char* my_cat(char* start, char* end) {
    char* res = malloc(sizeof(char) * (strlen(start) + strlen(end)) - 1);
    strcat(res, start);
    strcat(res, end);
    return res;
}

void check_exec_time() {
    size_t max_len = 1024;
    char* buf = malloc(sizeof(char) * 1024);
    FILE* file;
    file = fopen("daemon_dir/timings.txt", "r");
    ssize_t nread = 0;
    while ((nread = getline(&buf, &max_len, file)) != 0) {
        if (nread < 0) {
            perror("read");
            exit(1);
        }
        int i = 0;
        while (isspace(buf[i]) == 0) {
            i++;
        }
        char subtext_id[sizeof(char) * i];
        strncpy(subtext_id,&buf[0],i);
        uint64_t id = atol(subtext_id);
        //TODO : if the atcual timing is right do task
        exec_task_from_id(id);
    }
    free(buf);
}


void execute(char** argv, char* ret_file, char* out_file, char* err_file, int flags) {

    int status;
    int ret_fd = -1;
    int out_fd = -1;
    int err_fd = -1;

    if (flags == 0) {
        flags = O_TRUNC;
    }

    if (ret_file != NULL) {
        ret_fd = open(ret_file, flags | O_WRONLY | O_CREAT, 0666);
        if (ret_fd < 0) goto error;
    }

    if (out_file != NULL) {
        out_fd = open(out_file, flags | O_WRONLY | O_CREAT, 0666);
        if (out_fd < 0) goto error;
        if (dup2(out_fd, STDOUT_FILENO) == -1) goto error;
        close(out_fd);
    }

    if(err_file != NULL) {
        err_fd = open(err_file, flags | O_WRONLY | O_CREAT, 0666);
        if (err_fd < 0) goto error;
        if (dup2(err_fd, STDERR_FILENO) == -1) goto error;
        close(err_fd);
    }

    if (fork() == 0) {
        execvp(argv[0], argv);
        goto error;
    } else {
        /* the parent process calls wait() on the child */
        if (wait(&status) > 0) {
            if (WIFEXITED(status)) {
                //terminated in good condition
                int exit_status = WEXITSTATUS(status);
                char buf[256];
                sprintf(buf, "%d", exit_status);
                //TODO : trouver une meilleure alternative pour eviter log de 0.
                write(ret_fd, buf, (size_t) log10(exit_status + 1) + 1) ;
                close(ret_fd);
            } else {
                //child got a problem
                goto error;
            }
        } else {
            //wait failed
            goto error;
        }
    }

    error:
    perror("execute");
    if (ret_fd >= 0) close(ret_fd);
    if (out_fd >= 0) close(out_fd);
    if (err_fd >= 0) close(err_fd);

}

int line_to_tokens(char *line, char **tokens) {
    /*
     * Gotten from TP8.
     */
    int i = 0;
    memset(tokens, 0, MAX_TOKENS*sizeof(char*));
    while (line != NULL) {
        // découpage au niveau des espaces et de la fin de ligne
        tokens[i] = strsep(&line, " \t\n");
        // incrémentation de i seulement si le mot trouvé est non vide
        if (*tokens[i] != '\0') i++;
    }
    tokens[i] = NULL;

    return i;
}

void exec_task_from_id(uint64_t task_id) {
    char task_dir[1024];
    sprintf(task_dir, "daemon_dir/%ld/", task_id);

    char* date_buf = time_output_from_int64(time(NULL));
    strcat(date_buf, ".txt");

    char err_file[1024];
    strcat(strcpy(err_file, task_dir), "error_out/");
    strcat(err_file, date_buf);

    char ret_file[1024];
    strcat(strcpy(ret_file, task_dir), "return_values/");
    strcat(ret_file, date_buf);

    char out_file[1024];
    strcat(strcpy(out_file, task_dir), "standard_out/");
    strcat(out_file, date_buf);

    char cmdLine_file[1024];
    strcat(strcpy(cmdLine_file, task_dir), "args.txt");

    char cmdLine[1024];
    int fd_cmd = open(cmdLine_file, O_RDONLY);
    if (fd_cmd < 0) {
        perror("open");
        exit(1);
    }
    if (read(fd_cmd, cmdLine, 1024) < 0) {
        perror("read");
        exit(1);
    }

    char *argv[MAX_TOKENS];
    line_to_tokens(cmdLine, argv);

    close(fd_cmd);
    free(date_buf);
    execute(argv, ret_file, out_file, err_file, O_CREAT);

}