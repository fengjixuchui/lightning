/*
*    .__  .__       .__     __         .__
*    |  | |__| ____ |  |___/  |_  ____ |__| ____    ____
*    |  | |  |/ ___\|  |  \   __\/    \|  |/    \  / ___\
*    |  |_|  / /_/  >   Y  \  | |   |  \  |   |  \/ /_/  >
*    |____/__\___  /|___|  /__| |___|  /__|___|  /\___  /
*           /_____/      \/          \/        \//_____/
*
*          lightning fast key-value store in a single header
*                      written by null333
*/

#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <sys/types.h>
#include <string.h>
#include <cstdint>

#define CAPACITY 65536
#define NBUCKETS 256


class Lightning
{
    private:
        key_t key; // system v ipc key
        int shmid; // id of shm segment
        void *ptr_start; // pointer to start of shm segment
        void *ptr_next; // pointer to next free byte in shm
        sem_t *mut; // semaphore used to lock/unlock table and keep track of processes using shm

        struct node
        {
            char *key_off; // offset of key
            char *value_off; // offset of value
            struct node *next_off = nullptr; // offset of next node
        };

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
            // TODO: return nullptr when out of memory instead of blocking

            char *t_ptr = (char*) ptr_next;

            while (true)
            {
                if (t_ptr > (char*) ptr_start + CAPACITY)
                {
                    t_ptr = (char*) ptr_start;
                }

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
                ptr_next = t_ptr + sz;
                return (void*) t_ptr;
            }
        }

        inline void *off_to_ptr(void *off)
        {
            // returns: pointer to object at off

            if (off == nullptr)
            {
                // NOTE: nullptr offsets cant be computed normally because they will be computed as ptr_start
                return nullptr;
            }
            return (void*)((uintptr_t) off + (uintptr_t) ptr_start);
        }

        inline void *ptr_to_off(void *ptr)
        {
            // returns: offset of object at ptr
            return (void*)((uintptr_t) ptr - (uintptr_t) ptr_start);
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

        void init()
        {
            // allocates new shm segment if not created yet, then maps it into memory
            // otherwise, maps existing segment into memory
            // returns: pointer to start of shm segment in memory

            bool creat_new = true;
            shmid = shmget(key, CAPACITY, (0666 | IPC_CREAT | IPC_EXCL));
            if (shmid == -1)
            {
                shmid = shmget(key, CAPACITY, 0);
                creat_new = false;
            }
            ptr_start = shmat(shmid, NULL, 0);
            __builtin_prefetch((const void*) ptr_start);
            ptr_next = ptr_start;

            table = (node**) ptr_start;
            mut = (sem_t *)((uintptr_t) table + (uintptr_t) NBUCKETS * sizeof(node*));

            if (creat_new)
            {
                memset(ptr_start, -1, CAPACITY);
                memset(ptr_start, 0, NBUCKETS * sizeof(node*) + sizeof(sem_t));
                sem_init(mut, 1, 1);
            }

        }

        void detach()
        {
            // unmaps shm

            shmdt(ptr_start);
        }

        void close()
        {
            // unmaps and closes shm

            sem_destroy(mut);
            shmdt(ptr_start);
            shmctl(shmid, IPC_RMID, NULL);
        }

        void set(char *key, char *value)
        {
            // sets value in table

            unsigned h = hash(key);
            node *c_node;

            sem_wait(mut);

            if (table[h] == nullptr)
            {
                table[h] = (node*) ptr_to_off((node*) shm_alloc(sizeof(node)));
                c_node = (node*) off_to_ptr(table[h]);
            }
            else
            {
                while (off_to_ptr(c_node->next_off) != nullptr)
                {
                    c_node = (node*) off_to_ptr(c_node->next_off);
                }
                c_node->next_off = (node*) ptr_to_off((node*) shm_alloc(sizeof(node)));
                c_node = (node*) off_to_ptr(c_node->next_off);
            }

            c_node->key_off = (char*) ptr_to_off(shm_alloc(strlen(key)));
            strcpy((char*) off_to_ptr(c_node->key_off), key);
            c_node->value_off = (char*) ptr_to_off(shm_alloc(strlen(value)));
            strcpy((char*) off_to_ptr(c_node->value_off), value);
            c_node->next_off = nullptr;

            sem_post(mut);
        }

        char* get(char *key)
        {
            // returns: pointer to value associated with key in shm table, nullptr if key doesnt exist

            node *c_node = (node*) off_to_ptr(table[hash(key)]);
            if (c_node == nullptr)
            {
                return nullptr;
            }
            while (strcmp(key, (char*) off_to_ptr(c_node->key_off)) != 0)
            {
                if (c_node == nullptr)
                {
                    return nullptr;
                }
                c_node = (node*) off_to_ptr(c_node->next_off);
            }

            return (char*) off_to_ptr(c_node->value_off);
        }

        int rm(char *key)
        {
            // removes key from table and frees its contents, nullptr if key doesnt exist

            node *c_node = (node*) off_to_ptr(table[hash(key)]);
            if (c_node == nullptr)
            {
                return -1;
            }
            node *prev_node = nullptr;
            node *next_node_off = nullptr;
            while (strcmp(key, (char*) off_to_ptr(c_node->key_off)) != 0)
            {
                if (c_node == nullptr)
                {
                    return -1;
                }
                prev_node = c_node;
                c_node = (node*) off_to_ptr(c_node->next_off);
            }
            next_node_off = c_node->next_off;

            if (prev_node && next_node_off)
            {
                prev_node->next_off = next_node_off;
            }

            sem_wait(mut);

            memset(off_to_ptr(c_node->key_off), -1, strlen((char *) off_to_ptr(c_node->key_off)));
            memset(off_to_ptr(c_node->value_off), -1, strlen((char *) off_to_ptr(c_node->value_off)));
            memset(&c_node, -1, sizeof(c_node));

            table[hash(key)] = nullptr;

            sem_post(mut);

            return 0;
        }
};
