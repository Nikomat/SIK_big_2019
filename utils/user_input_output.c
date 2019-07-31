//
// Created by nikodem on 28.07.19.
//

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <zconf.h>

#include "user_input_output.h"
#include "command_utils.h"

int strncicmp(char const *a, char const *b, size_t __n);

struct UserInput getUserInput() {
    char *line_buf = NULL;
    size_t line_buf_size = 0;
    ssize_t line_size;

    line_size = getline(&line_buf, &line_buf_size, stdin);

    struct UserInput userInput;
    userInput.full_command = line_buf;
    userInput.command_size = line_size;
    userInput.action = UNKNOWN_ACTION;
    userInput.arg = NULL;

    for (int i = 0; i < ACTION_ENUM_SIZE; i++) {
        if (strncicmp(Action[i], line_buf, strlen(Action[i])) == 0 && isspace(line_buf[strlen(Action[i])])) {
            userInput.action = i;
            if (line_size > strlen(Action[i]) + 1) {
                userInput.arg = line_buf + strlen(Action[i]) + 1;
            }
        }
    }
    return userInput;
}

int strncicmp(char const *a, char const *b, size_t __n) {
    for (size_t i = 0; i < __n; i++, a++, b++) {
        int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
        if (d != 0 || !*a)
            return d;
    }
    return 0;
}

void printSimplCmd(struct SIMPL_CMD* cmd) {
    debugLog("SIMPLE COMMAND: \n");
    debugLog("\t[cmd] = %.10s (%d)\n", cmd->cmd, getCommand(cmd->cmd));
    debugLog("\t[cmd_seq] = %lu\n", cmd->cmd_seq);
    debugLog("\t[data] = %s\n", cmd->data);
}

void printCmplxCmd(struct CMPLX_CMD* cmd) {
    debugLog("COMPLEX COMMAND: \n");
    debugLog("\t[cmd] = %.10s (%d)\n", cmd->cmd, getCommand(cmd->cmd));
    debugLog("\t[cmd_seq] = %lu\n", cmd->cmd_seq);
    debugLog("\t[param] = %lu\n", cmd->param);
    debugLog("\t[data] = %s\n", cmd->data);
}

void debugLog(const char *__restrict __format, ...) {
    if (DEBUG) {
        printf("\033[0;36m");
        va_list args;
        va_start(args, __format);
        vprintf(__format, args);
        va_end(args);
        printf("\033[0m");
    }
}