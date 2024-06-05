#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <memory>
#include <signal.h>
#include <sys/time.h>
#include <setjmp.h>
#include <unistd.h>
#include <vector>
#include <queue>
#include <cassert>
#include "uthreads.h"
#include <unordered_map>

#define MAX_THREAD_NUM 100 /* maximal number of threads */
#define STACK_SIZE 4096 /* stack size per thread (in bytes) */
typedef void (*thread_entry_point)(void);

#define SECOND 1000000
#define FAILURE_RETURN_VALUE (-1)
#define SUCCESS_RETURN_VALUE 0
#define NOT_SLEEPING (-1)

#define FULL_THREADS (-1)

#define JB_SP 6
#define JB_PC 7

//
#define SYSTEM_ERROR_PREFIX "system error: "
#define LIBRARY_ERROR_PREFIX "thread library error: "

//
#define QUANTUM_ERROR_MSG "quantum must be positive value in ms."
#define MEMORY_ERROR_MSG "failed to allocate memory."
#define SIGACTION_ERROR_MSG "sigaction error."
#define SETITIMER_ERROR_MSG "setitimer error."
#define ENTRY_POINT_ERROR_MSG "entry point can't be null."
#define FULL_THREADS_ERROR_MSG "Can't create more threads."
#define INVALID_TID_ERROR_MSG "no thread with ID tid exists."
#define SIGEMPTYSET_ERROR_MSG "sigemptyset error."
#define BOLCK_ERROR_MSG "illegal id."
#define RESUME_ERROR_MSG " can't resume running or blocked thread"
typedef void (*thread_entry_point)(void);

typedef unsigned long address_t;

//TODO need 32 and 64???
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
        "rol    $0x11,%0\n"
        : "=g" (ret)
        : "0" (addr));
    return ret;
}

/**** THREAD STRUCT ****/

// TODO
typedef enum
{
    RUNNING, READY, BLOCKED
} thread_state;

// TODO
typedef struct Thread
{
    unsigned int id;
    thread_state state;
    char* stack;
    int quantum; // TODO need??
    sigjmp_buf env;
    bool sleeping_time;
} Thread;

/********************* GLOBAL VARIABLES *********************/

//typedef std::vector<Thread *> threads_vector;
//blocked threads
std::unordered_map<int, Thread*> blocked_threads;
// quantom vars
int total_quantom = 0;
int quantom_len;
// all threads
std::vector<Thread*> threads(MAX_THREAD_NUM, nullptr);

// TODO init
std::priority_queue<int, std::vector<int>, std::greater<>> min_heap_id;

std::queue<int> ready_threads;

// TODO init
struct itimerval timer;

int curr_thread_id; // (-1) if null

/********************* FUNCTIONS IMPLEMENTATION *********************/

void init_min_heap_id()
{
    // initialize the id min heap (id from 0 to 99)
    for (int i = 0; i < MAX_THREAD_NUM; ++i)
    {
        min_heap_id.push(i);
    }
}

// TODO need to free stack?
void free_threads()
{
    for (int i = 0; i < MAX_THREAD_NUM; ++i)
    {
        if (threads[i] != nullptr)
            if (threads[i]->stack != nullptr)
            {
                free(threads[i]->stack);
            }
        free(threads[i]);
    }
}

int get_min_available_id()
{
    if (min_heap_id.empty())
    {
        return FULL_THREADS;
    }
    int min_id = min_heap_id.top();
    min_heap_id.pop();
    return min_id;
}
void wake_sleeping_threads()
{
    for(auto pair:blocked_threads)
    {
        Thread *cur_thread = pair.second;
        if (cur_thread->sleeping_time == NOT_SLEEPING)
        {
            // todo: wake up the thread, add it to ready list - need to check if thread was resumed while sleep
        }
    }

}
void set_ready_to_run()
{
    //TODO: we push here the curr_thread to ready. not always good. for exmp where the curr_thread is blocked
    if (threads[curr_thread_id]->state == BLOCKED)
    {
        blocked_threads.emplace(curr_thread_id, threads[curr_thread_id]);
    }
    if (threads[curr_thread_id]->state == RUNNING)
    {
        ready_threads.push(curr_thread_id);
    }

    curr_thread_id = ready_threads.front(); //TODO empty check ?
    threads[curr_thread_id]->state = RUNNING;
    ready_threads.pop();
    threads[curr_thread_id]->quantum++;
    total_quantom++;
    wake_sleeping_threads();
    siglongjmp(threads[curr_thread_id]->env, 1);
}

