#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "../utils/err.h"
#include "../utils/types.h"
#include "../utils/connection_utils.h"
#include "../utils/args_utils.h"
#include "../utils/user_input_output.h"
#include "../utils/command_utils.h"
#include "../utils/file_utils.h"


struct ConnectionData {
    uint64_t cmd_seq;
    int mcast_sock;
    struct sockaddr_in* addr;
    struct timeval timeout;
};

void handleDiscover(struct ConnectionData* connection_data);

void handleSearch(struct ConnectionData* connection_data, char* substring, struct FileList* list);

void handleRemove(struct ConnectionData* connection_data, char* filename);

void handleFetch(struct ConnectionData* connection_data, char* filepath, char* filename, struct FileList* list);

void handleUpload(struct ConnectionData* connection_data, char* filepath);

int main(int argc, char *argv[]) {
    /*
     * MCAST_ADDR – adres rozgłaszania ukierunkowanego (może być także adresem broadcast), ustawiany obowiązkowym parametrem -g; klient powinien używać tego adresu do wysyłania komunikatów do grupy węzłów serwerowych;
     * CMD_PORT – port UDP, na którym nasłuchują węzły serwerowe, ustawiany obowiązkowym parametrem -p; klient powinien używać tego numeru portu, aby wysyłać komunikaty do węzłów serwerowych;
     * OUT_FLDR – ścieżka do folderu dyskowego, gdzie klient będzie zapisywał pobrane pliki, ustawiany parametrem obowiązkowym -o;
     * TIMEOUT – czas oczekiwania na zbieranie informacji od węzłów wyrażony w sekundach; akceptowana wartość powinna być dodatnia i większa od zera; wartość domyślna 5; wartość maksymalna 300; może zostać zmieniona przez opcjonalny parametr -t.
     */

    char* MCAST_ADDR = findArg("-g", argc, argv);
    char* CMD_PORT_STR = findArg("-p", argc, argv);
    char* TMP_OUT_FLDR = findArg("-o", argc, argv);
    char* TIMEOUT_STR = findArg("-t", argc, argv);

    if (MCAST_ADDR == NULL || CMD_PORT_STR == NULL || TMP_OUT_FLDR == NULL) {
        fatal("Usage: %s -g MCAST_ADDR -p CMD_PORT -o OUT_FLDR [-t TIMEOUT] \n", argv[0]);
    }

    char OUT_FLDR[strlen(TMP_OUT_FLDR) + 2];
    strcpy(OUT_FLDR, TMP_OUT_FLDR);
    strcat(OUT_FLDR, "/"); // na wypadek gdy scieżka do katalogu nie zawiera '/'

    in_port_t CMD_PORT = (in_port_t) atoi(CMD_PORT_STR);
    int MAX_SPACE = 52428800; // 16-bitowych intów chyba już się nie spotyka
    struct timeval TIMEOUT;

    TIMEOUT.tv_sec =  5;
    TIMEOUT.tv_usec = 0;

    if (TIMEOUT_STR != NULL) {
        int temp = atoi(TIMEOUT_STR);
        if (temp > 0 ) {
            TIMEOUT.tv_sec = temp > 300 ? 300 : temp;
            TIMEOUT.tv_usec = 0;
        }
    }

    // === SOCKET SETUP === //

    int mcast_sock;
    struct sockaddr_in mcast_sockadd_in, local_addr;

    mcast_sock = openSocket(UDP);
    setBrodacastEnabled(mcast_sock);
    setTTL(mcast_sock);
    bindToLocalAddress(&local_addr, mcast_sock, 0);
    freeaddrinfo(getAddress(&mcast_sockadd_in, MCAST_ADDR, CMD_PORT_STR, UDP));
    //setMulticastLoopDisabled(mcast_sock);

    // ==== MAIN LOOP ==== //

    struct FileList available_files = initFileList();
    struct ConnectionData connection_data;

    connection_data.mcast_sock = mcast_sock;
    connection_data.timeout = TIMEOUT;
    connection_data.addr = &mcast_sockadd_in;
    connection_data.cmd_seq = 1;

    int stop = 0;

    while (!stop) {
        debugLog("CZEKAM NA KOMENDE:\n");
        struct UserInput input = getUserInput();

        switch (input.action) {
            case DISCOVER:
                handleDiscover(&connection_data);
                break;
            case SEARCH:
                handleSearch(&connection_data, input.arg, &available_files);
                break;
            case FETCH:
                handleFetch(&connection_data, OUT_FLDR, input.arg, &available_files);
                break;
            case UPLOAD:
                handleUpload(&connection_data, input.arg);
                break;
            case REMOVE:
                handleRemove(&connection_data, input.arg);
                break;
            case EXIT:
                stop = 1;
                break;
            default:
                // Ignorujemy nieznaną komendę
                break;
        }

        connection_data.cmd_seq++;
        free(input.full_command);
    }

    if (!isFileListEmpty(&available_files)) {
        purgeFileList(&available_files);
    }
    close(mcast_sock);
    return 0;
}


