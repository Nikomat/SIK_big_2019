//
// Created by nikodem on 27.07.19.
//

#include "args_utils.h"

char* findArg(char* flag, int argc, char *argv[]) {
    for (int i = 0; i<argc; i++) {
        if (strcmp(argv[i], flag) == 0 && i+1 < argc) {
            return argv[i+1];
        }
    }
    return NULL;
}