//
// Created by nikodem on 20.07.19.
//

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

#include "connection_utils.h"
#include "types.h"
#include "err.h"
#include "command_utils.h"
#include "user_input_output.h"
#include "file_utils.h"

#define MAX_TCP_LISTEN_QUEUE_LEN 1

int openSocket(Protocol protocol) {
    int sock;

    if (protocol == UDP) {
        sock = socket(AF_INET, SOCK_DGRAM, 0);
    } else if (protocol == TCP) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
    } else {
        syserr("new socket protocol");
        sock = -1;
    }

    if (sock < 0)
        syserr("socket");
    return sock;
}

void setBrodacastEnabled(int sock) {
    int optval = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void*)&optval, sizeof optval) < 0)
        syserr("setsockopt broadcast");
}

void setTTL(int sock) {
    int optval = TTL_VALUE;
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (void*)&optval, sizeof optval) < 0)
        syserr("setsockopt multicast ttl");
}

void setMulticastLoopDisabled(int sock) {
    int optval = 0;
    if (setsockopt(sock, SOL_IP, IP_MULTICAST_LOOP, (void*)&optval, sizeof optval) < 0)
        syserr("setsockopt loop");
}

void bindToAddress(int sock, struct sockaddr_in address) {
    if (bind(sock, (struct sockaddr *)&address, sizeof address) < 0)
        syserr("bind");
}

int connectToAddress(int sock, struct sockaddr_in address) {
    if (connect(sock, (struct sockaddr *)&address, sizeof address) < 0) {
        return 0;
    } else {
        return 1;
    }
}

void bindToLocalAddress(struct sockaddr_in* local_address, int sock, in_port_t port) {
    struct sockaddr_in local_addr;
    if (local_address == NULL) {
        local_address = &local_addr;
    }

    local_address->sin_family = AF_INET;
    local_address->sin_addr.s_addr = htonl(INADDR_ANY);
    local_address->sin_port = htons(port);
    bindToAddress(sock, *local_address);
}

void setPassiveOpenForTcp(int sock) {
    if (listen(sock, MAX_TCP_LISTEN_QUEUE_LEN) < 0)
        syserr("listen");
}

struct sockaddr_in getSockDetails(int sock) {
    struct sockaddr_in addr;
    socklen_t len_inet = (socklen_t) sizeof(addr);
    if (getsockname(sock, (struct sockaddr *)&addr, &len_inet) < 0){
        syserr("getsockname");
    }
    return addr;
}

struct addrinfo* getAddress(struct sockaddr_in* address, char host[], char port[], Protocol protocol) {
    struct addrinfo addr_hints;
    struct addrinfo* addr_result;
    in_port_t server_port = (in_port_t) atoi(port);

    // 'converting' host/port in string to struct addrinfo
    (void) memset(&addr_hints, 0, sizeof(struct addrinfo));
    addr_hints.ai_family = AF_INET;
    addr_hints.ai_protocol = 0;
    addr_hints.ai_flags = 0;
    addr_hints.ai_addrlen = 0;
    addr_hints.ai_addr = NULL;
    addr_hints.ai_canonname = NULL;
    addr_hints.ai_next = NULL;

    if (protocol == UDP) {
        addr_hints.ai_socktype = SOCK_DGRAM;
    } else if (protocol == TCP){
        addr_hints.ai_socktype = SOCK_STREAM;
    } else {
        syserr("new address protocol");
        addr_hints.ai_socktype = -1;
    }

    int err = getaddrinfo(host, port, &addr_hints, &addr_result);

    if (protocol == TCP && err == EAI_SYSTEM) {
        syserr("getaddrinfo: %s", gai_strerror(err));
    }

    if (err != 0) {
        syserr("getaddrinfo");
    }

    address->sin_family = AF_INET; // IPv4
    address->sin_addr.s_addr = ((struct sockaddr_in*) (addr_result->ai_addr))->sin_addr.s_addr; // address IP
    address->sin_port = htons(server_port);

    return addr_result;
}

void setMulticastEnabled(int sock, struct ip_mreq *ip_mreq, char *multicast_dotted_address) {
    ip_mreq->imr_multiaddr.s_addr = inet_addr(multicast_dotted_address);
    ip_mreq->imr_interface.s_addr = htonl(INADDR_ANY); // Wydaje mi się, że to może nie zadziałać dla kilku interfejsów
   // if (inet_aton(multicast_dotted_address, &(ip_mreq->imr_multiaddr)) == 0)
   //     syserr("inet_aton");
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) ip_mreq, sizeof (*ip_mreq)) < 0)
        syserr("setsockopt connect to multicast");
}

void setMulticastDisabled(int sock, struct ip_mreq *ip_mreq) {
    if (setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void *) &ip_mreq, sizeof ip_mreq) < 0)
        syserr("setsockopt disconnect from multicast");
}

void setReceiveTimeout(int sock, struct timeval timeout) {
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,&timeout,sizeof(timeout)) < 0) {
        perror("Error");
    }
}

void setReceiveTimeoutZero(int sock) {
    struct timeval zero_time;
    zero_time.tv_sec = 0;
    zero_time.tv_usec = 0;
    setReceiveTimeout(sock, zero_time);
}

