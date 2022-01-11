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
#include "timing.h"
#include "../include/saturnd_struct.h"
#include "../include/helpers.h"
#include "../include/server-reply.h"
#include "../include/saturnd.h"



int getLine(int fd, char * buffer, int max_length){
    memset(buffer, 0, max_length);
    int r = read(fd, buffer, max_length);
    int i;
    for (i = 0; i < r; i ++){
        if (*(buffer + i) == '\n'){
            break;
        } 
    }

    *(buffer + i) = 0;
    lseek(fd, (i  - r + 1), SEEK_CUR);
    return i;
}


/// Create task


void create_task_folder(TASK task) {
    /**
     * Création du dossier lié à un tâche avec enregistrement des ses arguments, son temps d'exécution,
     * et des dossiers pour sauvegarder les sorties standards et erreurs ainsi que les codes de retour.
     */
    char* directory = "daemon_dir";
    char* path = malloc(128);
    char* files[] = {"/args.txt", "/task_timing.txt", "/return_values/", "/standard_out/", "/error_out/"};
    memset(path, 0, 128);
    sprintf(path, "%s/%ld", directory, task.taskid);
    ensure_directory_exists(path);
    for (int i = 0; i < 5; i++) {
        char* file_name = my_cat(path, files[i]);
        int fd;
        if (i < 2) { //Cas de création d'un fichier simple
            if ((fd = open(file_name, O_WRONLY | O_CREAT, 0666)) == -1) {
                perror("open");
                exit(1);
            }
        } else { //Cas de création d'un dossier
            ensure_directory_exists(file_name);
        }
        free(file_name);
        char buf[TIMING_TEXT_MIN_BUFFERSIZE];
        memset(buf, 0, TIMING_TEXT_MIN_BUFFERSIZE);

        switch(i) {
            case 0: //Recopie des arguments demandés dans le fichier args.txt
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
                close(fd);
                break;
            case 1: //Recopie du timing demandé dans timing.txt
                timing_string_from_timing(buf, &(task.timing));
                if (write(fd, buf, strlen(buf)) < 0) {
                    perror("write");
                    exit(1);
                }
                close(fd);
                break;
            default: //others
                break;
        }
    }
    free(path);
}


void notify_timing(TASK task) {
    /**
     * A chaque création de tâches on ajoute son timing à un fichier qui sera lu pour déclencher les exécutions
     */
    int fd = open("daemon_dir/timings.txt", O_WRONLY | O_APPEND | O_CREAT, 0666);
    flock(fd, LOCK_EX);
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
    flock(fd, LOCK_UN);
    close(fd);
}

void create_task(TASK task, u_int64_t* taskid) {
    /**
     * Centralise les différentes étapes de la création d'une tâche.
     */
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
    /**
     * Attribue son numéro à une tâche et incrémente le compteur de numéro de tâche.
     */
    task->taskid = *next_id;
    *next_id += 1;
}


int is_number(char* input) {
    /**
     * Retourne 1 si input représente un nombre, 0 sinon.
     */
    for (int i = 0; i < strlen(input); i++) {
        if(!isdigit(*(input + i))) return 0;
    }
    return 1;
}

void set_next_id(uint64_t* next_id, char* path) {
    /**
     * Au lancement de Saturnd, cherche le prochain numéro de tâche disponible.
     */
    DIR *dir = opendir(path);
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if(is_number(entry->d_name)) {
            uint64_t max = (atol(entry->d_name) < *next_id) ? *next_id : atol(entry->d_name) + 1;
            *next_id = max;
        }
    }
    closedir(dir);
}


/// List tasks


COMMANDLINE * createCOMMANDLINE(char * commandline){
    char * cpy1 = malloc(strlen(commandline));
    char * cpy2 = malloc(strlen(commandline));
    strcpy(cpy1, commandline);
    strcpy(cpy2, commandline);
    char * arg = strtok(cpy1, " ");
    int argc = 1;
    while (strtok(NULL, " ") != NULL){
        argc++;
    }
    free(cpy1);
    COMMANDLINE * cmd = malloc(sizeof(COMMANDLINE));
    cmd->argc = argc;
    cmd->arguments = malloc(argc * sizeof(STRING));
    arg = strtok(cpy2, " ");
    STRING * s;
    int l;
    for (int i = 0; i < argc; i++){
        l = strlen(arg);
        s = malloc(sizeof(STRING));
        s->length = l;
        s->content = malloc(l);
        memset(s->content, 0, l);
        strcpy(s->content, arg);
        cmd->arguments[i] = s;
        arg = strtok(NULL, " ");
    }
    return cmd;
}