void executeFileSend(struct sockaddr_in server_addr, struct CMPLX_CMD* cmplx_cmd, char* filepath) {
    int tcp_sock = openSocket(TCP);

    server_addr.sin_port = (in_port_t) cmplx_cmd->param;
    if (!connectToAddress(tcp_sock, server_addr)) {
        printf("File %s uploading failed (%s:%lu) Server unavailable\n", filepath, inet_ntoa(server_addr.sin_addr), cmplx_cmd->param);
    } else if (!sendFile(tcp_sock, "", filepath)) {
        printf("File %s uploading failed (%s:%lu) Connection dropped\n", filepath, inet_ntoa(server_addr.sin_addr), cmplx_cmd->param);
    } else {
        printf("File {%s} uploaded (%s:%lu)\n", filepath, inet_ntoa(server_addr.sin_addr), cmplx_cmd->param);
    }
    close(tcp_sock);
}

void handleUpload(struct ConnectionData* connection_data, char* filepath) {
    if (isFile(filepath)) {
        struct stat statbuf;
        stat(filepath, &statbuf);
        uint64_t file_size = (uint64_t) statbuf.st_size;

        struct SIMPL_CMD* cmd = helloCmd(connection_data->cmd_seq);
        sendSimplCmd(connection_data->mcast_sock, cmd, connection_data->addr);
        free(cmd);

        struct SIMPL_CMD* simpl_cmd;
        struct CMPLX_CMD* cmplx_cmd;

        struct sockaddr_in server_addr;

        struct timeval start_time, loop_time;
        struct timeval timeout_diff;
        struct timeval timeout_left = connection_data->timeout;

        int64_t best_server_free_space = 0;
        struct HostList host_list = initHostList();

        gettimeofday(&start_time, NULL);

        while (timeout_left.tv_sec > 0 || (timeout_left.tv_sec == 0 && timeout_left.tv_usec > 0)) {
            setReceiveTimeout(connection_data->mcast_sock, timeout_left);
            CommandE e = readCommand(connection_data->mcast_sock, &server_addr, &simpl_cmd, &cmplx_cmd);

            if (e != NO_CMD) {
                isComplex[e] ? printCmplxCmd(cmplx_cmd) : printSimplCmd(simpl_cmd);

                if (e == GOOD_DAY && cmplx_cmd->cmd_seq == connection_data->cmd_seq) {
                    if (cmplx_cmd->param > best_server_free_space) {
                        best_server_free_space = cmplx_cmd->param;
                    }
                    addHost(&host_list, server_addr, cmplx_cmd->param);
                } else {
                    printCmdError(server_addr, "Got unexpected command: %s", Command[e]);
                }
                free(isComplex[e] ? (void *) cmplx_cmd : (void *) simpl_cmd);
            }

            gettimeofday(&loop_time, NULL);
            timersub(&loop_time, &start_time, &timeout_diff);
            timersub(&connection_data->timeout, &timeout_diff, &timeout_left);
        }
        setReceiveTimeoutZero(connection_data->mcast_sock);

        if (best_server_free_space >= file_size) {
            while (!isHostListEmpty(&host_list)) {

                uint64_t server_free_space;
                struct sockaddr_in best_server_addr = getHost(&host_list, &server_free_space);

                if (server_free_space >= file_size) {
                    connection_data->cmd_seq++;

                    timeout_left = connection_data->timeout;
                    gettimeofday(&start_time, NULL);

                    struct CMPLX_CMD *add_cmd = addCmd(connection_data->cmd_seq, file_size,
                                                       getFileNameFromPath(filepath));
                    sendCmplxCmd(connection_data->mcast_sock, add_cmd, &best_server_addr);
                    free(add_cmd);

                    while (timeout_left.tv_sec > 0 || (timeout_left.tv_sec == 0 && timeout_left.tv_usec > 0)) {
                        setReceiveTimeout(connection_data->mcast_sock, timeout_left);
                        CommandE e = readCommand(connection_data->mcast_sock, &server_addr, &simpl_cmd, &cmplx_cmd);

                        if (e != NO_CMD) {
                            isComplex[e] ? printCmplxCmd(cmplx_cmd) : printSimplCmd(simpl_cmd);

                            if (e == CAN_ADD && cmplx_cmd->cmd_seq == connection_data->cmd_seq) {
                                if (fork() == 0) {
                                    executeFileSend(server_addr, cmplx_cmd, filepath);
                                    free(isComplex[e] ? (void *) cmplx_cmd : (void *) simpl_cmd);
                                    exit(0);
                                } else {
                                    break;
                                }
                            } else if (e == NO_WAY && simpl_cmd->cmd_seq == connection_data->cmd_seq) {
                                printf("File %s uploading failed (%s:%hu) Server rejected a file\n", filepath,
                                       inet_ntoa(server_addr.sin_addr), server_addr.sin_port);
                                free(isComplex[e] ? (void *) cmplx_cmd : (void *) simpl_cmd);
                                break;
                            } else {
                                printCmdError(server_addr, "Got unexpected command: %s", Command[e]);
                            }
                            free(isComplex[e] ? (void *) cmplx_cmd : (void *) simpl_cmd);
                        }

                        gettimeofday(&loop_time, NULL);
                        timersub(&loop_time, &start_time, &timeout_diff);
                        timersub(&connection_data->timeout, &timeout_diff, &timeout_left);
                    }
                    setReceiveTimeoutZero(connection_data->mcast_sock);
                }
            }
        } else {
            printf("File %s too big", filepath);
        }

    } else {
        printf("File %s does not exist\n", filepath);
    }
}

