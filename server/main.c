#include <stdio.h>
#include <zconf.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include "../utils/err.h"
#include "../utils/types.h"
#include "../utils/connection_utils.h"
#include "../utils/args_utils.h"
#include "../utils/command_utils.h"
#include "../utils/user_input_output.h"
#include "../utils/file_utils.h"

void handleHello(uint64_t cmd_seq, int mcast_sock, struct sockaddr_in* addr, char* mcast_addr, int64_t free_space);

void handleList(uint64_t cmd_seq, int mcast_sock, struct sockaddr_in* addr, struct FileList* list, char* substring);

void handleDel(struct FileList* list, char* filename, char* dir_path);

void handleGet(uint64_t cmd_seq, int mcast_sock, struct sockaddr_in* addr, struct FileList* list, char* filename);

int main(int argc, char *argv[]) {
    /*
     * MCAST_ADDR – adres rozgłaszania ukierunkowanego, ustawiany obowiązkowym parametrem -g węzła serwerowego;
     * CMD_PORT – port UDP używany do przesyłania i odbierania poleceń, ustawiany obowiązkowym parametrem -p węzła serwerowego;
     * MAX_SPACE – maksymalna liczba bajtów udostępnianej przestrzeni dyskowej na pliki grupy przez ten węzeł serwerowy, ustawiana opcjonalnym parametrem -b węzła serwerowego, wartość domyślna 52428800;
     * SHRD_FLDR – ścieżka do dedykowanego folderu dyskowego, gdzie mają być przechowywane pliki, ustawiany parametrem obowiązkowym -f węzła serwerowego;
     * TIMEOUT – liczba sekund, jakie serwer może maksymalnie oczekiwać na połączenia od klientów, ustawiane opcjonalnym parametrem -t węzła serwerowego, wartość domyślna 5, wartość maksymalna 300.
     */

    char* MCAST_ADDR = findArg("-g", argc, argv);
    char* CMD_PORT_STR = findArg("-p", argc, argv);
    char* MAX_SPACE_STR = findArg("-b", argc, argv);
    char* SHRD_FLDR = findArg("-f", argc, argv);
    char* TIMEOUT_STR = findArg("-t", argc, argv);

    if (MCAST_ADDR == NULL || CMD_PORT_STR == NULL || SHRD_FLDR == NULL) {
        fatal("Usage: %s -g MCAST_ADDR -p CMD_PORT [-b MAX_SPACE] -f SHRD_FLDR [-t TIMEOUT] \n", argv[0]);
    }

    in_port_t CMD_PORT = (in_port_t) atoi(CMD_PORT_STR);
    int64_t MAX_SPACE = 52428800;
    struct timeval TIMEOUT;

    TIMEOUT.tv_sec =  5;
    TIMEOUT.tv_usec = 0;

    if (MAX_SPACE_STR != NULL) {
        MAX_SPACE = atol(MAX_SPACE_STR);
    }

    if (TIMEOUT_STR != NULL) {
        int temp = atoi(TIMEOUT_STR);
        if (temp > 0) {
            TIMEOUT.tv_sec = temp > 300 ? 300 : temp;
            TIMEOUT.tv_usec = 0;
        }
    }


    int mcast_sock = openSocket(UDP);
    struct ip_mreq mcast_ip_mreq;

    setMulticastEnabled(mcast_sock, &mcast_ip_mreq, MCAST_ADDR);
    bindToLocalAddress(NULL, mcast_sock, CMD_PORT);

    struct FileList file_list = initFileList();
    loadFilesFromDir(&file_list, SHRD_FLDR);

    /* czytanie tego, co odebrano */
    for (;;) {

        struct sockaddr_in client_address;
        struct SIMPL_CMD* simple_cmd;
        struct CMPLX_CMD* cmplx_cmd;
        CommandE cmd = readCommand(mcast_sock, &client_address, &simple_cmd, &cmplx_cmd);

        debugLog("ODEBRANO: {\n");
        isComplex[cmd] ? printCmplxCmd(cmplx_cmd) : printSimplCmd(simple_cmd);
        debugLog("}\n");

         switch (cmd) {
            case HELLO:
                handleHello(simple_cmd->cmd_seq, mcast_sock, &client_address, MCAST_ADDR, MAX_SPACE - file_list.size);
                free(simple_cmd);
                break;
            case LIST:
                handleList(simple_cmd->cmd_seq, mcast_sock, &client_address, &file_list, simple_cmd->data);
                free(simple_cmd);
                break;
            case GET:
                handleGet(simple_cmd->cmd_seq, mcast_sock, &client_address, &file_list, simple_cmd->data);
                free(simple_cmd);
                break;
            case DEL:
                handleDel(&file_list, simple_cmd->data, SHRD_FLDR);
                free(simple_cmd);
                break;
             case ADD:
                printf("ADD\n");
                free(cmplx_cmd);
                break;
            default:
                if (cmd != NO_CMD) {
                    printCmdError(client_address);
                    if (cmd != UNKNOWN_CMD) {
                        free(isComplex[cmd] ? (void *) cmplx_cmd : (void *) simple_cmd);
                    }
                }
                break;
        }
    }

    purgeFileList(&file_list);
    //setMulticastDisabled(mcast_sock, &mcast_ip_mreq);

}

void handleGet(uint64_t cmd_seq, int mcast_sock, struct sockaddr_in* addr, struct FileList* list, char* filename) {
    if (findFile(list, filename)) {





    } else {
        printCmdError(*addr);
    }
}

void handleDel(struct FileList* list, char* filename, char* dir_path) {
    char full_path[strlen(dir_path) + strlen(filename) + 1];
    strcpy(full_path, dir_path);
    strcat(full_path, filename);

    if (remove(full_path) == 0) {
        removeFile(list, findFile(list, filename));
    }
}

void handleHello(uint64_t cmd_seq, int mcast_sock, struct sockaddr_in* addr, char* mcast_addr, int64_t free_space) {
    uint64_t real_free_space = free_space > 0 ? (uint64_t) free_space : 0;
    struct CMPLX_CMD* cmd = goodDayCmd(cmd_seq, real_free_space, mcast_addr);
    sendCmplxCmd(mcast_sock, cmd, addr);
    free(cmd);
}

void handleList(uint64_t cmd_seq, int mcast_sock, struct sockaddr_in* addr, struct FileList* list, char* substring) {
    struct FileList list_iter = *list;

    while (list_iter.list != NULL) {
        char* files = castFileListToString(&list_iter, MAX_UDP_SIZE - sizeof(struct SIMPL_CMD), substring);
        if (files != NULL) {
            struct SIMPL_CMD *cmd = myListCmd(cmd_seq, files);
            sendSimplCmd(mcast_sock, cmd, addr);
            free(cmd);
            free(files);
        }
    }
}