#include "receiver.h"

void signal_handler(int signum) {
    if (sem_receiver != SEM_FAILED || sem_sender != SEM_FAILED) {
        sem_close(sem_receiver);
        sem_close(sem_sender);
        sem_unlink("/sem_receiver");
        sem_unlink("/sem_sender");
    }
    printf("Resources cleaned up\n");
    exit(0);
}

void receive(message_t* message_ptr, mailbox_t* mailbox_ptr) {
    if (mailbox_ptr->flag == MESSAGE_PASSING) {
        // printf("msgrcv\n");
        if(msgrcv(mailbox_ptr->storage.msqid, message_ptr, sizeof(*message_ptr) - sizeof(long), SENDER, IPC_NOWAIT) == -1){
            msgrcv(mailbox_ptr->storage.msqid, message_ptr, sizeof(*message_ptr) - sizeof(long), SENDER_EXIT, IPC_NOWAIT);
        }
        return;
    } else if (mailbox_ptr->flag == SHARED_MEMORY) {
        strcpy(message_ptr->mtext, mailbox_ptr->storage.shm_addr);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2){ // Expecting exactly 1 argument after the executable name
        printf("Usage: %s <mechanism(1/2)>\n", argv[0]);
        return 1;
    }

    //補捉ctrl+c，強制結束時清理資源
    signal(SIGINT, signal_handler);

    struct timespec start, end;
    double time_taken;
    double total_time = 0;
    key_t msg_key;
    int shmid;
    mailbox_t mailbox;
    message_t message;

    // 创建并初始化信号量
    sem_receiver = sem_open("/sem_receiver", O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0);
    sem_sender = sem_open("/sem_sender", O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1);
    if (sem_receiver == SEM_FAILED || sem_sender == SEM_FAILED) {
        // printf("semaphore exist\n");
        sem_receiver = sem_open("/sem_receiver", O_RDWR);
        sem_sender = sem_open("/sem_sender", O_RDWR);
        
        if (sem_receiver == SEM_FAILED || sem_sender == SEM_FAILED) {
            printf("sem_open error\n");
            return 0;
        }
    }

    // (system V) 通过文件获得唯一的 key_t 值，用於创建 message queue / shared memory
    msg_key = ftok("./sender.c", 10);
    if (msg_key < 0) {
        printf("ftok error\n");
        return 0;
    }

    // 选择通信方式：消息传递或共享内存
    int choice = atoi(argv[1]);
    printf("\033[36m\033[01mChoose communication method (1 for Message Passing, 2 for shared memory):\033[0m %d\n", choice);
    // scanf("%d", &choice);
    
    if (choice == MESSAGE_PASSING) {
        mailbox.flag = MESSAGE_PASSING;
        // 创建消息队列
        mailbox.storage.msqid = msgget(msg_key, IPC_CREAT | S_IRUSR | S_IWUSR);
        if (mailbox.storage.msqid < 0) {
            printf("msgget error\n");
            return 0;
        }
    } else if (choice == SHARED_MEMORY) {
        mailbox.flag = SHARED_MEMORY;
        // 创建共享内存
        shmid = shmget(msg_key, 1024, IPC_CREAT | S_IRUSR | S_IWUSR);
        if (shmid < 0) {
            printf("shmget error\n");
            return 0;
        }
        // 将共享内存附加到进程的地址空间
        mailbox.storage.shm_addr = (char*)shmat(shmid, NULL, 0);
        if (mailbox.storage.shm_addr == (void*)-1) {
            perror("Shared memory attach error");
        }
    } else {
        printf("Invalid choice\n");
        return 0;
    }

    while (1) {
        sem_wait(sem_receiver);
        // printf("get sem_receiver\n");

        clock_gettime(CLOCK_MONOTONIC, &start);
        receive(&message, &mailbox);
        clock_gettime(CLOCK_MONOTONIC, &end);
        time_taken = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
        total_time += time_taken;

        if (strcmp(message.mtext, "exit") != 0) {
            printf("\033[36m\033[01mReceiver received:\033[0m %s\n", message.mtext);
        }else if (strcmp(message.mtext, "exit") == 0) {
            printf("\033[31m\033[01mSender exit!\033[0m\n");
            break;
        }

        sem_post(sem_sender);
    }

    printf("Total time taken in receiving msg: %f s\n", total_time);

    sem_close(sem_receiver);
    sem_close(sem_sender);
    sem_unlink("/sem_receiver");
    sem_unlink("/sem_sender");

    if (mailbox.flag == MESSAGE_PASSING) {
        msgctl(mailbox.storage.msqid, IPC_RMID, NULL);
    } else if (mailbox.flag == SHARED_MEMORY) {
        shmdt(mailbox.storage.shm_addr);
        shmctl(shmid, IPC_RMID, NULL);
    }

    return 0;
}
