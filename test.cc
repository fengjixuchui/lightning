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
