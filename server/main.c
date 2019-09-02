#include <stdio.h>
#include <zconf.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>

#include "../utils/err.h"
#include "../utils/types.h"
#include "../utils/connection_utils.h"
#include "../utils/args_utils.h"
#include "../utils/command_utils.h"
#include "../utils/user_input_output.h"
#include "../utils/file_utils.h"

static volatile int stop = 0;
static jmp_buf buf;

// Oczekujemy na zakończenie obsługi ostatniego klienta
void gracefulStop(int sig) {
    stop = 1;
}

// Uciekamy z głównej pętli
void forceStop(int sig) {
    stop = 1;
    longjmp(buf, 0);
}

void handleHello(uint64_t cmd_seq, int mcast_sock, struct sockaddr_in* addr, char* mcast_addr, int64_t free_space);

void handleList(uint64_t cmd_seq, int mcast_sock, struct sockaddr_in* addr, struct FileList* list, char* substring);

void handleDel(struct FileList* list, char* filename, char* dir_path);

void handleGet(uint64_t cmd_seq, int mcast_sock, struct sockaddr_in* addr, struct FileList* list, char* filename, char* filepath, struct timeval timeout);

void handleAdd(uint64_t cmd_seq, int mcast_sock, struct sockaddr_in* addr, struct FileList* list, char* filename, uint64_t file_size, char* filepath, struct timeval timeout, int64_t max_size);

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
    char* TMP_SHRD_FLDR = findArg("-f", argc, argv);
    char* TIMEOUT_STR = findArg("-t", argc, argv);

    if (MCAST_ADDR == NULL || CMD_PORT_STR == NULL || TMP_SHRD_FLDR == NULL) {
        fatal("Usage: %s -g MCAST_ADDR -p CMD_PORT [-b MAX_SPACE] -f SHRD_FLDR [-t TIMEOUT] \n", argv[0]);
    }

    char SHRD_FLDR[strlen(TMP_SHRD_FLDR) + 2];
    strcpy(SHRD_FLDR, TMP_SHRD_FLDR);
    strcat(SHRD_FLDR, "/"); // na wypadek gdy scieżka do katalogu nie zawiera '/'

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

    // Zapisujemy sobie miejsce do którego chcemy skoczyć na koniec działania aplikacji
    setjmp(buf);

    /* czytanie tego, co odebrano */
    while (!stop) {
        struct sockaddr_in client_address;
        struct SIMPL_CMD* simple_cmd;
        struct CMPLX_CMD* cmplx_cmd;

        debugLog("OCZEKUJE NA KOMENDE...\n");

        CommandE cmd = readCommand(mcast_sock, &client_address, &simple_cmd, &cmplx_cmd);
        signal(SIGINT, gracefulStop);

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
                handleGet(simple_cmd->cmd_seq, mcast_sock, &client_address, &file_list, simple_cmd->data, SHRD_FLDR, TIMEOUT);
                free(simple_cmd);
                break;
            case DEL:
                handleDel(&file_list, simple_cmd->data, SHRD_FLDR);
                free(simple_cmd);
                break;
             case ADD:
                handleAdd(cmplx_cmd->cmd_seq, mcast_sock, &client_address, &file_list, cmplx_cmd->data, cmplx_cmd->param, SHRD_FLDR, TIMEOUT, MAX_SPACE);
                free(cmplx_cmd);
                break;
            default:
                if (cmd != NO_CMD) {
                    printCmdError(client_address, "Got unknown command");
                    if (cmd != UNKNOWN_CMD) {
                        free(isComplex[cmd] ? (void *) cmplx_cmd : (void *) simple_cmd);
                    }
                }
                break;
        }
        signal(SIGINT, forceStop);
    }

    debugLog("KOŃCZĘ SERWER.\n");
    
    purgeFileList(&file_list);
    setMulticastDisabled(mcast_sock, &mcast_ip_mreq);
}

int initFileListen(struct timeval timeout) {
    int tcp_sock = openSocket(TCP);
    bindToLocalAddress(NULL, tcp_sock, 0);
    setPassiveOpenForTcp(tcp_sock);
    setReceiveTimeout(tcp_sock, timeout);
    return tcp_sock;
}

int isFileGood(char* filename, uint64_t size, struct FileList* files, int64_t max_space) {
    if (size > (max_space-files->size)) {
        return 0;
    }
    if (strlen(filename) <= 0) {
        return 0;
    }
    if (strchr(filename, '/')) {
        return 0;
    }
    if (findFile(files, filename)) {
        return 0;
    }
    return 1;
}

