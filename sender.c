#include "sender.h"

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

void send(message_t message, mailbox_t* mailbox_ptr) {
    // 如果输入 "exit"，送出結束訊號給 receiver
    if (strcmp(message.mtext, "exit") == 0) {
        if (mailbox_ptr->flag == MESSAGE_PASSING) {
            message.mtype = SENDER_EXIT;
            msgsnd(mailbox_ptr->storage.msqid, &message, sizeof(message) - sizeof(long), 0); //0: 阻塞式發送
        } else if (mailbox_ptr->flag == SHARED_MEMORY) {
            strcpy(mailbox_ptr->storage.shm_addr, message.mtext);
        }
        return;
    }

    // 根据通信方式发送消息
    if (mailbox_ptr->flag == MESSAGE_PASSING) {
        message.mtype = SENDER;
        msgsnd(mailbox_ptr->storage.msqid, &message, sizeof(message) - sizeof(long), 0); //0: 阻塞式發送
    } else if (mailbox_ptr->flag == SHARED_MEMORY) {
        strcpy(mailbox_ptr->storage.shm_addr, message.mtext);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {  // Expecting exactly 2 arguments after the executable name
        printf("Usage: %s <mechanism(1/2)> <input_file>\n", argv[0]);
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
    // scanf("%d", &choice);

    if (choice == MESSAGE_PASSING) {
        printf("\033[36m\033[01mMessage Passing\033[0m\n");
        mailbox.flag = MESSAGE_PASSING;
        // 创建消息队列
        mailbox.storage.msqid = msgget(msg_key, IPC_CREAT | S_IRUSR | S_IWUSR);
        if (mailbox.storage.msqid < 0) {
            printf("msgget error\n");
            return 0;
        }
    } else if (choice == SHARED_MEMORY) {
        printf("\033[36m\033[01mShared Memory\033[0m\n");
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
    
    // 打开 input 文件
    FILE *fmessage = fopen(argv[2], "r");
    if (fmessage == NULL) {
        perror("Error opening input file");
        exit(EXIT_FAILURE);
    }

    // printf("\033[36m\033[01mInput 'exit' to exit\033[0m \n");
    while (1) {
        sem_wait(sem_sender);

        // fgets(message.mtext, sizeof(message.mtext), stdin); // 读取用户输入
        // message.mtext[strcspn(message.mtext, "\n")] = '\0'; // 去除换行符
        // 从文件读取消息
        if (fgets(message.mtext, sizeof(message.mtext), fmessage) != NULL) {
            message.mtext[strcspn(message.mtext, "\n")] = '\0'; // 去除换行符
            printf("\033[36m\033[01mSending message: \033[0m ");
            printf("%s\n", message.mtext);
            
            clock_gettime(CLOCK_MONOTONIC, &start);
            send(message, &mailbox);
            clock_gettime(CLOCK_MONOTONIC, &end);
            time_taken = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
            total_time += time_taken;

            sem_post(sem_receiver);
        }else if (feof(fmessage)) { //fgets 回傳 NULL 且為 EOF (fgets 會讀取到 \n 或 EOF 或讀取失敗才會回傳 NULL)
            // printf("debug\n");
            strcpy(message.mtext, "exit");

            clock_gettime(CLOCK_MONOTONIC, &start);
            send(message, &mailbox);
            clock_gettime(CLOCK_MONOTONIC, &end);
            time_taken = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
            total_time += time_taken;

            printf("\033[31m\033[01mEnd of input file! exit!\033[0m\n");
            sem_post(sem_receiver);
            break;
        } else if(!feof(fmessage)){ //fgets 回傳 NULL 且並非 EOF
            perror("fgets()"); // 读取文件失败
            break;
        }
    }

    printf("Total time taken in sending msg: %f s\n", total_time);
    fclose(fmessage);

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