void listTasks(int clyde){
    struct stat buffer;
    if (stat("daemon_dir/timings.txt", &buffer) < 0){
        uint16_t reptype = htobe16(SERVER_REPLY_OK);
        uint32_t nbtasks = 0;
        write(clyde, &reptype, sizeof(uint16_t));
        write(clyde, &nbtasks, sizeof(uint32_t));
        return;
    }
    int timings = open("daemon_dir/timings.txt", O_RDONLY);
    flock(timings, LOCK_EX);
    int max_length = 256;
    char * buff = malloc(max_length);
    uint32_t nbtasks = 0;
    int command_file;
    uint64_t taskid;
    char * filename = malloc(1024);
    char * taskid_s;
    char * mins;
    char * hours;
    char * days;
    char * commandline_s = malloc(1024);
    char * reste;
    TIMING t;

    COMMANDLINE * cmd;

    while (getLine(timings, buff, max_length) != 0){
        nbtasks++;
    }

    uint16_t reptype = htobe16(SERVER_REPLY_OK);
    uint32_t nbtasks_r = htobe32(nbtasks);
    write(clyde, &reptype, sizeof(uint16_t));
    write(clyde, &nbtasks_r, sizeof(uint32_t));

    lseek(timings, 0, SEEK_SET);

    while (getLine(timings, buff, max_length) != 0){
        taskid_s = strtok(buff, " ");
        mins = strtok(NULL, " ");
        hours = strtok(NULL, " ");
        days = strtok(NULL, " ");
        reste = strtok(NULL, " ");
        timing_from_strings(&t, mins, hours, days);
        memset(filename, 0, 1024);
        sprintf(filename, "daemon_dir/%s/args.txt", taskid_s);
        command_file = open(filename, O_RDONLY);
        memset(commandline_s, 0, 1024);
        int r = read(command_file, commandline_s, max_length);
        close(command_file);
        cmd = createCOMMANDLINE(commandline_s);
        uint64_t taskid = htobe64(atol(taskid_s));
        write(clyde, &taskid, sizeof(uint64_t));
        t.minutes = htobe64(t.minutes);
        t.hours = htobe32(t.hours);
        write(clyde, &(t.minutes), sizeof(uint64_t));
        write(clyde, &(t.hours), sizeof(uint32_t));
        write(clyde, &(t.daysofweek), sizeof(uint8_t));
        uint32_t argc_r = htobe32(cmd->argc);
        write(clyde, &(argc_r), sizeof(uint32_t));
        for (int i = 0; i < cmd->argc; i++){
            cmd->arguments[i]->length = htobe32(cmd->arguments[i]->length);
            write(clyde, &(cmd->arguments[i]->length), sizeof(uint32_t));
            write(clyde, cmd->arguments[i]->content, strlen(cmd->arguments[i]->content));
            free(cmd->arguments[i]->content);
            free(cmd->arguments[i]);
        }
        free(cmd->arguments);
        free(cmd);
    }
    flock(timings, LOCK_UN);
    close(timings);
}


/// Structure helpers

void ensure_directory_exists(const char *path){
    /**
     * Vérifie si un dictionnaire existe, le crée sinon.
     */
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0777);
        chmod(path, 0777);
    }
}


char* my_cat(char* start, char* end) {
    /**
     * Alloue une chaîne de caractère pour concaténer start et end.
     * A FREE
     */
    size_t size = sizeof(char) * (strlen(start) + strlen(end) + 10);
    char* res = malloc(size);
    memset(res, 0, size);
    strcat(res, start);
    strcat(res, end);

    return res;
}


/// Execution