void handleAdd(uint64_t cmd_seq, int mcast_sock, struct sockaddr_in* addr, struct FileList* list, char* filename, uint64_t file_size, char* filepath, struct timeval timeout, int64_t max_size) {
    if (isFileGood(filename, file_size, list, max_size)) {
        addFile(list, filename, file_size, NULL);

        if (fork() == 0) { // Proces potomny

            debugLog("[TCP] ROZPOCZYNAM PROCES POTOMNY:\n");

            int tcp_sock = initFileListen(timeout);
            struct sockaddr_in local_address = getSockDetails(tcp_sock);

            struct CMPLX_CMD* cmd = canAddCmd(cmd_seq, (uint64_t) local_address.sin_port, "");
            sendCmplxCmd(mcast_sock, cmd, addr);
            free(cmd);

            debugLog("[TCP] OCZEKUJE NA POLACZENIE OD KLIENTA NA PORCIE: %d\n", local_address.sin_port);

            struct sockaddr_in client_address;
            socklen_t client_address_len = sizeof(client_address);
            int con_fd = accept(tcp_sock, (struct sockaddr *) &client_address, &client_address_len);

            if (con_fd < 0) {
                debugLog("[TCP] NIE UDAŁO SIĘ NAWIĄZAĆ POŁĄCZENIA Z KLIENTEM.\n");
            } else {
                debugLog("[TCP] UDAŁO SIĘ NAWIĄZAĆ POŁĄCZENIE.\n");
                debugLog("[TCP] OCZEKUJE NA POBRANIE PLIKU.\n");

                receiveFile(con_fd, filepath, filename);

                debugLog("[TCP] POBIERANIE ZAKOŃCZONE, KOŃCZĘ POŁĄCZENIE.\n");

                if (close(con_fd) < 0)
                    syserr("close");
            }

            debugLog("[TCP] KOŃCZE PROCES POTOMNY.\n");

            if (close(tcp_sock) < 0)
                syserr("close");
            exit(0);
        }

    } else {
        struct SIMPL_CMD* cmd = noWayCmd(cmd_seq, filename);
        sendSimplCmd(mcast_sock, cmd, addr);
        free(cmd);
    }
}

void handleGet(uint64_t cmd_seq, int mcast_sock, struct sockaddr_in* addr, struct FileList* list, char* filename, char* filepath, struct timeval timeout) {
    if (findFile(list, filename)) {

        if (fork() == 0) { // Proces potomny

            debugLog("[TCP] ROZPOCZYNAM PROCES POTOMNY:\n");

            int tcp_sock = initFileListen(timeout);
            struct sockaddr_in local_address = getSockDetails(tcp_sock);

            struct CMPLX_CMD* cmd = connectMeCmd(cmd_seq, (uint64_t) local_address.sin_port, filename);
            sendCmplxCmd(mcast_sock, cmd, addr);
            free(cmd);

            debugLog("[TCP] OCZEKUJE NA POLACZENIE OD KLIENTA NA PORCIE: %d\n", local_address.sin_port);

            struct sockaddr_in client_address;
            socklen_t client_address_len = sizeof(client_address);
            int con_fd = accept(tcp_sock, (struct sockaddr *) &client_address, &client_address_len);

            if (con_fd < 0) {
                debugLog("[TCP] NIE UDAŁO SIĘ NAWIĄZAĆ POŁĄCZENIA Z KLIENTEM.\n");
            } else {
                debugLog("[TCP] UDAŁO SIĘ NAWIĄZAĆ POŁĄCZENIE.\n");
                debugLog("[TCP] ROZPOCZYNAM WYSYŁANIE PLIKU.\n");

                if (sendFile(con_fd, filepath, filename)) {
                    debugLog("[TCP] WYSYŁANIE ZAKOŃCZONE SUKCESEM, KOŃCZĘ POŁĄCZENIE.\n");
                } else {
                    debugLog("[TCP] WYSYŁANIE ZAKOŃCZONE BŁĘDEM, KOŃCZĘ POŁĄCZENIE.\n");
                }

                if (close(con_fd) < 0)
                    syserr("close");
            }

            debugLog("[TCP] KOŃCZE PROCES POTOMNY.\n");

            if (close(tcp_sock) < 0)
                syserr("close");
            exit(0);
        }

    } else {
        printCmdError(*addr, "Client requested file: %s", filename);
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