void executeFileTransfer(struct sockaddr_in server_addr, struct CMPLX_CMD* cmplx_cmd, char* filepath) {
    int tcp_sock = openSocket(TCP);

    server_addr.sin_port = (in_port_t) cmplx_cmd->param;
    if (!connectToAddress(tcp_sock, server_addr)) {
        printf("File %s downloading failed (%s:%lu) Server unavailable\n", cmplx_cmd->data, inet_ntoa(server_addr.sin_addr), cmplx_cmd->param);
    } else if (!receiveFile(tcp_sock, filepath, cmplx_cmd->data)) {
        printf("File %s downloading failed (%s:%lu) Connection dropped\n", cmplx_cmd->data, inet_ntoa(server_addr.sin_addr), cmplx_cmd->param);
    } else {
        printf("File %s downloaded (%s:%lu)\n", cmplx_cmd->data, inet_ntoa(server_addr.sin_addr), cmplx_cmd->param);
    }
    close(tcp_sock);
}

void handleFetch(struct ConnectionData* connection_data, char* filepath, char* filename, struct FileList* list) {
    struct FileNode* file = findFile(list, filename);

    if (file) {
        struct SIMPL_CMD *cmd = getCmd(connection_data->cmd_seq, filename);
        sendSimplCmd(connection_data->mcast_sock, cmd, &(file->host));
        free(cmd);

        struct SIMPL_CMD* simpl_cmd;
        struct CMPLX_CMD* cmplx_cmd;

        struct sockaddr_in server_addr;

        struct timeval start_time, loop_time;
        struct timeval timeout_diff;
        struct timeval timeout_left = connection_data->timeout;

        gettimeofday(&start_time, NULL);

        while (timeout_left.tv_sec > 0 || (timeout_left.tv_sec == 0 && timeout_left.tv_usec > 0)) {
            setReceiveTimeout(connection_data->mcast_sock, timeout_left);
            CommandE e = readCommand(connection_data->mcast_sock, &server_addr, &simpl_cmd, &cmplx_cmd);

            if (e != NO_CMD) {
                isComplex[e] ? printCmplxCmd(cmplx_cmd) : printSimplCmd(simpl_cmd);

                if (e == CONNECT_ME && cmplx_cmd->cmd_seq == connection_data->cmd_seq) {
                    if (fork() == 0) {
                        executeFileTransfer(server_addr, cmplx_cmd, filepath);
                        free(isComplex[e] ? (void *) cmplx_cmd : (void *) simpl_cmd);
                        exit(0);
                    } else {
                        break;
                    }
                } else {
                    printCmdError(server_addr, "Got unexpected command: %s", Command[e]);
                }
                free(isComplex[e] ? (void *) cmplx_cmd : (void *) simpl_cmd);
            } else { // trafimy tutaj jeżeli nie została nam przysłana odpowiedź z serwera
                printf("File %s downloading failed (%s:%lu) Server did not respond\n", cmplx_cmd->data, inet_ntoa(server_addr.sin_addr), cmplx_cmd->param);
            }

            gettimeofday(&loop_time, NULL);
            timersub(&loop_time, &start_time, &timeout_diff);
            timersub(&connection_data->timeout, &timeout_diff, &timeout_left);
        }
        setReceiveTimeoutZero(connection_data->mcast_sock);

    } else {
        printf("File \"%s\" is not present in cached list of files. Please use: \"search [substring]\" first.\n", filename);
    }
}

