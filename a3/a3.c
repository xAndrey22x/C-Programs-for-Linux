#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <dirent.h>
#include <stdbool.h>
#include <sys/mman.h>

#define RESP_PIPE "RESP_PIPE_42663"
#define REQ_PIPE "REQ_PIPE_42663"

int main(){
    unlink(RESP_PIPE);
    bool verified = true;
    if (mkfifo(RESP_PIPE, 0600) != 0){
        printf("ERROR\ncannot create the response pipe\n");
        verified = false;
    }
    int fdRequest = open(REQ_PIPE, O_RDONLY);
    if (fdRequest == -1){
        printf("ERROR\ncannot open the request pipe\n");
        verified = false;
    }
    int fdResponse = open(RESP_PIPE, O_WRONLY);
    char sizeBegin = 5;
    if (write (fdResponse, &sizeBegin , 1) == -1 || write(fdResponse, "BEGIN", sizeBegin) == -1)
        verified = false;

    int shmFd = 0;
    unsigned int shmSize = 0;
    volatile char *shmMemory = NULL;

    int fdMap;
    unsigned int sizeMap = 0;
    char* dataMap = NULL;

    if (verified){
        printf("SUCCESS\n");
        char* reqMessage;
        char reqSize;
        while(1){
            read(fdRequest, &reqSize, 1);
            reqMessage = (char*) malloc(sizeof(char) * reqSize);
            read(fdRequest, reqMessage, reqSize);
            if(strncmp(reqMessage, "VARIANT", reqSize) == 0){
                unsigned int variantNumber = 42663;
                char sizeValue = 5;
                write(fdResponse, &reqSize, 1);
                write(fdResponse, reqMessage, reqSize);
                write(fdResponse, &sizeValue, 1);
                write(fdResponse, "VALUE", sizeValue);
                write(fdResponse, &variantNumber, sizeof(variantNumber));
            }
            else if (strncmp(reqMessage, "CREATE_SHM", reqSize) == 0){
                read(fdRequest, &shmSize, sizeof(shmSize));
                shmFd = shm_open("/RLOZ8GF", O_CREAT | O_RDWR, 0664);
                if (shmFd < 0){
                    char errorSize = 5;
                    write(fdResponse, &reqSize, 1);
                    write(fdResponse, reqMessage, reqSize);
                    write(fdResponse, &errorSize, 1);
                    write(fdResponse, "ERROR", errorSize);
                }
                else {
                    ftruncate(shmFd, shmSize);
                    shmMemory = (volatile char*)mmap(0, shmSize, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
                    char successSize = 7;
                    write(fdResponse, &reqSize, 1);
                    write(fdResponse, reqMessage, reqSize);
                    write(fdResponse, &successSize, 1);
                    write(fdResponse, "SUCCESS", successSize);
                }
            }
            else if (strncmp(reqMessage, "WRITE_TO_SHM", reqSize) == 0){
                unsigned int offsetValue, value;
                read(fdRequest, &offsetValue, sizeof(offsetValue));
                read(fdRequest, &value, sizeof(value));
                if (offsetValue > 0 && offsetValue <= shmSize && (sizeof(value) + offsetValue) <= shmSize){
                        memcpy((char*)(shmMemory + offsetValue), &value, sizeof(value));
                        char successSize = 7;
                        write(fdResponse, &reqSize, 1);
                        write(fdResponse, reqMessage, reqSize);
                        write(fdResponse, &successSize, 1);
                        write(fdResponse, "SUCCESS", successSize);
                    }
                else {
                    char errorSize = 5;
                    write(fdResponse, &reqSize, 1);
                    write(fdResponse, reqMessage, reqSize);
                    write(fdResponse, &errorSize, 1);
                    write(fdResponse, "ERROR", errorSize);
                }
            }
            else if(strncmp(reqMessage, "MAP_FILE", reqSize) == 0){
                char fileSize;
                read(fdRequest, &fileSize, 1);
                char* fileName = (char*) malloc(sizeof(char) * (fileSize + 1));
                read(fdRequest, fileName, fileSize);
                fileName[(int)fileSize] = '\0';
                fdMap = open(fileName, O_RDONLY);
                if (fdMap == -1){
                    char errorSize = 5;
                    write(fdResponse, &reqSize, 1);
                    write(fdResponse, reqMessage, reqSize);
                    write(fdResponse, &errorSize, 1);
                    write(fdResponse, "ERROR", errorSize);
                }
                else{
                    sizeMap = lseek(fdMap, 0, SEEK_END);
                    lseek(fdMap, 0, SEEK_SET);
                    dataMap = (char*)mmap(NULL, sizeMap, PROT_READ, MAP_PRIVATE, fdMap, 0);
                    char successSize = 7;
                    write(fdResponse, &reqSize, 1);
                    write(fdResponse, reqMessage, reqSize);
                    write(fdResponse, &successSize, 1);
                    write(fdResponse, "SUCCESS", successSize);
                }
                free(fileName);
            }
            else if (strncmp(reqMessage, "READ_FROM_FILE_OFFSET", reqSize) == 0){
                unsigned int offsetValue, noOfBytes;
                read(fdRequest, &offsetValue, sizeof(offsetValue));
                read(fdRequest, &noOfBytes, sizeof(noOfBytes));
                if (dataMap == (void*)-1 || dataMap == NULL || shmMemory == (void*)-1 || shmMemory == NULL || 
                offsetValue + noOfBytes > sizeMap){
                    char errorSize = 5;
                    write(fdResponse, &reqSize, 1);
                    write(fdResponse, reqMessage, reqSize);
                    write(fdResponse, &errorSize, 1);
                    write(fdResponse, "ERROR", errorSize);
                }
                else {
                    printf("%d | %d\n", offsetValue, noOfBytes);
                    memcpy((char*) shmMemory, dataMap + offsetValue, noOfBytes);
                    char successSize = 7;
                    write(fdResponse, &reqSize, 1);
                    write(fdResponse, reqMessage, reqSize);
                    write(fdResponse, &successSize, 1);
                    write(fdResponse, "SUCCESS", successSize);
                }
            }
            else if(strncmp(reqMessage, "EXIT", reqSize) == 0){
                free(reqMessage);
                break;
            }
            else {
                free(reqMessage);
                break;
            }
            free(reqMessage);
        }
    }
    munmap((void*)shmMemory, shmSize);
    munmap(dataMap, sizeMap);
    close(shmFd);
    shm_unlink("/RLOZ8GF");
    close(fdMap);
    unlink(RESP_PIPE);
    close(fdRequest);
    close(fdResponse);
    return 0;
}