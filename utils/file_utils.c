//
// Created by nikodem on 31.07.19.
//

#include <dirent.h>
#include <z3.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "file_utils.h"
#include "err.h"
#include "user_input_output.h"


char* getFiles(char* dir_path) {
    DIR *directory;
    struct dirent *node;
    directory = opendir(dir_path);

    uint32_t message_len = 0;
    char *message = malloc(0);

    if (directory) {
        while ((node = readdir(directory)) != NULL) {
            if (node->d_type == DT_REG) {
                message = realloc(message, (strlen(node->d_name) + message_len + 1) * sizeof(char));
                memcpy(message + message_len, node->d_name, strlen(node->d_name));
                message_len += strlen(node->d_name) + 1;
                message[message_len - 1] = '\n';
            }
        }
        message[message_len - 1] = '\0';
        closedir(directory);
    } else {
        fatal("given path to directory is incorrect");
    }
    return message;
}


size_t getFilesSize(SHRD_FLDR) {
    size_t directory_size;
    struct stat st;
   // if (stat(node->d_name, &st) == 0)
    //    directory_size += st.st_size;
    return 0;
}