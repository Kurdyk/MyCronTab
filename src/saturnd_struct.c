#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
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


/// Create task


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
        free(file_name);
        char buf[TIMING_TEXT_MIN_BUFFERSIZE];

        switch(i) {
            case 0: //args*
                ;
                u_int32_t argc = task.commandline->argc;
                for (int i = 0; i < argc; i++) {
                    if (i < argc - 1) {
                        sprintf(buf, "%s ", task.commandline->arguments[i]->content);
                    } else {
                        sprintf(buf, "%s", task.commandline->arguments[i]->content);
                    }
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
    }
}


void notify_timing(TASK task) {
    int fd = open("daemon_dir/timings.txt", O_WRONLY | O_APPEND | O_CREAT, 0666);
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

void set_next_id(uint64_t* next_id, char* path) {
    DIR *dirp = opendir(path);
    struct dirent *entry;
    while ((entry = readdir(dirp))) {
        if(is_number(entry->d_name)) {
            uint64_t max = (atol(entry->d_name) < *next_id) ? *next_id : atol(entry->d_name) + 1;
            *next_id = max;
        }
    }
    closedir(dirp);
}


/// Structure helpers

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


/// Execution

void check_exec_time() {
    struct stat st;
    if (stat("daemon_dir/timings.txt", &st) < 0) {
        return;
    }
    size_t max_len = 1024;
    char* buf = malloc(sizeof(char) * 256);
    FILE* file;
    file = fopen("daemon_dir/timings.txt", "r");
    flock(fileno(file), LOCK_EX);
    ssize_t nread = 0;
    while ((nread = getline(&buf, &max_len, file)) > 0) {
        if (nread < 0) {
            perror("read");
            _exit(1);
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
    flock(fileno(file), LOCK_UN);
    fclose(file);
    free(buf);
}


void execute(char* argv[], char* ret_file, char* out_file, char* err_file) {


    struct stat st = {0};

    int status;
    int ret_fd = -1;
    int out_fd = -1;
    int err_fd = -1;



    if (ret_file != NULL) {
        ret_fd = open(ret_file, O_WRONLY | O_CREAT, 0666);
        if (ret_fd < 0) goto open_error;
    }


    if (out_file != NULL) {
        out_fd = open(out_file,  O_WRONLY | O_CREAT, 0666);
        if (out_fd < 0) goto open_error;
        if (dup2(out_fd, STDOUT_FILENO) == -1) goto dup_error;
    }
    close(out_fd);

    if(err_file != NULL) {
        err_fd = open(err_file,  O_WRONLY | O_CREAT, 0666);
        if (err_fd < 0) goto open_error;
        if (dup2(err_fd, STDERR_FILENO) == -1) goto dup_error;
    }

    close(err_fd);
    int r = fork();

    if (r == 0) {
        execvp(argv[0], argv);
        //execvp failed
        if (ret_fd >= 0) close(ret_fd);
        if (out_fd >= 0) close(out_fd);
        if (err_fd >= 0) close(err_fd);
        //raise(SIGKILL); // se suicide en cas d'echec... une belle métaphore de la vie
        return;
    } else {
        /* the parent process calls wait() on the child */
        if (waitpid(r, &status, 0) > 0) {
            if (WIFEXITED(status)) {
                //terminated in good condition
                int exit_status = WEXITSTATUS(status);
                char buf[256];
                sprintf(buf, "%d", exit_status);
                write(ret_fd, buf, (size_t) log10(abs((exit_status == 0)?1:exit_status)) + 1) ;
                close(ret_fd);
            } else {
                //child got a problem
                goto other_error;
            }
        } else {
            //wait failed
            goto other_error;
        }
    }

    close(ret_fd);
    return;

    other_error:
    perror("other error in execute");
    if (ret_fd >= 0) close(ret_fd);
    if (out_fd >= 0) close(out_fd);
    if (err_fd >= 0) close(err_fd);
    return;

    open_error:
    perror("open error in execute");
    if (ret_fd >= 0) close(ret_fd);
    if (out_fd >= 0) close(out_fd);
    if (err_fd >= 0) close(err_fd);
    return;

    dup_error:
    perror("dup error in execute");
    if (ret_fd >= 0) close(ret_fd);
    if (out_fd >= 0) close(out_fd);
    if (err_fd >= 0) close(err_fd);
    return;

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
    char task_dir[256];
    sprintf(task_dir, "daemon_dir/%ld/", task_id);

    char* date_buf = time_output_from_int64(time(NULL));
    date_buf = realloc(date_buf, sizeof(char) * FILE_NAME_LENGTH);
    strcat(date_buf, ".txt");

    char err_file[256];
    strcat(strcpy(err_file, task_dir), "error_out/");
    strcat(err_file, date_buf);

    char ret_file[256];
    strcat(strcpy(ret_file, task_dir), "return_values/");
    strcat(ret_file, date_buf);

    char out_file[256];
    strcat(strcpy(out_file, task_dir), "standard_out/");
    strcat(out_file, date_buf);

    char cmdLine_file[256];
    strcat(strcpy(cmdLine_file, task_dir), "args.txt");

    char* cmdLine = malloc(256);
    memset(cmdLine, 0, 256);
    int fd_cmd = open(cmdLine_file, O_RDONLY);
    if (fd_cmd < 0) {
        perror("open in exec_from_id");
        exit(1);
    }
    if (read(fd_cmd, cmdLine, 256) < 0) {
        perror("read in exec_from_id");
        exit(1);
    }

    char** argv = malloc(sizeof(char*) * MAX_TOKENS);
    line_to_tokens(cmdLine, argv);



    close(fd_cmd);
    free(date_buf);
    execute(argv, ret_file, out_file, err_file);

    free(cmdLine);
}

/// Remove task


int remove_task(u_int64_t taskid) {

    //On conserve comme ça les valeurs de retour en cas de volonté de consultation ultérieure.
    char path[64];
    sprintf(path, "daemon_dir/%ld", taskid);
    char new_path[64];
    sprintf(new_path, "daemon_dir/trash_bin/%ld", taskid);
    rename(path, new_path);

    size_t max_len = 1024;
    char* buf = malloc(sizeof(char) * 1024);
    FILE* file = fopen("daemon_dir/timings.txt", "r+");
    flock(fileno(file), LOCK_EX);
    FILE* new = fopen("daemon_dir/timings_buff.txt", "w");
    ssize_t nread = 0;
    int find = 0;
    while ((nread = getline(&buf, &max_len, file)) > 0) {
        int i = 0;
        while (isspace(buf[i]) == 0) {
            i++;
        }
        char subtext_id[sizeof(char) * i];
        strncpy(subtext_id,&buf[0],i);
        uint64_t id = atol(subtext_id);
        if (id == taskid) {
            find = 1;
        } else {
            fputs(buf, new);
        }
    }
    remove("daemon_dir/timings.txt");
    rename("daemon_dir/timings_buff.txt", "daemon_dir/timings.txt");
    chmod("daemon_dir/timings.txt", 0666);
    flock(fileno(file), LOCK_UN);
    fclose(file);
    fclose(new);
    free(buf);
    return find;

}



/// Get strout ou strerr



char* last_exec_name(u_int64_t taskid, int is_deleted) {

    char* prev = malloc(sizeof(char)*FILE_NAME_LENGTH);

    char task_dir[1024];
    if (!is_deleted) {
        sprintf(task_dir, "daemon_dir/%ld/standard_out", taskid);
    } else {
        sprintf(task_dir, "daemon_dir/trash_bin/%ld/standard_out", taskid);
    }

    DIR *dir = opendir(task_dir);
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..")  != 0 && strcmp(prev, entry->d_name) < 0) {
            strcpy(prev, entry->d_name);
        }
    }
    return prev;
}


void send_string(STRING msg) {
    int clyde = open_rep();
    uint32_t size = htobe32(msg.length);
    if (write(clyde, &size, sizeof(uint32_t)) < 0) goto error;
    if (write(clyde, msg.content, msg.length) < 0) goto error;
    close(clyde);

    return;

    error:
    perror("write in send_string");
    close(clyde);
    exit(1);
}


 void send_std(char* name, uint64_t taskid) {
     struct stat st;

     int clyde;
     uint16_t rep;
     uint16_t error_type;
     int is_deleted = 0;

     char file_path[256];

     sprintf(file_path, "daemon_dir/%ld", taskid);
     if (stat(file_path, &st) < 0) {
         /* Task may have been deleted */
         sprintf(file_path, "daemon_dir/trash_bin/%ld", taskid);
         if (stat(file_path, &st) < 0) {
             /* Task not found */
             clyde = open_rep();
             rep = htobe16(SERVER_REPLY_ERROR);
             if (write(clyde, &rep, sizeof(rep)) < 0) goto write_error;
             error_type = htobe16(SERVER_REPLY_ERROR_NOT_FOUND);
             if (write(clyde, &error_type, sizeof(error_type)) < 0) goto write_error;
             close(clyde);
             return;
         } else {
             is_deleted = 1;
         }
     }


    char* last_exec = last_exec_name(taskid, is_deleted);
    if (strlen(last_exec) == 0) {
        /* Task never run */
        clyde = open_rep();
        rep = htobe16(SERVER_REPLY_ERROR);
        if (write(clyde, &rep, sizeof(rep)) < 0) goto write_error;
        error_type = htobe16(SERVER_REPLY_ERROR_NEVER_RUN);
        if (write(clyde, &error_type, sizeof(error_type)) < 0) goto write_error;
        close(clyde);
        return;
    }

    if (!is_deleted) {
        sprintf(file_path, "daemon_dir/%ld/%s/%s", taskid, name, last_exec);
    } else {
        sprintf(file_path, "daemon_dir/trash_bin/%ld/%s/%s", taskid, name, last_exec);
    }
    free(last_exec);

    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        perror("open in send_std");
        exit(1);
    }
    char* content = malloc(sizeof(char) * UINT32_MAX);
    int n_read = read(fd, content, sizeof(content));
    if (n_read < 0) {
        perror("read in send_std");
        close(fd);
        exit(1);
    }
    STRING string = {.length = n_read, .content = content};
    clyde = open_rep();
    rep = htobe16(SERVER_REPLY_OK);
    if (write(clyde, &rep, sizeof(rep)) < 0) goto write_error;
    send_string(string);
    free(content);
    close(fd);

     return;

     write_error :
     perror("write in send_std");
     close(clyde);
     exit(1);

}


