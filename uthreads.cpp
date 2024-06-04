//
// Created by Shalev Shitrit on 02/06/2024.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <sys/time.h>
#include <setjmp.h>
#include <unistd.h>

#include "uthreads.h"

typedef enum
{
    RUNNING, READY, BLOCKED, SLEEPING
} thread_state;

struct thread
{
    unsigned int tid,
    thread_state state,
    thread_entry_point entry_point,
    stack_t stack,
    int quantum
};

int uthread_init (int quantum_usecs)
{

}

int uthread_spawn (thread_entry_point entry_point)
{

}

int uthread_terminate (int tid)
{

}

int uthread_block (int tid)
{

}

int uthread_resume (int tid)
{

}

int uthread_sleep (int num_quantums)
{

}
int uthread_get_tid ()
{

}
int uthread_get_total_quantums ()
{

}
int uthread_get_quantums (int tid)
{

}