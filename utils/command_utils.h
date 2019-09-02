//
// Created by nikodem on 26.07.19.
//

#ifndef COMMAND_UTILS_H
#define COMMAND_UTILS_H

#include <string.h>
#include <zconf.h>
#include <stdio.h>
#include <stdlib.h>

#include "types.h"
#include "err.h"

extern CommandE getCommand(char* string);

extern struct SIMPL_CMD* createSimplCmd(const char cmd[10], uint64_t cmd_seq, char data[]);

extern struct CMPLX_CMD* createCmplxCmd(const char cmd[10], uint64_t cmd_seq, uint64_t param, char data[]);

// KOMENDY KLIENTA

extern struct SIMPL_CMD* helloCmd(uint64_t cmd_seq);

extern struct SIMPL_CMD* listCmd(uint64_t cmd_seq, char substring[]);

extern struct SIMPL_CMD* getCmd(uint64_t cmd_seq, char filename[]);

extern struct SIMPL_CMD* delCmd(uint64_t cmd_seq, char filename[]);

extern struct CMPLX_CMD* addCmd(uint64_t cmd_seq, uint64_t file_size, char filename[]);

// KOMENDY SERWERA

extern struct CMPLX_CMD* goodDayCmd(uint64_t cmd_seq, uint64_t free_space, char dotted_mcast_addr[]);

extern struct SIMPL_CMD* myListCmd(uint64_t cmd_seq, char file_list[]);

extern struct CMPLX_CMD* connectMeCmd(uint64_t cmd_seq, uint64_t tcp_port, char filename[]);

extern struct SIMPL_CMD* noWayCmd(uint64_t cmd_seq, char filename[]);

extern struct CMPLX_CMD* canAddCmd(uint64_t cmd_seq, uint64_t tcp_port, char filename[]);

#endif //COMMAND_UTILS_H