void sendSimplCmd(int sock, struct SIMPL_CMD* cmd, struct sockaddr_in* addr) {

    debugLog("WYSYŁAM: {\n");
    printSimplCmd(cmd);
    if (addr != NULL) debugLog("DO: %s:%d\n", inet_ntoa(addr->sin_addr), addr->sin_port);
    debugLog("}\n");

    cmd->cmd_seq = htobe64(cmd->cmd_seq);
    size_t cmd_size = sizeof(struct SIMPL_CMD) + (strlen(cmd->data) + 1) * sizeof(char);

    if (addr == NULL) { // DEPRECATED
        write(sock, (char *) cmd, cmd_size);
    } else {
        socklen_t snda_len = (socklen_t) sizeof(*addr);

        int flags = 0;
        ssize_t snd_len = sendto(sock, (char *) cmd, cmd_size, flags, (struct sockaddr *) addr, snda_len);
        debugLog("WYSLANO: %ld BYTOW\n", snd_len);
        if (snd_len != cmd_size)
            syserr("error on sending simpl cmd to client socket");
    }
}

void sendCmplxCmd(int sock, struct CMPLX_CMD* cmd, struct sockaddr_in* addr) {

    debugLog("WYSYŁAM: {\n");
    printCmplxCmd(cmd);
    if (addr != NULL) debugLog("DO: %s:%d\n", inet_ntoa(addr->sin_addr), addr->sin_port);
    debugLog("}\n");

    cmd->cmd_seq = htobe64(cmd->cmd_seq);
    cmd->param = htobe64(cmd->param);
    size_t cmd_size = sizeof(struct CMPLX_CMD) + (strlen(cmd->data) + 1) * sizeof(char);

    if (addr == NULL) { // DEPRECATED
        write(sock, (char *) cmd, cmd_size);
    } else {
        socklen_t snda_len = (socklen_t) sizeof(*addr);

        int flags = 0;
        ssize_t snd_len = sendto(sock, (char *) cmd, cmd_size, flags, (struct sockaddr *) addr, snda_len);
        debugLog("WYSLANO: %ld BYTOW\n", snd_len);
        if (snd_len != cmd_size)
            syserr("error on sending cmplx cmd to client socket");
    }
}

CommandE readCommand(int sock, struct sockaddr_in* sender_address, struct SIMPL_CMD** simpl_cmd, struct CMPLX_CMD** cmplx_cmd) {
    char buffer[MAX_UDP_SIZE];

    struct sockaddr_in temp_address;
    if (sender_address == NULL) {
        sender_address = &temp_address;
    }

    socklen_t rcva_len = (socklen_t) sizeof(sender_address);
    int flags = 0;
    ssize_t len = recvfrom(sock, buffer, MAX_UDP_SIZE, flags,
                           (struct sockaddr *) sender_address, &rcva_len);

    if (len < 0) {
        //printf("%d\n", errno);
        //syserr("recvfrom");
        return NO_CMD;
    } else {
        CommandE cmd = getCommand(buffer);
        if (cmd != UNKNOWN_CMD) {
            if(isComplex[cmd] && len >= sizeof(struct CMPLX_CMD)) {
                (*cmplx_cmd) = malloc((size_t) len /*+ 1*/);
                memcpy(*cmplx_cmd, buffer, (size_t) len);
                (*cmplx_cmd)->cmd_seq = be64toh((*cmplx_cmd)->cmd_seq);
                (*cmplx_cmd)->param = be64toh((*cmplx_cmd)->param);
                //*(((char*)cmplx_cmd) + len) = '\0';
            } else if (!isComplex[cmd] && len >= sizeof(struct SIMPL_CMD)){
                (*simpl_cmd) = malloc((size_t) len /*+ 1*/);
                memcpy(*simpl_cmd, buffer, (size_t) len);
                (*simpl_cmd)->cmd_seq = be64toh((*simpl_cmd)->cmd_seq);
                //*(((char*)simpl_cmd) + len) = '\0';
            } else { // Jeżeli dostaliśmy za mało bajtów
                return UNKNOWN_CMD;
            }
        }
        return cmd;
    }
}

int sendFile(int socket, char* path, char* filename) {
    FILE* file = getFile(path, filename, "r");
    if (file == NULL) {
        return 0;
    }
    char buffer[FILE_PART_SIZE];
    size_t len;
    while (0 < (len = fread(buffer, sizeof(char), FILE_PART_SIZE, file))) {
        ssize_t snd_len = write(socket, buffer, len);
        if (snd_len != len) {
            return 0;
        }
    }
    fclose(file);
    return 1;
}

int receiveFile(int socket, char* path, char* filename) {
    FILE* file = getFile(path, filename, "w");
    if (file == NULL) {
        return 0;
    }

    char buffer[FILE_PART_SIZE];

    ssize_t len = 0;
    while (0 < (len = read(socket, buffer, FILE_PART_SIZE))) {
        fwrite(buffer, sizeof(char), (size_t) len, file);
    }
    fclose(file);

    if (len == -1) {
        return 0;
    } else {
        return 1;
    }
}

struct HostList initHostList() {
    struct HostList list;
    list.list = NULL;
    return list;
}

int isHostListEmpty(struct HostList* list) {
    return list->list == NULL;
}

void addHost(struct HostList* list, struct sockaddr_in addr, uint64_t size) {
    struct HostNode* iter = list->list;
    struct HostNode* last = NULL;

    struct HostNode* new_node = malloc(sizeof(struct HostNode));
    new_node->free_space = size;
    new_node->host = addr;

    while (iter != NULL && iter->free_space > new_node->free_space) {
        last = iter;
        iter = iter->next;
    }
    if (last != NULL) {
        last->next = new_node;
    } else {
        list->list = new_node;
    }
    new_node->next = iter;
}

struct sockaddr_in getHost(struct HostList* list, uint64_t* free_space) {
    struct sockaddr_in addr;
    if (!isHostListEmpty(list)) {
        addr = list->list->host;
        struct HostNode* old_node = list->list;
        list->list = old_node->next;
        *free_space = old_node->free_space;
        free(old_node);
    }
    return addr;
}