//
// Created by omer1 on 06/06/2024.
//

#include "tests_fun.h"
#include "uthreads.h"

#include <iostream>

void f(void)
{
    std::cout << uthread_get_tid << std::endl;
}

int main(void)
{
    uthread_init(1000);


    uthread_spawn(f);
    uthread_spawn(f);

    uthread_terminate(1);
    uthread_terminate(2);

    uthread_spawn(f);
    uthread_spawn(f);
    uthread_spawn(f);

    uthread_terminate(2);
    uthread_spawn(f);
}