void check_exec_time() {
    /**
     * Vérifie le fichier de temps d'exécution et lance les tâches à exécuter.
     */
    struct stat st = {0};
    if (stat("daemon_dir/timings.txt", &st) < 0) { //Cas pas de tâche
        return;
    }
    size_t max_len = 256;
    char* buf = malloc(sizeof(char) * 256);
    memset(buf, 0, 256);
    int timings_fd;
    timings_fd = open("daemon_dir/timings.txt", O_RDONLY);
    flock(timings_fd, LOCK_EX); //On s'assure que Saturnd ne modifie pas le fichier en même temps.
    while (getLine(timings_fd, buf, max_len) > 0) {
        int i = 0;
        while (isspace(buf[i]) == 0) {
            i++;
        }
        char* subtext_id = malloc(sizeof(char) * i + 1);
        memset(subtext_id, 0, sizeof(char) * i + 1);
        strncpy(subtext_id, buf, i);
        uint64_t id = atol(subtext_id);
        free(subtext_id);

        TIMING * timing = malloc(sizeof(TIMING));
        char delim[] = " \n";
        char * mins = strtok((buf + i  + 1), delim);
        char * hours = strtok(NULL, delim);
        char * days = strtok(NULL, delim);
        char * reste = strtok(NULL, delim);
        //printf("Parsé : Jour %s à %s:%s\n", days, hours, mins);
        timing_from_strings(timing, mins, hours, days);
        time_t now = time(NULL);
        struct tm * time_now = localtime(&now);
        //printf("On est le %d jour de la semaine. Il est %d:%d\n", time_now->tm_wday, time_now->tm_hour, time_now->tm_min);
        if ((timing->hours & (1<<(time_now->tm_hour)))){
            if ((timing->minutes & (1<<(time_now->tm_min)))){
                if ((timing->daysofweek & (1<<(time_now->tm_wday)))){
                    //printf("Lancement de la tache %d\n", id);
                    exec_task_from_id(id);
                } else {
                    //printf("Erreur dans le jour\n");
                }
            } else {
                //printf("Erreur dans les minutes\n");
            }
        } else {
            //printf("Erreur dans les heures\n");
        }
        free(timing);
    }
    flock(timings_fd, LOCK_UN);
    close(timings_fd);
    free(buf);
}


void execute(char* argv[], char* ret_file, char* out_file, char* err_file) {
    /**
     * Lance l'exécution d'une tâche après redirection de la sortie et de l'erreur standards.
     * Un fork permet au père de récupérer le code de retour du fils de de le conserver dans un fichier.
     */
    int status;
    int ret_fd = -1;
    int out_fd = -1;
    int err_fd = -1;

    int r = fork();

    if (r == 0) {

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

        execvp(argv[0], argv);
        //execvp failed
        perror("execvp in execute");
        if (ret_fd >= 0) close(ret_fd);
        if (out_fd >= 0) close(out_fd);
        if (err_fd >= 0) close(err_fd);
        //raise(SIGKILL);
        _exit(127);
        return;

    } else {

        if (ret_file != NULL) {
            ret_fd = open(ret_file, O_WRONLY | O_CREAT, 0666);
            if (ret_fd < 0) goto open_error;
        }

        /* the parent process calls wait() on the child */
        if (waitpid(r, &status, 0) > 0) {
            if (WIFEXITED(status)) {
                //terminated in good condition
                int exit_status = WEXITSTATUS(status);
                char buf[256];
                sprintf(buf, "%d", exit_status);
                write(ret_fd, buf, (size_t) log10(abs((exit_status == 0)?1:exit_status)) + 1) ;
                close(ret_fd);
                raise(SIGKILL);
            } else {
                //child got a problem
                goto child_error;
            }
        } else {
            //wait failed
            goto wait_error;
        }
    }

    close(ret_fd);
    raise(SIGKILL);
    return;

    wait_error:
    perror("wait error in execute");
    if (ret_fd >= 0) close(ret_fd);
    if (out_fd >= 0) close(out_fd);
    if (err_fd >= 0) close(err_fd);
    raise(SIGKILL);
    return;

    child_error: //Peut arriver en cas de commande inconnue.
    if (ret_fd >= 0) close(ret_fd);
    if (out_fd >= 0) close(out_fd);
    if (err_fd >= 0) close(err_fd);
    raise(SIGKILL);
    return;

    open_error:
    perror("open error in execute");
    if (ret_fd >= 0) close(ret_fd);
    if (out_fd >= 0) close(out_fd);
    if (err_fd >= 0) close(err_fd);
    raise(SIGKILL);
    return;

    dup_error:
    perror("dup error in execute");
    if (ret_fd >= 0) close(ret_fd);
    if (out_fd >= 0) close(out_fd);
    if (err_fd >= 0) close(err_fd);
    raise(SIGKILL);
    return;

}

