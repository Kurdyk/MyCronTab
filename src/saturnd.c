#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <endian.h>

#include "cassini.h"
#include "timing.h"
#include "timing-text-io.h"
#include "helpers.h"


#define PIPES_MODE 0777


struct stat st = {0};


void ensure_directory_exists(const char *path){
    if (stat(path, &st) == -1) {
        mkdir(path, 0777);
        chmod(path, 0777);
    }
}


int timings_data_file() {
    char* directory = "daemon_dir";
    char path[64];
    ensure_directory_exists(directory);
    sprintf(path, "%s/timings.txt", directory );
    int fd = open(path, O_RDWR | O_APPEND | O_CREAT, 0666);
    if (fd < 0)
    {
        perror("open");
        exit(-1);
    }
    return fd;
}


char* my_cat(char* start, char* end) {
    char* res = malloc(sizeof(char) * (strlen(start) + strlen(end)) - 1);
    strcat(res, start);
    strcat(res, end);
    return res;
}


void create_task_folder(TASK task) {
    char* directory = "daemon_dir";
    char path[128];
    char* files[] = {"/args.txt", "/task_timing.txt", "/return_values.txt", "/standard_out.txt", "/error_out.txt"};
    sprintf(path, "%s/%ld", directory, task.taskid);
    ensure_directory_exists(directory);
    ensure_directory_exists(path);
    for (int i = 0; i < 5; i++) {
        char* file_name = my_cat(path, files[i]);
        int fd;
        if ((fd = open(file_name, O_RDWR | O_APPEND | O_CREAT, 0666))== -1) {
            perror("open");
            exit(1);
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
}


void create_task(TASK task, PIPES* pipes) {
    create_task_folder(task);
    notify_timing(task);
    uint16_t rep = htobe16(SERVER_REPLY_OK);
    uint64_t id = htobe64(task.taskid);
    write(pipes->clyde, &rep, sizeof(uint16_t));
    write(pipes->clyde, &id, sizeof(uint64_t));
}


TASK* read_task(PIPES* pipes, uint8_t* current_id) {
    TASK* res = malloc(sizeof(TASK));
    read_timing(pipes, &(res->timing));
    res->commandline = read_commandline(pipes);
    res->taskid = *current_id;
    (*current_id)++;
    return res;
}


PIPES * open_pipes(char *path){
    //ensure_directory_exists(path);
    char * bonny_name = malloc(sizeof(char) * 110);
    char * clyde_name = malloc(sizeof(char) * 110);
    sprintf(bonny_name, "%s/bonny.pipe", path);
    sprintf(clyde_name, "%s/clyde.pipe", path);


    mkfifo(bonny_name, PIPES_MODE);
    mkfifo(clyde_name, PIPES_MODE);

    printf("%s\n%s\n", bonny_name, clyde_name);

    int fdread = open(bonny_name, O_RDONLY);
    int fdwrite = open(clyde_name, O_WRONLY);

    if (fdwrite == -1 || fdread == -1){
        perror("open");
        exit(1);
    }

    printf("On a ouvert Bonny and Clyde\n");
    free(bonny_name);
    free(clyde_name);
    PIPES * pipes = malloc(sizeof(PIPES));
    pipes->bonny = fdread;
    pipes->clyde = fdwrite;
    return pipes;
}


void close_pipes(PIPES* pipes) {
    close(pipes->bonny);
    close(pipes->clyde);
}

int main(int argc, char **argv){

    uint8_t current_id = 0;

    char * username = malloc(sizeof(char) * 50);
    getlogin_r(username, 50);

    char * pipes_directory = malloc(sizeof(char) * 100);
    //sprintf(pipes_directory, "/tmp/%s/saturnd/pipes", username);
    sprintf(pipes_directory, "/tmp");

    free(username);
    PIPES * pipes = open_pipes(pipes_directory);

    int i = 1;
    uint16_t* demande = malloc(sizeof(u_int16_t));

    while (i > 0) {
        // lecture dans le tube
        if (read(pipes->bonny, demande, sizeof(uint64_t)) > 0) {
            uint16_t operation = be16toh(*demande);
            switch (operation) {
                case CLIENT_REQUEST_CREATE_TASK:
                    create_task(*read_task(pipes, &current_id), pipes);
                    break;
            }
            //printf("Message de cassini : %s\n", demande);
        }

        printf("On a scanné, et on a rien trouvé");
        sleep(1);
    }


    //test
    STRING arg;
    arg.length = 5;
    arg.content = "echo";

    STRING arg1;
    arg1.length = 5;
    arg1.content = "blab";

    STRING* blop[] = {&arg, &arg1};

    COMMANDLINE cmdline;
    cmdline.argc = 2;
    cmdline.arguments = blop;

    TIMING t;
    t.minutes = 1;
    t.hours = 18;
    t.daysofweek = 6;

    TASK test;
    test.taskid = 1;
    test.timing = t;
    test.commandline = &cmdline;

    //create_task(test);

    close_pipes(pipes);
    free(pipes);
    return EXIT_SUCCESS;
}