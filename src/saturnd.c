#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <endian.h>
#include <signal.h>
#include <poll.h>

#include "timing.h"
#include "timing-text-io.h"
#include "helpers.h"
#include "saturnd_struct.h"
#include "saturnd.h"

#define PIPES_MODE 0777


void terminate() {
    //printf("Terminate\n");
    uint16_t rep = htobe16(SERVER_REPLY_OK);
    int clyde = open_rep();
    write(clyde, &rep, sizeof(uint16_t));
    close(clyde);
}

int open_rep() {
    char * pipes_directory = malloc(sizeof(char) * 100);
    //sprintf(pipes_directory, "/tmp/%s/saturnd/pipes", username);
    sprintf(pipes_directory, "/tmp");
    char *clyde_name = malloc(sizeof(char) * 110);
    //sprintf(clyde_name, "%s/saturnd-reply-pipe", pipes_directory);
    sprintf(clyde_name, "%s/saturnd-reply-pipe", pipes_directory);

    int clyde;
    if ((clyde = open(clyde_name, O_WRONLY)) < 0) {
        perror("open");
        exit(1);
    }
    free(pipes_directory);
    free(clyde_name);
    return clyde;
}

void open_pipes(char *path){
    //ensure_directory_exists(path);
    char * bonny_name = malloc(sizeof(char) * 110);
    char * clyde_name = malloc(sizeof(char) * 110);
    sprintf(bonny_name, "%s/saturnd-request-pipe", path);
    sprintf(clyde_name, "%s/saturnd-reply-pipe", path);

    mkfifo(bonny_name, PIPES_MODE);
    mkfifo(clyde_name, PIPES_MODE);

    printf("%s\n%s\n", bonny_name, clyde_name);

    printf("On a crée Bonny and Clyde\n");
    free(bonny_name);
    free(clyde_name);

}


int main(int argc, char **argv){

    ensure_directory_exists("daemon_dir");
    u_int64_t next_id = 1;
    set_next_id(&next_id, "daemon_dir");
    set_next_id(&next_id, "daemon_dir/trash_bin");


    char * username = malloc(sizeof(char) * 50);
    getlogin_r(username, 50);

    char * pipes_directory = malloc(sizeof(char) * 100);
    //sprintf(pipes_directory, "/tmp/%s/saturnd/pipes", username);
    sprintf(pipes_directory, "/tmp");
    free(username);
    open_pipes(pipes_directory);
    char *bonny_name = malloc(sizeof(char) * 110);
    sprintf(bonny_name, "%s/saturnd-request-pipe", pipes_directory);
    //sprintf(bonny_name, "%s/bonny.pipe", pipes_directory);
    int bonny = open(bonny_name, O_NONBLOCK | O_RDONLY);
    int clyde;
    free(bonny_name);

    pid_t child_pid = fork();
    if (child_pid == 0) { //partie execution de taches
        while(1) {
            sleep(1);
            //check_exec_time();
            sleep(59);
        }
    } else {
        uint16_t *demande = malloc(sizeof(u_int16_t));
        struct pollfd survey;
        survey.fd = bonny;
        survey.events = POLLIN;


        u_int64_t *id_buf;
        u_int64_t id;

        while (1) {
            //break;
            poll(&survey, bonny + 1, -1);
            // lecture dans le tube
            if (read(bonny, demande, sizeof(uint16_t)) > 0) {
                uint16_t operation = be16toh(*demande);
                switch (operation) {
                    case CLIENT_REQUEST_CREATE_TASK:
                        create_task(*read_task(bonny), &next_id);
                        break;
                    case CLIENT_REQUEST_TERMINATE:
                        terminate();
                        goto exit_succes;
                        break;
                    case CLIENT_REQUEST_REMOVE_TASK:
                        id_buf = malloc(sizeof(u_int64_t));
                        read(bonny, id_buf, sizeof(u_int64_t));
                        id = be64toh(*id_buf);
                        int rem = remove_task(id);
                        free(id_buf);
                        clyde = open_rep();
                        if(rem) {
                            uint16_t rep = htobe16(SERVER_REPLY_OK);
                            write(clyde, &rep, sizeof(uint16_t));
                        } else {
                            uint16_t rep = htobe16(SERVER_REPLY_ERROR);
                            uint16_t code = htobe16(SERVER_REPLY_ERROR_NOT_FOUND);
                            write(clyde, &rep, sizeof(u_int16_t));
                            write(clyde, &code, sizeof(u_int16_t));
                        }
                        close(clyde);
                        break;
                    case CLIENT_REQUEST_GET_STDERR: case CLIENT_REQUEST_GET_STDOUT:
                        ;
                        char name[16];
                        if(operation == CLIENT_REQUEST_GET_STDOUT) {
                            sprintf(name, "standard_out");
                        } else {
                            sprintf(name, "error_out");
                        }
                        id_buf = malloc(sizeof(u_int64_t));
                        read(bonny, id_buf, sizeof(u_int64_t));
                        id = be64toh(*id_buf);
                        free(id_buf);
                        send_std(name, id);
                        break;
                    case CLIENT_REQUEST_GET_TIMES_AND_EXITCODES:
                        id_buf = malloc(sizeof(u_int64_t));
                        read(bonny, id_buf, sizeof(u_int64_t));
                        id = be64toh(*id_buf);
                        free(id_buf);
                        send_time_and_exitcode(id);
                        break;

                }
                //printf("Message de cassini : %s\n", demande);
            }

            //printf("On a scanné, et on a rien trouvé");
            sleep(1);
        }
        free(demande);

        //test
        STRING arg;
        arg.length = 5;
        arg.content = "echo";

        STRING arg1;
        arg1.length = 5;
        arg1.content = "blab";

        STRING *blop[] = {&arg, &arg1};

        COMMANDLINE cmdline;
        cmdline.argc = 2;
        cmdline.arguments = blop;

        TIMING t;
        t.minutes = 1;
        t.hours = 18;
        t.daysofweek = 6;

        TASK test;
        test.timing = t;
        test.commandline = &cmdline;


        //create_task(test, &next_id);
        //exec_task_from_id(1);
        //remove_task(4);
        //send_std("error_out", 1);
        //send_time_and_exitcode(1);

        sleep(1);
        goto exit_succes;
    }

    exit_succes:
        free(pipes_directory);
        close(bonny);
        kill(child_pid, SIGKILL);
        return EXIT_SUCCESS;

}

