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

int open_rep() {
    /**
     * Ouvre le tube de réponse
     */
    char * username = malloc(sizeof(char) * 50);
    getlogin_r(username, 50);
    char * pipes_directory = malloc(sizeof(char) * 100);
    sprintf(pipes_directory, "/tmp/%s/saturnd/pipes", username);
    free(username);
    //sprintf(pipes_directory, "/tmp");
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
    /**
     * Génère les pipes.
     */
    //ensure_directory_exists(path);
    char * bonny_name = malloc(sizeof(char) * 110);
    char * clyde_name = malloc(sizeof(char) * 110);
    sprintf(bonny_name, "%s/saturnd-request-pipe", path);
    sprintf(clyde_name, "%s/saturnd-reply-pipe", path);

    mkfifo(bonny_name, PIPES_MODE);
    mkfifo(clyde_name, PIPES_MODE);

    printf("On a crée Bonny and Clyde\n");
    free(bonny_name);
    free(clyde_name);

}


int main(int argc, char **argv){

    ensure_directory_exists("daemon_dir");
    ensure_directory_exists("daemon_dir/trash_bin");
    u_int64_t next_id = 1;
    set_next_id(&next_id, "daemon_dir");
    set_next_id(&next_id, "daemon_dir/trash_bin");


    char * username = malloc(sizeof(char) * 50);
    getlogin_r(username, 50);
    char path1[128];
    sprintf(path1, "/tmp/%s", username);
    ensure_directory_exists(path1);
    sprintf(path1, "/tmp/%s/saturnd", username);
    ensure_directory_exists(path1);
    sprintf(path1, "/tmp/%s/saturnd/pipes", username);
    ensure_directory_exists(path1);


    char * pipes_directory = malloc(sizeof(char) * 100);
    sprintf(pipes_directory, "/tmp/%s/saturnd/pipes", username);
    //sprintf(pipes_directory, "/tmp");
    free(username);
    open_pipes(pipes_directory);
    char *bonny_name = malloc(sizeof(char) * 110);
    sprintf(bonny_name, "%s/saturnd-request-pipe", pipes_directory);
    //sprintf(bonny_name, "%s/bonny.pipe", pipes_directory);
    int bonny = open(bonny_name, O_NONBLOCK | O_RDONLY);
    int clyde;
    free(pipes_directory);

    pid_t child_pid = fork();
    if (child_pid == 0) { //Partie execution de taches
        while(1) {
            check_exec_time();
            sleep(60);
        }
    } else { //Partie communication avec Cassini
        uint16_t *demande = malloc(sizeof(u_int16_t));

        struct pollfd survey[1];

        survey[0].fd = bonny;
        survey[0].events = POLLIN;


        u_int64_t *id_buf;
        u_int64_t id;
        int n_read;


        while (1) {

            poll(survey, 1, -1);
            if (survey[0].revents & POLLHUP) {
                close(bonny);
                bonny = open(bonny_name, O_NONBLOCK | O_RDONLY);
                continue;
            }
            // lecture dans le tube
            if ((n_read = read(bonny, demande, sizeof(uint16_t))) > 0) {
                uint16_t operation = be16toh(*demande);
                switch (operation) {
                    case CLIENT_REQUEST_CREATE_TASK:
                        ;
                        TASK* task = read_task(bonny);
                        create_task(*task, &next_id);
                        free_task(task);
                        break;
                    case CLIENT_REQUEST_TERMINATE:
                        terminate();
                        free(demande);
                        goto exit_succes;
                        break;
                    case CLIENT_REQUEST_LIST_TASKS:
                        {
                            int clyde = open_rep();
                            listTasks(clyde);
                            close(clyde);
                            break;
                        }
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
                        memset(name, 0, 16);
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
                    default:
                        break;
                }
            }

            if (n_read < 0) {
                printf("%d", n_read);
                perror("read in main saturnd");
                close(bonny);
                free(bonny_name);
                kill(child_pid, SIGKILL);
                exit(EXIT_FAILURE);
            }

        }

        goto exit_succes;
    }

    exit_succes:
    free(bonny_name);
    close(bonny);
        kill(child_pid, SIGKILL);
        exit(EXIT_SUCCESS);

}

