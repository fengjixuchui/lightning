#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <cstdio>

#define CAPACITY 65536
#define NBUCKETS 256


class Lightning
{
    private:
        key_t key; // system v ipc key
        int shmid; // id of shm segment
        void *ptr_start; // pointer to start of shm segment

        typedef struct node
        {
            char *key;
            char *value;
            struct node *next = nullptr;
        }
        node;

        unsigned hash(char *key)
        {
            // returns: index of value associated with key in table array

            unsigned long hash = 5381;
            int c;

            while (0 != (c = *key++))
            {
                hash = ((hash << 5) + hash) + c;
            }

            return hash % NBUCKETS;
        }

        node** table;

        void* shm_alloc(long sz)
        {
            // returns: pointer to chunk of shm with size sz
            // TODO: optimize with sephamores (atomic current chunk pointer)

            char *t_ptr = (char*) ptr_start;

            while (true)
            {
                while (*t_ptr != -1)
                {
                    t_ptr += 1;
                }

                for (int i = 0; i < sz; i++)
                {
                    if (*(t_ptr + i) != -1)
                    {
                        t_ptr += i;
                        continue;
                    }
                }

                memset(t_ptr, 0, sz);
                return (void*) t_ptr;
            }
        }

        void* table_alloc()
        {
            memset(ptr_start, 0, NBUCKETS * sizeof(node*));
            return ptr_start;
        }

    public:
        Lightning()
        {
            // default path: current directory
            // default id: 'a'

            key = ftok(".", 'a');
        }

        Lightning(const char *path, int id)
        {
            // alternate constructor allowing user to specify path, id

            key = ftok(path, id);
        }


        void* init(bool creat_new)
        {
            // allocates new shm segment if creat_new is specified, then maps it into memory
            // otherwise, maps existing segment into memory
            // returns: pointer to start of shm segment in memory
            // TODO: replace creat_new with flags

            int shmflag = creat_new ? (0666 | IPC_CREAT) : 0;
            shmid = shmget(key, CAPACITY, shmflag);
            ptr_start = shmat(shmid, NULL, 0);
            if (creat_new)
            {
                memset(ptr_start, -1, CAPACITY);
                table = (node**) table_alloc();
            }
            else
            {
                table = (node**) ptr_start;
            }

            return ptr_start;
        }

        void close()
        {
            // TODO: use semaphores to track # of open instances, destroy if 0
            shmdt(ptr_start);
            shmctl(shmid, IPC_RMID, NULL);
        }

        void set(char *key, char *value)
        {
            // sets value in table
            // TODO: fix race conditions with semaphore

            unsigned h = hash(key);
            node *c_node;
            if (table[h] == nullptr)
            {
                table[h] = (node*) shm_alloc(sizeof(node));
                c_node = table[h];
            }
            else
            {
                while (c_node->next != nullptr)
                {
                    c_node = c_node->next;
                }
                c_node->next = (node*) shm_alloc(sizeof(node));
                c_node = c_node->next;
            }

            c_node->key = (char*) shm_alloc(strlen(key));
            strcpy(c_node->key, key);
            c_node->value = (char*) shm_alloc(strlen(value));
            strcpy(c_node->value, value);
            c_node->next = nullptr;

            printf("cnode_offset: %p\n", c_node - (node *) ptr_start);
        }

        char* get(char *key)
        {
            // returns: pointer to value associated with key in shm table

            node *c_node = table[hash(key)];
            while (strcmp(key, c_node->key) != 0)
            {
                c_node = c_node->next;
            }

            return c_node->value;
        }
};
