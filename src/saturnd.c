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
    set_next_id(&next_id);

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

    pid_t child_pid = fork();
    if (child_pid == 0) { //partie execution de taches
        while(1) {
            //TODO : verif non vide deamon_dir
            sleep(1);
            check_exec_time();
            sleep(59);
        }
    } else {
        uint16_t *demande = malloc(sizeof(u_int16_t));
        struct pollfd survey;
        survey.fd = bonny;
        survey.events = POLLIN;

        while (1) {
            //break;
            poll(&survey, bonny + 1, -1);
            // lecture dans le tube
            if (read(bonny, demande, sizeof(uint64_t)) > 0) {
                uint16_t operation = be16toh(*demande);
                switch (operation) {
                    case CLIENT_REQUEST_CREATE_TASK:
                        create_task(*read_task(bonny), &next_id);
                        break;
                    case CLIENT_REQUEST_TERMINATE:
                        terminate();
                        goto exit_succes;
                }
                //printf("Message de cassini : %s\n", demande);
            }

            //printf("On a scanné, et on a rien trouvé");
            sleep(1);
        }


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

        sleep(2);
        goto exit_succes;
    }

    exit_succes:
        close(bonny);
        kill(child_pid, SIGKILL);
        return EXIT_SUCCESS;

}