void handleRemove(struct ConnectionData* connection_data, char* filename) {
    if (strlen(filename) > 0) {
        struct SIMPL_CMD *cmd = delCmd(connection_data->cmd_seq, filename);
        sendSimplCmd(connection_data->mcast_sock, cmd, connection_data->addr);
        free(cmd);
    }
}

void handleSearch(struct ConnectionData* connection_data, char* substring, struct FileList* list) {

    if (!isFileListEmpty(list)) {
        purgeFileList(list);
    }

    struct SIMPL_CMD* cmd = listCmd(connection_data->cmd_seq, substring);
    sendSimplCmd(connection_data->mcast_sock, cmd, connection_data->addr);
    free(cmd);

    struct SIMPL_CMD* simpl_cmd;
    struct CMPLX_CMD* cmplx_cmd;

    struct sockaddr_in server_addr;

    struct timeval start_time, loop_time;
    struct timeval timeout_diff;
    struct timeval timeout_left = connection_data->timeout;

    gettimeofday(&start_time, NULL);

    if (!isFileListEmpty(list)) {
        purgeFileList(list);
    }

    while (timeout_left.tv_sec > 0 || (timeout_left.tv_sec == 0 && timeout_left.tv_usec > 0)) {
        setReceiveTimeout(connection_data->mcast_sock, timeout_left);
        CommandE e = readCommand(connection_data->mcast_sock, &server_addr, &simpl_cmd, &cmplx_cmd);

        if (e != NO_CMD) {
            isComplex[e] ? printCmplxCmd(cmplx_cmd) : printSimplCmd(simpl_cmd);

            if (e == MY_LIST && simpl_cmd->cmd_seq == connection_data->cmd_seq) {
                castStringToFileList(list, simpl_cmd->data, &server_addr);
            } else {
                printCmdError(server_addr, "Got unexpected command: %s", Command[e]);
            }
            free(isComplex[e] ? (void *) cmplx_cmd : (void *) simpl_cmd);
        }

        gettimeofday(&loop_time, NULL);
        timersub(&loop_time, &start_time, &timeout_diff);
        timersub(&(connection_data->timeout), &timeout_diff, &timeout_left);
    }
    printFileList(list);

}

void handleDiscover(struct ConnectionData* connection_data) {

    struct SIMPL_CMD* cmd = helloCmd(connection_data->cmd_seq);
    sendSimplCmd(connection_data->mcast_sock, cmd, connection_data->addr);
    free(cmd);

    struct SIMPL_CMD* simpl_cmd;
    struct CMPLX_CMD* cmplx_cmd;

    struct sockaddr_in server_addr;

    struct timeval start_time, loop_time;
    struct timeval timeout_diff;
    struct timeval timeout_left = connection_data->timeout;

    gettimeofday(&start_time, NULL);

    while (timeout_left.tv_sec > 0 || (timeout_left.tv_sec == 0 && timeout_left.tv_usec > 0)) {
        setReceiveTimeout(connection_data->mcast_sock, timeout_left);
        CommandE e = readCommand(connection_data->mcast_sock, &server_addr, &simpl_cmd, &cmplx_cmd);

        if (e != NO_CMD) {
            isComplex[e] ? printCmplxCmd(cmplx_cmd) : printSimplCmd(simpl_cmd);

            if (e == GOOD_DAY && cmplx_cmd->cmd_seq == connection_data->cmd_seq) {
                printf("Found %s (%s) with free space %lu\n", inet_ntoa(server_addr.sin_addr), cmplx_cmd->data, cmplx_cmd->param);
            } else {
                printCmdError(server_addr, "Got unexpected command: %s", Command[e]);
            }
            free(isComplex[e] ? (void *) cmplx_cmd : (void *) simpl_cmd);
        }

        gettimeofday(&loop_time, NULL);
        timersub(&loop_time, &start_time, &timeout_diff);
        timersub(&connection_data->timeout, &timeout_diff, &timeout_left);
    }
    setReceiveTimeoutZero(connection_data->mcast_sock);
}