int line_to_tokens(char *line, char **tokens) {
    /**
     * Gotten from TP8.
     * Permet de parser les arguments d'une tâche pour execution.
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
    /**
     * Lance la tâche de numéro task_id.
     * Source de memory leaks mais je n'arrive pas à les corriger.
     */
    char task_dir[256];
    sprintf(task_dir, "daemon_dir/%ld/", task_id);

    char* date_buf = time_output_from_int64(time(NULL)); /*Le nom du fichier dépend de la date actuelle.*/
    date_buf = realloc(date_buf, sizeof(char) * FILE_NAME_LENGTH);
    strcat(date_buf, ".txt");

    char err_file[256];
    memset(err_file, 0, 256);
    strcat(strcpy(err_file, task_dir), "error_out/");
    strcat(err_file, date_buf);

    char ret_file[256];
    memset(ret_file, 0, 256);
    strcat(strcpy(ret_file, task_dir), "return_values/");
    strcat(ret_file, date_buf);

    char out_file[256];
    memset(out_file, 0, 256);
    strcat(strcpy(out_file, task_dir), "standard_out/");
    strcat(out_file, date_buf);

    char cmdLine_file[256];
    memset(cmdLine_file, 0, 256);
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
    close(fd_cmd);


    char** argv = malloc(sizeof(char*) * MAX_TOKENS);
    line_to_tokens(cmdLine, argv);

    free(date_buf);

    if (fork() == 0 ) {
        execute(argv, ret_file, out_file, err_file);
    }

    free(argv);
    free(cmdLine);



}

/// Remove task


int remove_task(u_int64_t taskid) {
    /**
     * Supprime la ligne de la tâche dans timings.txt, ce qui empêche son exécution.
     * Déplace le dossier de la tâche dans le dossier trash_bin.
     * On conserve comme ça les valeurs de retour en cas de volonté de consultation ultérieure.
     */
    char path[64];
    sprintf(path, "daemon_dir/%ld", taskid);
    char new_path[64];
    sprintf(new_path, "daemon_dir/trash_bin/%ld", taskid);
    rename(path, new_path);

    size_t max_len = 1024;
    char* buf = malloc(sizeof(char) * 1024);
    int old = open("daemon_dir/timings.txt", O_RDONLY);
    flock(old, LOCK_EX);
    int new = open("daemon_dir/timings_buff.txt", O_CREAT | O_WRONLY, 0666);
    ssize_t nread = 0;
    int find = 0;
    while ((nread = getLine(old, buf, max_len)) > 0) {
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
            strcat(buf, "\n");
            write(new, buf, strlen(buf));
        }
    }
    remove("daemon_dir/timings.txt");
    rename("daemon_dir/timings_buff.txt", "daemon_dir/timings.txt");
    chmod("daemon_dir/timings.txt", 0666);
    flock(old, LOCK_UN);
    close(old);
    close(new);
    free(buf);
    return find;

}



/// Get strout ou strerr



char* last_exec_name(u_int64_t taskid, int is_deleted) {
    /**
     * Donne le nom de fichier de la dernière exécution d'une tâche, celui-ci correspond à la date d'exécution.
     */
    char* prev = malloc(sizeof(char)*FILE_NAME_LENGTH);
    memset(prev, 0, FILE_NAME_LENGTH);

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
    /**
     * Envoie une string à Cassini.
     */
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
    /**
     * En fonction de ce qui est demandé par name, envoie à Cassini la dernière sortie standard ou erreur de la tâche
     * spécifiée par taskid.
     */
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
    int n_read = read(fd, content, UINT32_MAX);
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
    /**
     * Récupère toutes les dates d'exécution (par le nom des fichiers résultants d'une exécution) et les codes de retour
     * associés.
     */
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
        //Trouve le nombre d'exécutions à envoyer.
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


/// Terminate

void terminate() {
    /**
     * Envoie à Cassini que la demande de terminaison est bien reçue, termine dans saturnd.c.
     */
    uint16_t rep = htobe16(SERVER_REPLY_OK);
    int clyde = open_rep();
    write(clyde, &rep, sizeof(uint16_t));
    close(clyde);
}
