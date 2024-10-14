#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <time.h>

#define MESSAGE_PASSING 1
#define SHARED_MEMORY 2
#define SENDER 1
#define SENDER_EXIT 2

sem_t *sem_receiver;
sem_t *sem_sender;

// 定义消息传递和共享内存，共用的 mailbox
typedef struct {
    int flag; // 1 for message passing, 2 for shared memory
    union{
        int msqid; //for system V api. You can replace it with struecture for POSIX api
        char* shm_addr;
    }storage;
} mailbox_t;

// 定义消息传递和共享内存结构
typedef struct {
    long mtype;
    char mtext[1024];  // Message content
} message_t;

void send(message_t message, mailbox_t* mailbox_ptr);