//
// Created by nikodem on 26.07.19.
//

#include <string.h>
#include <zconf.h>
#include <stdio.h>
#include <stdlib.h>
#include "command_utils.h"
#include "err.h"

CommandE getCommand(char* string) {
    CommandE cmd = UNKNOWN_CMD;
    for (int i = 0; i < LAST_ENTRY; i++) {
        if (strncmp(Command[i], string, strlen(Command[i])) == 0) {
            cmd = i;
        }
    }
    return cmd;
}

struct SIMPL_CMD* createSimplCmd(const char cmd[10], uint64_t cmd_seq, char data[]) {
    size_t data_len = strlen(data) + 1; // + znak końca
    size_t cmd_size = sizeof(struct SIMPL_CMD) + data_len * sizeof(char);
    struct SIMPL_CMD* simpl_cmd = (struct SIMPL_CMD*) malloc(cmd_size);

    if (!simpl_cmd) {
        fatal("createSimplCmd malloc failed!");
    } else {
        memcpy(simpl_cmd->cmd, cmd, 10 * sizeof(char));
        simpl_cmd->cmd_seq = cmd_seq;
        memcpy(simpl_cmd->data, data, data_len);
    }
    return simpl_cmd;
}

struct CMPLX_CMD* createCmplxCmd(const char cmd[10], uint64_t cmd_seq, uint64_t param, char data[]) {
    size_t data_len = strlen(data) + 1; // + znak końca
    size_t cmd_size = sizeof(struct CMPLX_CMD) + data_len * sizeof(char);
    struct CMPLX_CMD* cmplx_cmd = (struct CMPLX_CMD*) malloc(cmd_size);

    if (!cmplx_cmd) {
        fatal("createCmplxCmd malloc failed!");
    } else {
        memcpy(cmplx_cmd->cmd, cmd, 10 * sizeof(char));
        cmplx_cmd->cmd_seq = cmd_seq;
        cmplx_cmd->param = param;
        memcpy(cmplx_cmd->data, data, data_len);
    }
    return cmplx_cmd;
}


struct SIMPL_CMD* helloCmd(uint64_t cmd_seq) {
    return createSimplCmd(Command[HELLO], cmd_seq, "");
}

struct SIMPL_CMD* listCmd(uint64_t cmd_seq, char substring[]) {
    return createSimplCmd(Command[LIST], cmd_seq, substring);
}

struct SIMPL_CMD* getCmd(uint64_t cmd_seq, char filename[]) {
    return createSimplCmd(Command[GET], cmd_seq, filename);
}

struct SIMPL_CMD* delCmd(uint64_t cmd_seq, char filename[]) {
    return createSimplCmd(Command[DEL], cmd_seq, filename);
}

struct CMPLX_CMD* addCmd(uint64_t cmd_seq, uint64_t file_size, char filename[]) {
    return createCmplxCmd(Command[ADD], cmd_seq, file_size, filename);
}

struct CMPLX_CMD* goodDayCmd(uint64_t cmd_seq, uint64_t free_space, char dotted_mcast_addr[]) {
    return createCmplxCmd(Command[GOOD_DAY], cmd_seq, free_space, dotted_mcast_addr);
}

struct SIMPL_CMD* myListCmd(uint64_t cmd_seq, char file_list[]) {
    return createSimplCmd(Command[MY_LIST], cmd_seq, file_list);
}

struct CMPLX_CMD* connectMeCmd(uint64_t cmd_seq, uint64_t tcp_port, char filename[]) {
    return createCmplxCmd(Command[CONNECT_ME], cmd_seq, tcp_port, filename);
}