//
// Created by nikodem on 31.07.19.
//

#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <netinet/in.h>

// Wielkość fragmentu pliku, który jesteśmy gotowi załadować do pamięci
#define FILE_PART_SIZE 1024

struct FileNode {
    struct FileNode* next;
    int64_t size;
    struct sockaddr_in host;
    char filename[];
};

struct FileList {
    struct FileNode* list;
    int64_t size;
};

extern struct FileList initFileList();

extern void addFile(struct FileList* list, char* filename, int64_t size, struct sockaddr_in* host);

extern struct FileNode* findFile(struct FileList* list, char* filename);

extern int removeFile(struct FileList* list, struct FileNode* node);

extern void purgeFileList(struct FileList* list);

extern void loadFilesFromDir(struct FileList* list, char* dir_path);

extern int isFileListEmpty(struct FileList* list);

extern char* castFileListToString(struct FileList* list, size_t max_size, char* substring);

extern void castStringToFileList(struct FileList* list, char* files, struct sockaddr_in* server);

extern void printFileList(struct FileList* list);

extern FILE* getFile(char* filepath, char* filename, char* mode);

extern int isFile(const char *path);

extern char* getFileNameFromPath(char* filepath);

#endif //FILE_UTILS_H
