#include "lightning.hh"
#include <stdio.h>
#include <stdlib.h>

#define BUFSIZE 64

int main()
{
    Lightning lightning;

    lightning.init();

    while (true)
    {
        char input[BUFSIZE];
        fgets(input, BUFSIZE, stdin);
        input[strcspn(input, "\n")] = 0;

        if (input[0] == 'd')
        {
            lightning.detach();
            printf("-> detached from lightning\n");
            exit(0);
        }
        else if (input[0] == 'c')
        {
            lightning.close();
            printf("-> closed lightning\n");
            exit(0);
        }

        char *command = strtok(input, " ");

        if (command[0] == 's')
        {
            char *key = strtok(NULL, " ");
            char *value = strtok(NULL, " ");
            lightning.set(key, value);
            printf("set key %s to %s\n", key, value);
        }
        else if (command[0] == 'g')
        {
            char *key = strtok(NULL, " ");
            char *value = lightning.get(key);
            if (value)
            {
                printf("key %s: %s\n", key, value);
            }
            else
            {
                printf("key %s does not exist\n", key);
            }
        }
        else if (command[0] == 'r')
        {
            char *key = strtok(NULL, " ");
            int err = lightning.rm(key);
            if (err == 0)
            {
                printf("removed key %s\n", key);
            }
            else
            {
                printf("key %s does not exist\n", key);
            }
        }
    }

    return 0;
}