/// Time and exit code

void send_time_and_exitcode(u_int64_t taskid) {
    struct stat st;

    int clyde;
    uint16_t rep;
    uint16_t error_type;

    u_int32_t nb_exec = 0;

    char file_path[32];
    sprintf(file_path, "daemon_dir/%ld", taskid);
    if (stat(file_path, &st) < 0) {
        /* Task may have been deleted */
        sprintf(file_path, "daemon_dir/trash_bin/%ld", taskid);
        if (stat(file_path, &st) < 0) {
            /* Task not found */
            clyde = open_rep();
            rep = htobe16(SERVER_REPLY_ERROR);
            if (write(clyde, &rep, sizeof(rep)) < 0) goto write_error;
            error_type = htobe16(SERVER_REPLY_ERROR_NOT_FOUND);
            if (write(clyde, &error_type, sizeof(error_type)) < 0) goto write_error;
            close(clyde);
            return;
        }
    }


    clyde = open_rep();
    rep = htobe16(SERVER_REPLY_OK);
    if (write(clyde, &rep, sizeof(rep)) < 0) goto write_error;

    strcat(file_path, "/return_values");

    DIR* dir = opendir(file_path);
    struct dirent* entry;

    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            nb_exec++;
        }
    }

    u_int32_t test = htobe32(nb_exec);
    if (write(clyde, &test, sizeof(test)) <0) {
        closedir(dir);
        goto write_error;
    }

    rewinddir(dir);

    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char* file_name = malloc(512);
            sprintf(file_name, "%s/%s", file_path, entry->d_name);
            char* buf = malloc(sizeof(uint16_t));
            int fd = open(file_name, O_RDONLY);
            if (fd < 0) {
                perror("open in send_time_and_exitcode");
                closedir(dir);
                exit(1);
            }
            int n_read = read(fd, buf, sizeof(buf));
            if (n_read < 0) {
                perror("read in send_time_and_exitcode");
                close(fd);
                closedir(dir);
                exit(1);
            }
            free(file_name);
            close(fd);
            char* timestamp = malloc(sizeof(char) * 20);
            strncpy(timestamp, entry->d_name, 19);
            u_int64_t sec = htobe64(int64_output_from_timestamp(timestamp));
            if (write(clyde, &sec, sizeof(sec)) < 0) goto write_error;
            u_int16_t code = htobe16(atol(buf));
            if (write(clyde, &code, sizeof(code)) < 0) goto write_error;
            free(timestamp);

        }

    }

    close(clyde);
    closedir(dir);
    return;

    write_error :
        perror("write in send_time_and_exitcode");
        close(clyde);
        exit(1);
}
