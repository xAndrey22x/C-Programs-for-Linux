#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

typedef struct headerStruct{
    char MAGIC[5];
    short HEADER_SIZE;
    short VERSION;
    short NO_OF_SECTIONS;
}header;

typedef struct sectionHeader{
    char NAME[16];
    int TYPE;
    int OFFSET;
    int SIZE;
}sHeader;

void freeMem(header** h, sHeader*** s){
    for (int i = 0; i < (*h)->NO_OF_SECTIONS; i++)
        free((*s)[i]);
    free((*s));
    free((*h));
}

int checkForArg(int argc, char **argv){
    // 1 = list
    // 2 = parse
    // 3 = extract
    // 4 = findall
    for (int i = 0; i < argc; i++){
        if (strcmp(argv[i], "list") == 0)
            return 1;
        if (strcmp(argv[i], "parse") == 0)
            return 2;
        if (strcmp(argv[i], "extract") == 0)
            return 3;
        if (strcmp(argv[i], "findall") == 0)
            return 4;
    }
    return 0;
}

void showContent(int argc, char** argv, char* path, char* nameEnds, int hasPerm, int recur){
    int recursive = 0;
    if (recur == 0){
        for (int i = 0; i < argc; i++){
            if(strncmp(argv[i], "path", 4) == 0){
                    path = (char*) malloc(sizeof(char) * strlen(argv[i]));
                    strcpy(path, argv[i] + 5);
                }
            if(strcmp(argv[i], "recursive") == 0)
                recursive = 1;
            if (strncmp(argv[i], "name_ends_with", 14) == 0){
                    nameEnds = (char*) malloc(sizeof(char) * strlen(argv[i]));
                    strcpy(nameEnds, argv[i]+15);
                }
            if (strcmp(argv[i], "has_perm_write") == 0)
                hasPerm = 1;
        }
    }
    else {
        recursive = 1;
    }
    DIR* dir = NULL;
    struct dirent *entry = NULL;
    char fullPath[1024];
    struct stat statbuf;
    dir = opendir(path);
    if(dir == NULL){
        printf("ERROR\ninvalid directory path");
        free(path);
        return ;
    }
    if (recur == 0)
        printf("SUCCESS\n");
    
    while((entry = readdir(dir)) != NULL){
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0){
            sprintf(fullPath, "%s/%s", path, entry->d_name);
            if(lstat(fullPath, & statbuf) == 0){
                if (nameEnds == NULL && hasPerm == 0)
                    printf("%s\n", fullPath);
                if (nameEnds != NULL && hasPerm == 1)
                    if(strcmp(entry->d_name+(strlen(entry->d_name) - strlen(nameEnds)), nameEnds) == 0 && (statbuf.st_mode & S_IWUSR) == S_IWUSR)
                        printf("%s\n", fullPath);
                if (nameEnds != NULL && hasPerm == 0)
                    if(strcmp(entry->d_name+(strlen(entry->d_name) - strlen(nameEnds)), nameEnds) == 0)
                        printf("%s\n", fullPath);
                if (nameEnds == NULL && hasPerm == 1)
                    if ((statbuf.st_mode & S_IWUSR) == S_IWUSR)
                        printf("%s\n", fullPath);
                if (recursive == 1 && S_ISDIR(statbuf.st_mode)){
                    showContent(argc, argv, fullPath, nameEnds, hasPerm, 1);
                }
            }
        }
    }
    if (recur == 0){
        free(path);
        free(nameEnds);
    }
    closedir(dir);
}

