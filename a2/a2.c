#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <signal.h>
#include "a2_helper.h"
#include <sys/types.h>
#include <sys/wait.h>

typedef struct {
    int id;
    int Pid;
    sem_t *currentSem;
    sem_t *dependecySem;
    sem_t *semDiffProc1;
    sem_t *semDiffProc2;
} TH_STRUCT;

int found = 0;
int finish = 0;
int countThreads = 0;
int confirmedThreads = 0;

void *threadFunc(void *semStr)
{   
    TH_STRUCT* semStruct = (TH_STRUCT*)semStr;
    if(semStruct->id == 2 && semStruct->Pid == 6){ // wait for process P6 T2
        sem_wait(semStruct->semDiffProc2);
    }
    if(semStruct->id == 3 && semStruct->Pid == 8){ // wait for process P8 T3
        sem_wait(semStruct->semDiffProc1);
    }
    sem_wait(semStruct->currentSem); //normal wait for danger area
    info(BEGIN, semStruct->Pid, semStruct->id);
    if(semStruct->id == 4 && semStruct->Pid == 8){ // if thread 4, activate thread 1 and wait for thread 1 to complete, P8
        sem_post(semStruct->dependecySem);
        sem_wait(semStruct->currentSem);
    }
    info(END, semStruct->Pid, semStruct->id);
    if (semStruct->id == 1 && semStruct->Pid == 8) // after thread 1 is completed, activate thread 4 to finnish too, P8
        sem_post(semStruct->dependecySem); 
    sem_post(semStruct->currentSem); //normal post for danger area
    if(semStruct->id == 3 && semStruct->Pid == 6){ //post for P6 T3
        sem_post(semStruct->semDiffProc1);
    }
    if(semStruct->id == 3 && semStruct->Pid == 8){ //post for P8 T3
        sem_post(semStruct->semDiffProc2);
    }
    return NULL;
}

void *threadFuncBarr(void *semStr){
    TH_STRUCT* semStruct = (TH_STRUCT*)semStr;
    sem_wait(semStruct->currentSem); //normal wait for danger area
    info(BEGIN, semStruct->Pid, semStruct->id);
    countThreads++;
    confirmedThreads++;
    if (semStruct->id == 11){
        found = 1;
        while(1){
            if (confirmedThreads == 4){
                info(END, semStruct->Pid, semStruct->id);
                confirmedThreads--; 
                finish = 1;
                found = 0;
                break;
            }
        }
    }
    if (found == 1){
        while(1){
            if(finish == 1 && confirmedThreads == 3)
                break;
        }
        found = 0;
    }
    else if (countThreads > 37){
        while(1){
            if(finish == 1)
                break;
        }
    }
    if (semStruct->id != 11){
        info(END, semStruct->Pid, semStruct->id);
        confirmedThreads--;
    }
    sem_post(semStruct->currentSem); //normal post for danger area
    return NULL;
}

int main(){
    init();
    info(BEGIN, 1, 0);

    pid_t p[10];
    sem_unlink("/semaphore");
    sem_t* semDiffProc1 = sem_open("/semaphore", O_CREAT, 0644, 0);
    sem_unlink("/semaphore2");
    sem_t* semDiffProc2 = sem_open("/semaphore2", O_CREAT, 0644, 0);
    p[0] = fork();
    if (p[0] == 0){ // P2
        info(BEGIN, 2, 0);
        p[1] = fork();
        if (p[1] == 0){ //P3
            info(BEGIN, 3, 0);
            p[2] = fork();
            if (p[2] == 0){ //P8
                info(BEGIN, 8, 0);
                pthread_t tid[10];
                sem_t logSem[4];
                TH_STRUCT semStruct[4];
                sem_init(&logSem[0], 0, 0);
                semStruct[0].currentSem = &logSem[0];  
                for (int i = 1; i < 4; i++){
                    sem_init(&logSem[i], 0, 1);
                    semStruct[i].currentSem = &logSem[i];  
                }
                semStruct[0].dependecySem = semStruct[3].currentSem; // thread 1 associated with thread 4
                semStruct[3].dependecySem = semStruct[0].currentSem; // thread 4 associated with thread 1
                semStruct[2].semDiffProc1 = semDiffProc1;
                semStruct[2].semDiffProc2 = semDiffProc2;
                for (int i = 0; i < 4; i++) {
                    semStruct[i].Pid = 8;
                    semStruct[i].id = i + 1;
                    pthread_create(&tid[i], NULL, threadFunc, (void*)&semStruct[i]);
                }
                for (int i = 0; i < 4; i++)
                    pthread_join(tid[i], NULL);
                info(END, 8, 0);
                for (int i = 0; i < 4; i++)
                    sem_destroy(&logSem[i]);
                exit(0);
            }
            waitpid(p[2], NULL, 0);
            info(END, 3, 0);
            exit(0);
        }
        p[3] = fork();
        if (p[3] == 0) { //P4
            info(BEGIN, 4, 0);
            p[4] = fork();
            if (p[4] == 0) { //P6
                info(BEGIN, 6, 0);
                pthread_t tid[5];
                TH_STRUCT semStruct[5];
                sem_t logSem;
                sem_init(&logSem, 0, 1);
                for (int i = 0; i < 5; i++){
                    semStruct[i].Pid = 6;
                    semStruct[i].id = i + 1;
                    semStruct[i].currentSem = &logSem;  
                }
                semStruct[2].semDiffProc1 = semDiffProc1;
                semStruct[1].semDiffProc2 = semDiffProc2;
                for (int i = 0; i < 5; i++){
                    pthread_create(&tid[i], NULL, threadFunc, (void*)&semStruct[i]);
                }
                for (int i = 0; i < 5; i++)
                    pthread_join(tid[i], NULL);
                p[5] = fork();
                if (p[5] == 0){ //P7
                    info(BEGIN, 7, 0);
                    info(END, 7, 0);
                    exit(0);
                }
                waitpid(p[5], NULL, 0);
                info(END, 6, 0);
                exit(0);
            }
            waitpid(p[4], NULL, 0);
            info(END, 4, 0);
            exit(0);
        }
        p[6] = fork();
        if (p[6] == 0){ //P5
            info(BEGIN, 5, 0);
            info(END, 5, 0);
            exit(0);
        }
        p[7] = fork();
        if (p[7] == 0){ //P9
            info(BEGIN, 9, 0);
            pthread_t tid[41];
            TH_STRUCT semStruct[41];
            sem_t logSem;
            sem_init(&logSem, 0, 4);
            for (int i = 0; i < 41; i++){
                semStruct[i].Pid = 9;
                semStruct[i].id = i + 1;
                semStruct[i].currentSem = &logSem;  
            }
            pthread_create(&tid[10], NULL, threadFuncBarr, (void*)&semStruct[10]);
            for (int i = 0; i < 41; i++){
                if (i != 10)
                    pthread_create(&tid[i], NULL, threadFuncBarr, (void*)&semStruct[i]);
            }
            for (int i = 0; i < 41; i++)
                pthread_join(tid[i], NULL); 
            sem_destroy(&logSem);
            info(END, 9, 0);
            exit(0);
        }
        waitpid(p[1], NULL, 0);
        waitpid(p[3], NULL, 0);
        waitpid(p[6], NULL, 0);
        waitpid(p[7], NULL, 0);
        info(END, 2, 0);
        exit(0);
    }
    waitpid(p[0], NULL, 0);
    sem_close(semDiffProc1);
    sem_close(semDiffProc2);
    info(END, 1, 0);
    return 0;
}