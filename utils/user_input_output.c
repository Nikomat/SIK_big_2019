//
// Created by nikodem on 28.07.19.
//

#include "user_input_output.h"

int strncicmp(char const *a, char const *b, size_t __n);

char* strTrim(char* str);

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
            userInput.arg = strTrim(line_buf + strlen(Action[i]));

            // Sprawdzenie czy występuje prawidłowy argument dla tej akcji
            if (strlen(userInput.arg) > 0 && isActionArgObligatory[i] == -1) {
                userInput.action = UNKNOWN_ACTION;
            }
            if (strlen(userInput.arg) <= 0 && isActionArgObligatory[i] == 1) {
                userInput.action = UNKNOWN_ACTION;
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

char* strTrim(char* str) {
    size_t max_len = strlen(str);
    size_t i = max_len - 1;
    while (isspace(*(str + i)) && i>=0) {
        *(str + i) = '\0';
        i--;
    }
    max_len = strlen(str);
    i = 0;
    while (isspace(*str) && i < max_len) {
        str++;
        i++;
    }
    return str;
}

void printCmdError(struct sockaddr_in address, const char *__restrict __format, ...) {
    printf("[PCKG ERROR] Skipping invalid package from %s:%d. ", inet_ntoa(address.sin_addr), address.sin_port);
    va_list args;
    va_start(args, __format);
    vprintf(__format, args);
    va_end(args);
    printf("\n");
}

void printSimplCmd(struct SIMPL_CMD* cmd) {
    debugLog("\tSIMPLE COMMAND: \n");
    debugLog("\t[cmd] = %.10s (%d)\n", cmd->cmd, getCommand(cmd->cmd));
    debugLog("\t[cmd_seq] = %lu\n", cmd->cmd_seq);
    debugLog("\t[data] = %s\n", cmd->data);
}

void printCmplxCmd(struct CMPLX_CMD* cmd) {
    debugLog("\tCOMPLEX COMMAND: \n");
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