void timer_handler(int sig)
{
    int ret_val = sigsetjmp(threads[curr_thread_id]->env, 1);
    bool did_just_save_bookmark = ret_val == 0;
    if (did_just_save_bookmark)
    {
        // TODO
        if (curr_thread_id != -1)
        {
            set_ready_to_run();
        }
    }
}

int init_timer()
{
    struct sigaction sa = {0};
    sa.sa_handler = &timer_handler;
    if (sigaction(SIGVTALRM, &sa, nullptr) < 0)
    {
        std::cerr << SYSTEM_ERROR_PREFIX << SIGACTION_ERROR_MSG << std::endl;
        free_threads();
        exit(1);
    }

    // first time interval seconds part
    timer.it_value.tv_sec = quantom_len / SECOND;
    // first time interval, microseconds part
    timer.it_value.tv_usec = quantom_len % SECOND;

    // following time intervals, seconds part
    timer.it_interval.tv_sec = quantom_len / SECOND;
    // following time intervals, microseconds part
    timer.it_interval.tv_usec = quantom_len % SECOND;

    if (setitimer(ITIMER_VIRTUAL, &timer, nullptr))
    {
        std::cerr << SYSTEM_ERROR_PREFIX << SETITIMER_ERROR_MSG << std::endl;
        free_threads();
        exit(1);
    }

    return SUCCESS_RETURN_VALUE;
}

void remove_tid_from_ready_queue(const int& tid)
{
    std::queue<int> temp_queue;
    while (!ready_threads.empty())
    {
        if (ready_threads.front() != tid)
        {
            temp_queue.push(ready_threads.front());
        }
        ready_threads.pop();
    }
    std::swap(ready_threads, temp_queue);
}

int uthread_init(int quantum_usecs)
{
    // Check that quantum_usecs is positive values
    if (quantum_usecs <= 0)
    {
        std::cerr << LIBRARY_ERROR_PREFIX << QUANTUM_ERROR_MSG << std::endl;
        return FAILURE_RETURN_VALUE;
    }


    Thread* main_thread = (Thread*)calloc(1, sizeof(Thread));
    if (main_thread == nullptr)
    {
        std::cerr << SYSTEM_ERROR_PREFIX << MEMORY_ERROR_MSG << std::endl;
        free_threads();
        exit(1);
    }
    quantom_len = quantum_usecs;
    total_quantom = 1;

    // Create the main thread
    init_min_heap_id();
    int min_id = get_min_available_id(); // min_id = 0

    // Initialize the main thread
    main_thread->id = min_id;
    main_thread->state = RUNNING;
    main_thread->quantum = 1;
    main_thread->sleeping_time = NOT_SLEEPING;
    threads[0] = main_thread;
    curr_thread_id = 0;

    // Initialize timer
    init_timer();

    return SUCCESS_RETURN_VALUE;
}

int uthread_spawn(thread_entry_point entry_point)
{
    if (entry_point == nullptr)
    {
        std::cerr << LIBRARY_ERROR_PREFIX << ENTRY_POINT_ERROR_MSG << std::endl;
        return FAILURE_RETURN_VALUE;
    }

    int new_id = get_min_available_id();
    if (new_id == FULL_THREADS)
    {
        std::cerr << LIBRARY_ERROR_PREFIX << FULL_THREADS_ERROR_MSG << std::endl;
        return FAILURE_RETURN_VALUE;
    }

    char* new_stack = (char*)(malloc(STACK_SIZE * sizeof(char)));
    if (new_stack == nullptr)
    {
        std::cerr << SYSTEM_ERROR_PREFIX << MEMORY_ERROR_MSG << std::endl;
        free_threads();
        exit(1);
    }

    Thread* new_thread = (Thread*)malloc(sizeof(Thread));
    if (new_thread == nullptr)
    {
        free(new_stack);
        std::cerr << SYSTEM_ERROR_PREFIX << MEMORY_ERROR_MSG << std::endl;
        free_threads();
        exit(1);
    }

    new_thread->id = new_id;
    new_thread->state = READY;
    new_thread->sleeping_time = NOT_SLEEPING;
    new_thread->stack = new_stack;
    new_thread->quantum = 0;
    address_t sp = (address_t)new_stack + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t)entry_point;

    sigsetjmp(new_thread->env, 1);
    (new_thread->env->__jmpbuf)[JB_SP] = translate_address(sp);
    (new_thread->env->__jmpbuf)[JB_PC] = translate_address(pc);

    if (sigemptyset(&new_thread->env->__saved_mask) == FAILURE_RETURN_VALUE)
    {
        // TODO forum question
        std::cerr << SYSTEM_ERROR_PREFIX << SIGEMPTYSET_ERROR_MSG << std::endl;
        free_threads();
        exit(1);
    }
    assert(threads[new_id] == nullptr);
    threads[new_id] = new_thread;
    ready_threads.push(new_id);

    return new_id;
}

