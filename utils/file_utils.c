//
// Created by nikodem on 31.07.19.
//

#include "file_utils.h"

struct FileList initFileList() {
    struct FileList list;
    list.list = NULL;
    list.size = 0;
    return list;
}

void addFile(struct FileList* list, char* filename, int64_t size, struct sockaddr_in* host) {
    size_t filename_len = strlen(filename);

    struct FileNode* node = malloc(sizeof(struct FileNode) + sizeof(char) * (filename_len + 1));
    node->size = size;
    strcpy(node->filename, filename);
    node->next = list->list;

    if (host != NULL) {
        node->host = *host;
    }

    list->size += size;
    list->list = node;
}

struct FileNode* findFile(struct FileList* list, char* filename) {
    struct FileNode* node_iter = list->list;
    while (node_iter != NULL) {
        if (strcmp(node_iter->filename, filename) == 0) {
            return node_iter;
        }
        node_iter = node_iter->next;
    }
    return NULL;
}

int removeFile(struct FileList* list, struct FileNode* node) {
    if (node == NULL) {
        return 0;
    }
    struct FileNode* last_node = NULL;
    struct FileNode* node_iter = list->list;

    while (node_iter != NULL) {
        if (node_iter == node) {
            if (last_node == NULL) {
                list->list = node_iter->next;
            } else {
                last_node->next = node_iter->next;
            }

            list->size -= node_iter->size;
            free(node_iter);
            return 1;
        }

        last_node = node_iter;
        node_iter = node_iter->next;
    }
    return 0;
}

void __reqDelFile(struct FileNode* node) {
    if (node == NULL) {
        return;
    }
    __reqDelFile(node->next);
    free(node);
}

void purgeFileList(struct FileList* list) {
    __reqDelFile(list->list);
    list->size = 0;
    list->list = NULL;
}

void loadFilesFromDir(struct FileList* list, char* dir_path) {

    DIR *directory;
    struct dirent *node;
    directory = opendir(dir_path);

    if (directory) {
        while ((node = readdir(directory)) != NULL) {
            if (node->d_type == DT_REG) {

                char* full_path = malloc((strlen(dir_path) + strlen(node->d_name) + 1) * sizeof(char));
                strcpy(full_path, dir_path);
                strcat(full_path, node->d_name);

                struct stat info;
                if (stat(full_path, &info) < 0) {
                    syserr("read file: %s", full_path);
                }

                addFile(list, node->d_name, info.st_size, NULL);
            }
        }
        closedir(directory);
    } else {
        fatal("given path to directory is incorrect");
    }
}

int isFileListEmpty(struct FileList* list) {
    return list->list == NULL;
}

char* castFileListToString(struct FileList* list, size_t max_size, char* substring) {

    size_t str_len = 0;
    char* files_str = NULL;

    while (list->list != NULL && str_len < max_size) {
        if (substring == NULL || (*substring) == '\0' || strstr(list->list->filename, substring) != NULL) {
            size_t file_str_len = strlen(list->list->filename) + 1;

            if (str_len + file_str_len <= max_size) {
                if (files_str == NULL) {
                    files_str = malloc(0);
                }
                files_str = realloc(files_str, (file_str_len + str_len) * sizeof(char));
                memcpy(files_str + str_len, list->list->filename, strlen(list->list->filename));
                str_len += file_str_len;
                files_str[str_len - 1] = '\n';
            } else {
                break;
            }
        }

        list->list = list->list->next; // tworzymy iterację po liście
    }

    if (files_str != NULL) {
        files_str[str_len - 1] = '\0';
    }

    return files_str;
}

void castStringToFileList(struct FileList* list, char* files, struct sockaddr_in* server) {

    while (files != NULL) {
        char* temp_files = strstr(files, "\n");
        if (temp_files != NULL) {
            (*temp_files) = '\0';
        }

        size_t file_len = strlen(files);
        if (file_len > 0) {
            addFile(list, files, -1, server);
        }
        
        files = temp_files != NULL ? temp_files + 1 : NULL;
    }
}

void printFileList(struct FileList* list) {
    struct FileNode* node_iter = list->list;
    while (node_iter != NULL) {
        printf("%s (%s)\n", node_iter->filename, inet_ntoa(node_iter->host.sin_addr));
        node_iter = node_iter->next;
    }
}

FILE* getFile(char* filepath, char* filename, char* mode) {
    char fullpath[strlen(filename) + strlen(filepath) + 1];
    strcpy(fullpath, filepath);
    strcat(fullpath, filename);

    FILE* file = fopen(fullpath, mode);
    return file;
}

int isFile(const char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0)
        return 0;
    return S_ISREG(statbuf.st_mode);
}

char* getFileNameFromPath(char* filepath) {
    char* whoami;
    (whoami = strrchr(filepath, '/')) ? ++whoami : (whoami = filepath);
    return whoami;
}