int checkFormat(int argc, char** argv, header** h, sHeader*** s, char* errorMessage, char* fullPath){
    char* path;
    if (argc != 0)
        for (int i = 0; i < argc; i++){
                if(strncmp(argv[i], "path", 4) == 0){
                        path = (char*) malloc(sizeof(char) * strlen(argv[i]));
                        strcpy(path, argv[i] + 5);
                    }
        }
    else {
        path = (char*) malloc(sizeof(char) * 1024);
        strcpy(path, fullPath);
    }
    int fd = open(path, O_RDONLY);
    int k = 1;
    if(fd == -1) {
        strcpy(errorMessage, "invalid file path");
        free(path);
        return 0;
    }
    (*h) = (header*) malloc(sizeof(header));
    (*h)->NO_OF_SECTIONS = 0;
    lseek(fd, 0, SEEK_SET);
    read(fd, (*h)->MAGIC, 4);
    read(fd, &(*h)->HEADER_SIZE, 2);
    read(fd, &(*h)->VERSION, 2);
    read(fd, &(*h)->NO_OF_SECTIONS, 1);

    if((*h)->MAGIC[0] != 0x6C && (*h)->MAGIC[1] != 0x4A && (*h)->MAGIC[2] != 0x39 && (*h)->MAGIC[3] != 0x6C){
        strcpy(errorMessage, "wrong magic");
        k = 0;
    }
    if ((*h)->VERSION < 101 || (*h)->VERSION > 134){
        strcpy(errorMessage, "wrong version");
        k = 0;
    }
    if ((*h)->NO_OF_SECTIONS < 7 || (*h)->NO_OF_SECTIONS > 14){
        strcpy(errorMessage, "wrong sect_nr");
        (*h)->NO_OF_SECTIONS = 0;
        k = 0;
    }
    (*s) = (sHeader**)malloc(sizeof(sHeader*) * (*h)->NO_OF_SECTIONS);
    for (int i = 0; i < (*h)->NO_OF_SECTIONS; i++){
        (*s)[i] = (sHeader*) malloc(sizeof(sHeader));
        read(fd, &(*s)[i]->NAME, 14);
        read(fd, &(*s)[i]->TYPE, 4);
        read(fd, &(*s)[i]->OFFSET, 4);
        read(fd, &(*s)[i]->SIZE, 4);
        if ((*s)[i]->TYPE != 85 && (*s)[i]->TYPE != 57 && (*s)[i]->TYPE != 45 && (*s)[i]->TYPE != 44 && (*s)[i]->TYPE != 80){
            strcpy(errorMessage, "wrong sect_types");
            k = 0;
        }
    }
    if (k == 1){
        free(path);
        return 1;
    }
    free(path);
    close(fd);
    return 0;
}

void parseFile(int argc, char** argv){
    char errorMessage[30];
    header* h = NULL;
    sHeader** s = NULL;
    if (checkFormat(argc, argv, &h, &s, errorMessage, NULL) == 1){
        printf("SUCCESS\nversion=%d\nnr_sections=%d\n", h->VERSION, h->NO_OF_SECTIONS);
            for (int i = 0; i < h->NO_OF_SECTIONS; i++){
                printf("section%d: %s %d %d\n", (i + 1), s[i]->NAME, s[i]->TYPE, s[i]->SIZE);
            }
    }
    else {
        printf("ERROR\n");
        printf("%s", errorMessage);
    }
    freeMem(&h, &s);
}