int uthread_terminate(int tid)
{
    // check tid validation
    if (tid >= MAX_THREAD_NUM || threads[tid] == nullptr)
    {
        std::cerr << LIBRARY_ERROR_PREFIX << INVALID_TID_ERROR_MSG << std::endl;
        return FAILURE_RETURN_VALUE;
    }

    // check if it's the main thread
    // TODO err msg need??
    if (tid == 0)
    {
        free_threads();
        exit(0);
    }

    if (threads[tid]->state == READY)
    {
        remove_tid_from_ready_queue(tid);
    }

    if (threads[tid]->state == RUNNING) // TODO can check with curr_thread_id too
    {
        set_ready_to_run();
        init_timer();
    }
    // if the thread was blocked remove it from the blocked map
    auto check_blocked = blocked_threads.find(tid);
    if (check_blocked != blocked_threads.end())
    {
        blocked_threads.erase(check_blocked);
    }

    // Terminates the thread with ID tid and deletes it from all structures
    free(threads[tid]->stack);
    threads[tid]->stack = nullptr;
    free(threads[tid]);
    threads[tid] = nullptr;

    //
    min_heap_id.push(tid);
    return SUCCESS_RETURN_VALUE;
}

int uthread_block(int tid)
{
    // if the id is not legal, the id is empty, or try to block main thread, return error.
    bool legal_id = 0 < tid && tid < MAX_THREAD_NUM && threads[tid] != nullptr;
    if (!legal_id)
    {
        std::cerr << LIBRARY_ERROR_PREFIX << INVALID_TID_ERROR_MSG << std::endl;
        return EXIT_FAILURE;
    }
    // if the thread is already block, no action is needed
    if (threads[tid]->state == BLOCKED)
    {
        return SUCCESS_RETURN_VALUE;
    }
    // make the ready thread run, reset the timer.
    if (threads[tid]->state == RUNNING)
    {
        threads[tid]->state == BLOCKED;
        set_ready_to_run();
        init_timer();
        return SUCCESS_RETURN_VALUE;
    }
    // if the thread was ready remove it from the ready list.
    if (threads[tid]->state == READY)
    {
        remove_tid_from_ready_queue(tid);
    }
    // this section is the same for sleeping and ready
    threads[tid]->state == BLOCKED;
    blocked_threads.emplace(tid, threads[tid]);
    //  todo: case where sleep?
    return SUCCESS_RETURN_VALUE;
}

int uthread_resume(int tid)
{
    // if the id is not legal, the id is empty, or try to block main thread, return error.
    bool legal_id = 0 < tid && tid < MAX_THREAD_NUM && threads[tid] != nullptr;
    if (!legal_id)
    {
        std::cerr << LIBRARY_ERROR_PREFIX << INVALID_TID_ERROR_MSG << std::endl;
        return EXIT_FAILURE;
    }
        if (threads[tid]->state == RUNNING || threads[tid]->state == READY)
    {
        return SUCCESS_RETURN_VALUE;
    }
    assert(
        (threads[tid]-> state == BLOCKED && blocked_threads.find(tid) != blocked_threads.end()) || (threads[tid]-> sleeping_time
            != NOT_SLEEPING));
    if (threads[tid]->state == BLOCKED)
    {
        threads[tid]->state == READY;
        ready_threads.push(tid);
        // todo:finish

    }

}

//TODO: if two threads wake up in the same time there is need to mask the other signals.

int uthread_sleep(int num_quantums)
{
}

int uthread_get_tid()
{
    return curr_thread_id;
}

int uthread_get_total_quantums()
{
    return total_quantom;
}

int uthread_get_quantums(int tid)
{
}
