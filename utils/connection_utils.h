//
// Created by nikodem on 20.07.19.
//

#ifndef CONNECTION_UTILS_H
#define CONNECTION_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>

#include "types.h"
#include "err.h"
#include "command_utils.h"
#include "user_input_output.h"
#include "file_utils.h"

#define TTL_VALUE     4

struct HostNode {
    struct HostNode* next;
    struct sockaddr_in host;
    uint64_t free_space;
};

struct HostList {
    struct HostNode* list;
};

/* otworzenie gniazda */
extern int openSocket(Protocol a_protocol);

/* uaktywnienie rozgłaszania */
extern void setBrodacastEnabled(int sock);

/* ustawienie Time To Live dla datagramów rozsyłanych do grupy */
extern void setTTL(int sock);

/* zablokowanie rozsyłania grupowego do siebie */
extern void setMulticastLoopDisabled(int sock);

/* podpiecie socketu pod konkretny adres */
extern void bindToAddress(int sock, struct sockaddr_in address);

/* podpięcie się pod lokalny adres i port socket'u */
extern void bindToLocalAddress(struct sockaddr_in* local_address, int sock, in_port_t port);

/* przejście w pasywne oczekiwanie na połączenia */
extern void setPassiveOpenForTcp(int sock);

/* połącznie socketu z adresem, w domyśle z zamiarem pisania */
extern int connectToAddress(int sock, struct sockaddr_in address);

extern struct sockaddr_in getSockDetails(int sock);

extern struct addrinfo* getAddress(struct sockaddr_in* address, char host[], char port[], Protocol protocol);

/* podpięcie się do grupy rozsyłania */
extern void setMulticastEnabled(int sock, struct ip_mreq *ip_mreq, char *multicast_dotted_address);

/* w taki sposób można odpiąć się od grupy rozsyłania */
extern void setMulticastDisabled(int sock, struct ip_mreq *ip_mreq);

extern void setReceiveTimeout(int sock, struct timeval timeout);

extern void sendSimplCmd(int sock, struct SIMPL_CMD* cmd, struct sockaddr_in* addr);

extern void sendCmplxCmd(int sock, struct CMPLX_CMD* cmd, struct sockaddr_in* addr);

extern CommandE readCommand(int sock, struct sockaddr_in* sender_address, struct SIMPL_CMD** simpl_cmd, struct CMPLX_CMD** cmplx_cmd);

extern void setReceiveTimeoutZero(int sock);

extern int sendFile(int socket, char* path, char* filename);

extern int receiveFile(int socket, char* path, char* filename);

extern struct HostList initHostList();

extern int isHostListEmpty(struct HostList* list);

extern void addHost(struct HostList* list, struct sockaddr_in addr, uint64_t size);

extern struct sockaddr_in getHost(struct HostList* list, uint64_t* free_space);

#endif //CONNECTION_UTILS_H
