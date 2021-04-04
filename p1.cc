#include <iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>

int main()
{
    key_t key = ftok(".", 'a');
    printf("%i\n", key);

    // shmget returns an identifier in shmid
    int shmid = shmget(key, 1024, 0666 | IPC_CREAT);

    // shmat to attach to shared memory
    char *str = (char*) shmat(shmid, NULL, 0);

    printf("%p\n", str);
    std::cout<<"Write Data : ";
    fgets(str, 1024, stdin);

    printf("Data written in memory: %s\n", str);

    //detach from shared memory
    shmdt(str);

    return 0;
}
