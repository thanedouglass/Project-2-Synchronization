#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <time.h>
#include <signal.h>

typedef struct {
    int bankAccount;
    sem_t mutex;
} SharedData;

int ShmID;
SharedData *shared;

void cleanup(int sig) {
    sem_destroy(&shared->mutex);
    shmdt((void *)shared);
    shmctl(ShmID, IPC_RMID, NULL);
    exit(0);
}

void DadProcess(SharedData *shared) {
    srand(getpid() + time(NULL));
    int localBalance;
    while(1) {
        sleep(rand() % 6); // Sleep 0 - 5 seconds
        printf("Dear Old Dad: Attempting to Check Balance\n");
        
        sem_wait(&shared->mutex);
        int r = rand();
        if (r % 2 == 0) {
            localBalance = shared->bankAccount;
            if (localBalance < 100) {
                int deposit_r = rand();
                int amount = rand() % 101; // 0 to 100
                if (deposit_r % 2 == 0) {
                    localBalance += amount;
                    printf("Dear old Dad: Deposits $%d / Balance = $%d\n", amount, localBalance);
                    shared->bankAccount = localBalance;
                } else {
                    printf("Dear old Dad: Doesn't have any money to give\n");
                }
            } else {
                printf("Dear old Dad: Thinks Student has enough Cash ($%d)\n", localBalance);
            }
        } else {
            localBalance = shared->bankAccount;
            printf("Dear Old Dad: Last Checking Balance = $%d\n", localBalance);
        }
        sem_post(&shared->mutex);
    }
}

void MomProcess(SharedData *shared) {
    srand(getpid() + time(NULL));
    int localBalance;
    while(1) {
        sleep(rand() % 11); // Sleep 0 - 10 seconds
        printf("Loveable Mom: Attempting to Check Balance\n");
        
        sem_wait(&shared->mutex);
        localBalance = shared->bankAccount;
        if (localBalance <= 100) {
            int amount = rand() % 126; // 0 to 125
            localBalance += amount;
            printf("Lovable Mom: Deposits $%d / Balance = $%d\n", amount, localBalance);
            shared->bankAccount = localBalance;
        }
        sem_post(&shared->mutex);
    }
}

void StudentProcess(SharedData *shared, int id) {
    srand(getpid() + time(NULL) + id);
    int localBalance;
    while(1) {
        sleep(rand() % 6); // Sleep 0 - 5 seconds
        printf("Poor Student: Attempting to Check Balance\n");
        
        sem_wait(&shared->mutex);
        int r = rand();
        if (r % 2 == 0) {
            localBalance = shared->bankAccount;
            int need = rand() % 51; // 0 to 50
            printf("Poor Student needs $%d\n", need);
            if (need <= localBalance) {
                localBalance -= need;
                printf("Poor Student: Withdraws $%d / Balance = $%d\n", need, localBalance);
                shared->bankAccount = localBalance;
            } else {
                printf("Poor Student: Not Enough Cash ($%d)\n", localBalance);
            }
        } else {
            localBalance = shared->bankAccount;
            printf("Poor Student: Last Checking Balance = $%d\n", localBalance);
        }
        sem_post(&shared->mutex);
    }
}

int main(int argc, char *argv[]) {
    int parent_mode = 1;
    int num_students = 1;

    if (argc >= 3) {
        parent_mode = atoi(argv[1]);
        num_students = atoi(argv[2]);
    }

    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    ShmID = shmget(IPC_PRIVATE, sizeof(SharedData), IPC_CREAT | 0666);
    if (ShmID < 0) {
        printf("*** shmget error (server) ***\n");
        exit(1);
    }

    shared = (SharedData *) shmat(ShmID, NULL, 0);
    if ((long)shared == -1) {
        printf("*** shmat error (server) ***\n");
        exit(1);
    }

    shared->bankAccount = 0;
    if (sem_init(&shared->mutex, 1, 1) < 0) {
        printf("*** sem_init error ***\n");
        exit(1);
    }

    // Fork Lovable Mom if mode == 2
    if (parent_mode == 2) {
        pid_t mom_pid = fork();
        if (mom_pid < 0) {
            printf("*** fork error (mom) ***\n");
            exit(1);
        } else if (mom_pid == 0) {
            MomProcess(shared);
            exit(0);
        }
    }

    // Fork Dear Old Dad
    pid_t dad_pid = fork();
    if (dad_pid < 0) {
        printf("*** fork error (dad) ***\n");
        exit(1);
    } else if (dad_pid == 0) {
        DadProcess(shared);
        exit(0);
    }

    // Fork N Poor Students
    for (int i = 0; i < num_students; i++) {
        pid_t student_pid = fork();
        if (student_pid < 0) {
            printf("*** fork error (student) ***\n");
            exit(1);
        } else if (student_pid == 0) {
            StudentProcess(shared, i);
            exit(0);
        }
    }

    // Parent process keeps running and waiting until killed
    while(1) {
        pause();
    }

    return 0;
}