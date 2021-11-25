#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define PIPES_MODE 0777


typedef struct 
{
  int bonny; // Pipe cassini -> starurnd
  int clyde; // Pipe saturnd -> cassini
} PIPES;

struct stat st = {0};


void ensure_directory_exists(const char *path){
    if (stat(path, &st) == -1) {
        mkdir(path, 0777);
    }
}

PIPES * open_pipes(char *path){
    ensure_directory_exists(path);
    char * bonny_name = malloc(sizeof(char) * 110);
    char * clyde_name = malloc(sizeof(char) * 110);
    sprintf(bonny_name, "%s/bonny.pipe", path);
    sprintf(clyde_name, "%s/clyde.pipe", path);

    mkfifo(bonny_name, PIPES_MODE);
    mkfifo(clyde_name, PIPES_MODE);


    int fdread = open(bonny_name, O_RDONLY);
    int fdwrite = open(clyde_name, O_WRONLY);
    if (fdwrite == -1 || fdread == -1){
        return NULL;
    }
    printf("On a ouvert Bonny and Clyde\n");
    free(bonny_name);
    free(clyde_name);
    PIPES * pipes = malloc(sizeof(PIPES));
    pipes->bonny = fdwrite;
    pipes->clyde = fdread;
    return pipes;
}

int main(int argc, char **argv){

    char * username = malloc(sizeof(char) * 50);
    getlogin_r(username, 50);

    char * pipes_directory = malloc(sizeof(char) * 100);
    sprintf(pipes_directory, "/tmp/%s/saturnd/pipes", username);
    PIPES * pipes = open_pipes(pipes_directory);

    int i = 1;
    char * message = malloc(sizeof(char) * 250);
    while (i > 0) {
        // lecture dans le tube
        if (read(pipes->bonny, message, 250) > 0) {
            printf("Message de cassini : %s\n", message);
        }
        printf("On a scanné, et on a rien trouvé");
        sleep(1);
    }

    return EXIT_SUCCESS;
}