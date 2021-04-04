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

    lightning.close();

    return 0;
}
