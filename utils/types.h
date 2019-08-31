//
// Created by nikodem on 08.07.19.
//

#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

#define MAX_UDP_SIZE 60

typedef enum {
    HELLO,
    GOOD_DAY,
    LIST,
    MY_LIST,
    GET,
    CONNECT_ME,
    DEL,
    ADD,
    NO_WAY,
    CAN_ADD,
    LAST_ENTRY,

    UNKNOWN_CMD,
    NO_CMD
} CommandE;

const static char* Command[LAST_ENTRY] = {"HELLO", "GOOD_DAY", "LIST", "MY_LIST", "GET", "CONNECT_ME", "DEL", "ADD", "NO_WAY", "CAN_ADD"};

const static int isComplex[LAST_ENTRY] = {0, 1, 0, 0, 0, 1, 0, 1, 0, 1};

typedef enum {
    UDP,
    TCP
} Protocol;

struct SIMPL_CMD {
    char cmd[10];
    uint64_t cmd_seq;
    char data[];
} __attribute__((packed));

struct CMPLX_CMD {
    char cmd[10];
    uint64_t cmd_seq;
    uint64_t param;
    char data[];
} __attribute__((packed));

#endif //TYPES_H
