# Lightning
### Lightning Fast Shared Memory Key-Value Store
Lightning is an easy to use single-header library that allows for fast IPC through a shared memory key-value store.

## Installation:
Move lightning.hh to your project directory and add ``#include "lightning.hh"`` at the top of files where you will use it.

Then, compile with ``-pthread``.


## Usage:
``lightning.init()`` Maps the store into the memory of the calling process. (also creates the store if it does not already exist)

``lightning.set(char *key, char *value)`` sets "key" to "value" in the store.

``lightning.get(char *key)`` Returns a pointer to the value associated with key in shared memory.

``lightning.rm(char *key)`` Removes and frees the entry associated with key.

``lightning.detach()`` Unmaps the shared memory from the calling processes memory.

``lightning.close`` Unmaps the shared memory from the calling processes memory, then destorys the shared memory.


## Example:
``programA.cc``
```c++
#include "lightning.hh"
#include <iostream>
#include <stdio.h>

int main()
{
    Lightning lightning;

    lightning.init();

    lightning.set("test_key", "test_value");
    printf("set test_key: test_value\n");

    lightning.detach();

    return 0;
}
```
``programB.cc``
```c++
#include "lightning.hh"
#include <iostream>
#include <stdio.h>

int main()
{
    Lightning lightning;

    lightning.init();

    char *value = lightning.get("test_key");
    printf("get test_key: %s\n", value);

    lightning.rm("test_key");
    printf("removed test_key\n");

    lightning.close();

    return 0;
}
```
