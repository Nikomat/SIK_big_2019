//
// Created by nikodem on 28.07.19.
//

#ifndef USER_INPUT_H
#define USER_INPUT_H

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <zconf.h>
#include <rpc/types.h>
#include <arpa/inet.h>

#include "types.h"
#include "command_utils.h"

#define DEBUG 0

/*
 * discover – po otrzymaniu tego polecenia klient powinien wypisać na standardowe wyjście listę wszystkich węzłów serwerowych dostępnych aktualnie w grupie.
 * search %s – klient powinien uznać polecenie za prawidłowe, także jeśli podany ciąg znaków %s jest pusty. Otrzymane listy plików powinny zostać wypisane na standardowe wyjście po jednej linii na jeden plik. Każda linia powinna zawierać informację: {nazwa_pliku} ({ip_serwera})
 * fetch %s – użytkownik może wskazać nazwę pliku %s, tylko jeśli nazwa pliku występowała na liście otrzymanej w wyniku ostatniego wykonania polecenia search.
 * upload %s – użytkownik powinien wskazać ścieżkę do pliku, który chce wysłać do przechowania w grupie.
 * exit – klient po otrzymaniu tego polecenia powinien zakończyć wszystkie otwarte połączenia i zwolnić pobrane zasoby z systemu oraz zakończyć pracę aplikacji.
 */
typedef enum {
    DISCOVER,
    SEARCH,
    FETCH,
    UPLOAD,
    REMOVE,
    EXIT,
    ACTION_ENUM_SIZE,

    UNKNOWN_ACTION
} ActionEnum;

struct UserInput {
    ActionEnum action;
    char* full_command;
    ssize_t command_size;
    char* arg;
};

const static char* Action[ACTION_ENUM_SIZE] = {"discover", "search", "fetch", "upload", "remove", "exit"};

const static int isActionArgObligatory[ACTION_ENUM_SIZE] = {-1, 0, 1, 1, 1, -1};

extern struct UserInput getUserInput();

extern void printCmdError(struct sockaddr_in address, const char *__restrict __format, ...);

extern void printSimplCmd(struct SIMPL_CMD* cmd);

extern void printCmplxCmd(struct CMPLX_CMD* cmd);

extern void debugLog(const char *__restrict __format, ...);

#endif //USER_INPUT_H