void extractFromFile(int argc, char** argv){
    char errorMessage[30];
    header* h = NULL;
    sHeader** s = NULL;
    int section = 0, line = 0;
    char *path;
    if (checkFormat(argc, argv, &h, &s, errorMessage, NULL) == 1){
        for (int i = 0; i < argc; i++){
            if(strncmp(argv[i], "path", 4) == 0){
                    path = (char*) malloc(sizeof(char) * strlen(argv[i]));
                    strcpy(path, argv[i] + 5);
                }
            if(strncmp(argv[i], "section", 7) == 0)
                    sscanf((argv[i] + 8), "%d", &section);
            if(strncmp(argv[i], "line", 4) == 0)
                    sscanf((argv[i] + 5), "%d", &line);
        }
        if(section < 1 || section > h->NO_OF_SECTIONS){
            printf("ERROR\ninvalid section");
            free(path);
            freeMem(&h, &s);
            return;
        }
        int fd = open(path, O_RDONLY);
        lseek(fd, s[section - 1]->OFFSET + s[section - 1]->SIZE, SEEK_SET);
        char c = 0;
        int currentPos = lseek(fd, 0, SEEK_CUR);
        int lineCount = 1;
        int k = 0;
        while(currentPos != s[section - 1]->OFFSET - 1 && lineCount <= line){
            read(fd, &c, 1);
            if (lineCount == line){
                char* finalLine = (char*) malloc(sizeof(char) * (currentPos - s[section - 1]->OFFSET + 1));
                lseek(fd, -(currentPos - s[section - 1]->OFFSET + 1) , SEEK_CUR);
                read(fd, finalLine, (currentPos - s[section - 1]->OFFSET + 1));
                if (k == 0)
                    printf("SUCCESS\n");
                int del = 0x0A;
                char delim[2];
                sprintf(delim, "%c", del);
                char* finalLineBuff = (char*) malloc(sizeof(char) * strlen(finalLine));
                char* freeBuf = finalLineBuff;
                finalLineBuff = strtok(finalLine, delim);
                while (finalLineBuff != NULL){
                    strcpy(finalLine, finalLineBuff);
                    finalLineBuff = strtok(NULL, delim);
                }
                k = 1;
                int length = strlen(finalLine);
                for (int i = length - 1; i >= 0; i--){
                    printf("%c", finalLine[i]);
                }
                free(freeBuf);
                free(finalLine);
                break;
            }
            if (c == 0x0A)
                lineCount++;
            currentPos = lseek(fd, -2, SEEK_CUR);
        }
        if (k == 0){
            printf("ERROR\ninvalid line");
            free(path);
            freeMem(&h, &s);
            close(fd);
            return;
        }
        close(fd);
    }
    else {
            printf("ERROR\ninvalid file");
            freeMem(&h, &s);
            return;
        }
    free(path);
    freeMem(&h, &s);
}

void findFiles(int argc, char** argv, char* path, int recur){
    if (recur == 0)
        for (int i = 0; i < argc; i++)
                if(strncmp(argv[i], "path", 4) == 0){
                        path = (char*) malloc(sizeof(char) * strlen(argv[i]));
                        strcpy(path, argv[i] + 5);
                    }
    DIR* dir = NULL;
    struct dirent *entry = NULL;
    char fullPath[1024];
    struct stat statbuf;
    dir = opendir(path);
    if(dir == NULL){
        printf("ERROR\ninvalid directory path");
        free(path);
        return ;
    }
    if(recur == 0)
        printf("SUCCESS\n");
    while((entry = readdir(dir)) != NULL){
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0){
            sprintf(fullPath, "%s/%s", path, entry->d_name);
            if(lstat(fullPath, &statbuf) == 0){
                if (S_ISDIR(statbuf.st_mode))
                    findFiles(argc, argv, fullPath, 1);
                else if (S_ISREG(statbuf.st_mode)){
                    char errorMessage[30];
                    header* h = NULL;
                    sHeader** s = NULL;
                    if(checkFormat(0, NULL, &h, &s, errorMessage, fullPath) == 1){
                        int k = 1;
                        for (int i = 0; i < h->NO_OF_SECTIONS; i++)
                            if (s[i]->SIZE > 1086)
                                k = 0;
                        if (k == 1)
                            printf("%s\n", fullPath);
                    }
                    freeMem(&h, &s);
                }
                
            }
        }
    }
    if (recur == 0)
        free(path);
    closedir(dir);
}

int main(int argc, char **argv){
    if(argc == 2){
        if(strcmp(argv[1], "variant") == 0){
            printf("42663\n");
        }
    }
    if (checkForArg(argc, argv) == 1)
        showContent(argc, argv, NULL, NULL, 0, 0);
    else if (checkForArg(argc, argv) == 2)
           parseFile(argc, argv);
           else if (checkForArg(argc, argv) == 3)
                    extractFromFile(argc, argv);
            else if (checkForArg(argc, argv) == 4)
                    findFiles(argc, argv, NULL, 0);
    return 0;
}