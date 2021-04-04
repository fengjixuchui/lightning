
/*
*    .__  .__       .__     __         .__
*    |  | |__| ____ |  |___/  |_  ____ |__| ____    ____
*    |  | |  |/ ___\|  |  \   __\/    \|  |/    \  / ___\
*    |  |_|  / /_/  >   Y  \  | |   |  \  |   |  \/ /_/  >
*    |____/__\___  /|___|  /__| |___|  /__|___|  /\___  /
*           /_____/      \/          \/        \//_____/
*
*          lightning fast IPC in a single header
*                  written by null333
*/

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
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
            // TODO: optimize with sephamores (atomic current chunk pointer)
            // TODO: return nullptr when out of memory instead of blocking

            char *t_ptr = (char*) ptr_next;

            while (true)
            {
                if (t_ptr > ptr_start + CAPACITY)
                {
                    t_ptr = (char *) ptr_start;
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

        void* table_alloc()
        {
            memset(ptr_start, 0, NBUCKETS * sizeof(node*));
            return ptr_start;
        }

        inline void *off_to_ptr(void *off)
        {
            // returns: pointer to object at off
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


        void* init(bool creat_new)
        {
            // allocates new shm segment if creat_new is specified, then maps it into memory
            // otherwise, maps existing segment into memory
            // returns: pointer to start of shm segment in memory
            // TODO: replace creat_new with flags
            // TODO: reread documentation for __builtin_prefetch

            int shmflag = creat_new ? (0666 | IPC_CREAT) : 0;
            shmid = shmget(key, CAPACITY, shmflag);
            ptr_start = shmat(shmid, NULL, 0);
            __builtin_prefetch((const void*) ptr_start);
            ptr_next = ptr_start;

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
            // unmaps and closes shm
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
        }

        char* get(char *key)
        {
            // returns: pointer to value associated with key in shm table

            node *c_node = (node*) off_to_ptr(table[hash(key)]);
            while (strcmp(key, (char*) off_to_ptr(c_node->key_off)) != 0)
            {
                c_node = (node*) off_to_ptr(c_node->next_off);
            }

            return (char*) off_to_ptr(c_node->value_off);
        }

        void rm(char *key)
        {
            // removes key from table and frees its contents

            node *c_node = (node*) off_to_ptr(table[hash(key)]);
            node *prev_node = nullptr;
            node *next_node_off = nullptr;
            while (strcmp(key, (char*) off_to_ptr(c_node->key_off)) != 0)
            {
                prev_node = c_node;
                c_node = (node*) off_to_ptr(c_node->next_off);
            }
            next_node_off = c_node->next_off;

            if (prev_node && next_node_off)
            {
                prev_node->next_off = next_node_off;
            }

            memset(off_to_ptr(c_node->key_off), -1, strlen((char *) off_to_ptr(c_node->key_off)));
            memset(off_to_ptr(c_node->value_off), -1, strlen((char *) off_to_ptr(c_node->value_off)));
            memset(&c_node, -1, sizeof(c_node));
        }
};
