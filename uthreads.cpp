/*
 * User-Level Threads Library (uthreads).
 * Hebrew University OS course.
 * Authors: Omer Dahan, Shalev Shitrit.
 */

/********************************* INCLUDES *********************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <memory>
#include <signal.h>
#include <sys/time.h>
#include <setjmp.h>
#include <vector>
#include <queue>
#include <unordered_set>

#include "uthreads.h"

/*************************** ADDRESS TRANSLATION ****************************/

#ifdef __x86_64__
/* Code for 64-bit Intel architecture */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
        "rol    $0x11,%0\n"
        : "=g" (ret)
        : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}

#endif

/********************************** MACROS **********************************/

#define MAX_THREAD_NUM 100 /* Maximum number of threads */
#define STACK_SIZE 4096 /* Stack size per thread (in bytes) */

#define SECOND 1000000 /* One second in microseconds */
#define SUCCESS_RETURN_VALUE 0
#define FAILURE_RETURN_VALUE (-1)
#define RUNNING_TERMINATED (-1)
#define NOT_SLEEPING (-1)
#define FULL_THREADS (-1)

#define SYSTEM_ERROR_PREFIX "system error: "
#define LIBRARY_ERROR_PREFIX "thread library error: "

/* System errors messages */
#define MEMORY_ERROR_MSG "Memory allocation failed."
#define SIGACTION_ERROR_MSG "Error in sigaction."
#define SETITIMER_ERROR_MSG "Error in setitimer."
#define SIGEMPTYSET_ERROR_MSG "Error in sigemptyset."
#define SIGADDSET_ERROR_MSG "Error in sigaddset."
#define SIGPROCMASK_ERROR_MSG "Error in sigprocmask."

/* Thread library errors messages */
#define QUANTUM_ERROR_MSG "Quantum must be a positive value (ms)."
#define ENTRY_POINT_ERROR_MSG "Entry point cannot be null."
#define FULL_THREADS_ERROR_MSG "Cannot create more threads."
#define INVALID_TID_ERROR_MSG "Thread ID does not exist."
#define SLEEP_MT_ERROR_MSG "Main thread can't sleep itself."

/****************************** THREAD STRUCT *******************************/

/**
 * @typedef thread_entry_point
 * @brief Defines a function pointer type for thread entry points.
 *
 * This type represents the entry point function for a thread.
 * The function should have no parameters and return void.
 */
typedef void (*thread_entry_point)(void);

/**
 * @enum thread_state
 * @brief Possible thread states.
 */
typedef enum
{
    RUNNING, READY, BLOCKED, SLEEPING
} thread_state;

/**
 * @struct Thread
 * @brief Represents a thread.
 * @var id Thread ID
 * @var state Current state of the thread
 * @var stack Pointer to the thread's stack
 * @var quantum Number of quantums this thread has run
 * @var env Environment buffer for context switching
 * @var sleeping_time Number of quantums the thread will sleep
 */
typedef struct Thread
{
    unsigned int id;
    thread_state state;
    char* stack;
    int quantum;
    sigjmp_buf env;
    int sleeping_time;
} Thread;

/***************************** GLOBAL VARIABLES *****************************/

/* Array of all threads */
std::vector<Thread*> threads(MAX_THREAD_NUM, nullptr);

/* Map of sleeping threads by ID */
//TODO convert to vector (only ID)
std::unordered_set<int> sleeping_threads;

/* Min-heap to manage available thread IDs */
std::priority_queue<int, std::vector<int>, std::greater<int>> min_heap_id;

/* Queue of ready threads */
std::queue<int> ready_threads;

/* Set of signals */
sigset_t signal_set;

/* Timer structure */
struct itimerval timer;

/* Total number of quantums elapsed */
int total_quantom = 0;

/* Length of a quantum in microseconds */
int quantom_len;

/* ID of the currently running thread */
int curr_thread_id;

/************************* FUNCTIONS IMPLEMENTATION *************************/

/**
 * @brief Initialize the timer.
 * @return SUCCESS_RETURN_VALUE on success, FAILURE_RETURN_VALUE on failure.
 */
int init_timer();

void
set_new_thread(thread_entry_point entry_point, int new_id, char* new_stack, Thread* new_thread);
/**
 * @brief Initialize the min-heap of available thread IDs.
 */
void init_min_heap_id()
{
    // Initialize the ID min-heap (ID from 0 to 99)
    for (int i = 0; i < MAX_THREAD_NUM; ++i)
    {
        min_heap_id.push(i);
    }
}

/**
 * @brief Initialize the signal set for SIGVTALRM.
 */
void init_sig_set()
{
    if (sigemptyset(&signal_set) == FAILURE_RETURN_VALUE)
    {
        std::cerr << SYSTEM_ERROR_PREFIX << SIGEMPTYSET_ERROR_MSG << std::endl;
        exit(1);
    }
    if (sigaddset(&signal_set, SIGVTALRM) == FAILURE_RETURN_VALUE)
    {
        std::cerr << SYSTEM_ERROR_PREFIX << SIGADDSET_ERROR_MSG << std::endl;
        exit(1);
    }
}

/**
 * @brief Block the timer signal.
 */
void mask_timer()
{
    if (sigprocmask(SIG_SETMASK, &signal_set, nullptr) == -1)
    {
        std::cerr << SYSTEM_ERROR_PREFIX << SIGPROCMASK_ERROR_MSG << std::endl;
        exit(1);
    }
}

/**
 * @brief Unblock the timer signal.
 */
void unmask_timer()
{
    if (sigprocmask(SIG_UNBLOCK, &signal_set, nullptr) == -1)
    {
        std::cerr << SYSTEM_ERROR_PREFIX << SIGPROCMASK_ERROR_MSG << std::endl;
        exit(1);
    }
}

/**
 * @brief Free all allocated thread resources.
 */
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

/**
 * @brief Get the minimum available thread ID.
 * @return Minimum available thread ID, or FULL_THREADS if no available IDs.
 */
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

/**
 * @brief Wake up sleeping threads whose sleep time has elapsed.
 */
void wake_sleeping_threads()
{
    std::vector<int> to_delete;
    for (int index : sleeping_threads)
    {
        Thread* cur_thread = threads[index];
        cur_thread->sleeping_time--;
        if (cur_thread->sleeping_time == 0)
        {
            cur_thread->sleeping_time = NOT_SLEEPING;
            to_delete.push_back(index);
            if (cur_thread->state == SLEEPING) // Make sure it's not blocked
            {
                cur_thread->state = READY;
                ready_threads.push(cur_thread->id);
            }
        }
    }
    for (auto index : to_delete)
    {
        sleeping_threads.erase(sleeping_threads.find(index));
    }
}

/**
 * @brief Set the next ready thread to running.
 */
void set_ready_to_run()
{
    // If the current thread is still running and has not been terminated
    if (curr_thread_id != RUNNING_TERMINATED
        && threads[curr_thread_id]->state == RUNNING)
    {
        ready_threads.push(curr_thread_id);
        threads[curr_thread_id]->state = READY;
    }
    curr_thread_id = ready_threads.front(); // Main is never sleep or blocked
    threads[curr_thread_id]->state = RUNNING;
    ready_threads.pop();
    threads[curr_thread_id]->quantum++;
    total_quantom++;
    wake_sleeping_threads();
    init_timer(); // Reinitialize the timer
    siglongjmp(threads[curr_thread_id]->env, 1); // Switch to the new thread
}

/**
 * @brief Handler for the timer signal.
 * @param sig Signal number
 */
void timer_handler(int sig)
{
    int ret_val = sigsetjmp(threads[curr_thread_id]->env, 1);
    bool did_just_save_bookmark = ret_val == 0;
    if (did_just_save_bookmark)
    {
        // Check if the current thread is valid
        set_ready_to_run();
    }
}

/**
 * @brief Initialize the timer.
 * @return SUCCESS_RETURN_VALUE on success, FAILURE_RETURN_VALUE on failure.
 */
int init_timer()
{
    struct sigaction sa = {0};
    sa.sa_handler = &timer_handler; // Set the timer handler
    if (sigaction(SIGVTALRM, &sa, nullptr) < 0)
    {
        std::cerr << SYSTEM_ERROR_PREFIX << SIGACTION_ERROR_MSG << std::endl;
        free_threads();
        exit(1);
    }

    // Set timer intervals
    timer.it_value.tv_sec = quantom_len / SECOND;
    timer.it_value.tv_usec = quantom_len % SECOND;
    timer.it_interval.tv_sec = quantom_len / SECOND;
    timer.it_interval.tv_usec = quantom_len % SECOND;

    if (setitimer(ITIMER_VIRTUAL, &timer, nullptr))
    {
        std::cerr << SYSTEM_ERROR_PREFIX << SETITIMER_ERROR_MSG << std::endl;
        free_threads();
        exit(1);
    }

    return SUCCESS_RETURN_VALUE;
}

/**
 * @brief Remove a thread ID from the ready queue.
 * @param tid Thread ID to be removed
 */
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

/**
 * @brief Initialize a new thread with the given parameters.
 *
 * This function sets up a new thread by initializing its attributes such as ID, state,
 * stack pointer, program counter, and environment buffer for context switching.
 *
 * @param entry_point Function pointer representing the thread's entry point.
 * @param new_id Integer representing the new thread's ID.
 * @param new_stack Pointer to the allocated stack memory for the new thread.
 * @param new_thread Pointer to the Thread structure to be initialized.
 */
void set_new_thread(thread_entry_point entry_point, int new_id,
                    char* new_stack, Thread* new_thread)
{
    new_thread->id = new_id;
    new_thread->state = READY;
    new_thread->sleeping_time = NOT_SLEEPING;
    new_thread->stack = new_stack;
    new_thread->quantum = 0;
    address_t sp = (address_t)new_stack + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t)entry_point;

    // Save the current state, including the stack pointer and program counter
    sigsetjmp(new_thread->env, 1);
    (new_thread->env->__jmpbuf)[JB_SP] = translate_address(sp);
    (new_thread->env->__jmpbuf)[JB_PC] = translate_address(pc);

    // Initialize the signal mask for the new thread
    if (sigemptyset(&new_thread->env->__saved_mask) == FAILURE_RETURN_VALUE)
    {
        std::cerr << SYSTEM_ERROR_PREFIX << SIGEMPTYSET_ERROR_MSG << std::endl;
        free_threads();
        unmask_timer();
        exit(1);
    }
}

/**
 * @brief Initialize the threading library.
 * @param quantum_usecs Length of each quantum in microseconds
 * @return SUCCESS_RETURN_VALUE on success, FAILURE_RETURN_VALUE on failure.
 */
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
    // initialize signal set
    init_sig_set();

    return SUCCESS_RETURN_VALUE;
}

/**
 * @brief Spawn a new thread.
 * @param entry_point Entry point function of the new thread
 * @return Thread ID of the new thread, or FAILURE_RETURN_VALUE on failure.
 */
int uthread_spawn(thread_entry_point entry_point)
{
    mask_timer();

    if (entry_point == nullptr)
    {
        std::cerr << LIBRARY_ERROR_PREFIX << ENTRY_POINT_ERROR_MSG << std::endl;
        unmask_timer();
        return FAILURE_RETURN_VALUE;
    }

    int new_id = get_min_available_id();
    if (new_id == FULL_THREADS)
    {
        std::cerr << LIBRARY_ERROR_PREFIX << FULL_THREADS_ERROR_MSG << std::endl;
        unmask_timer();
        return FAILURE_RETURN_VALUE;
    }

    char* new_stack = (char*)(malloc(STACK_SIZE * sizeof(char)));
    if (new_stack == nullptr)
    {
        std::cerr << SYSTEM_ERROR_PREFIX << MEMORY_ERROR_MSG << std::endl;
        free_threads();
        unmask_timer();
        exit(1);
    }

    Thread* new_thread = (Thread*)malloc(sizeof(Thread));
    if (new_thread == nullptr)
    {
        free(new_stack);
        std::cerr << SYSTEM_ERROR_PREFIX << MEMORY_ERROR_MSG << std::endl;
        free_threads();
        unmask_timer();
        exit(1);
    }

    set_new_thread(entry_point, new_id, new_stack, new_thread);
    threads[new_id] = new_thread;
    ready_threads.push(new_id);
    unmask_timer();

    return new_id;
}

/**
 * @brief Terminate a thread.
 * @param tid Thread ID of the thread to terminate
 * @return SUCCESS_RETURN_VALUE on success, FAILURE_RETURN_VALUE on failure.
 */
int uthread_terminate(int tid)
{
    mask_timer();

    // Check if tid is valid
    if (tid >= MAX_THREAD_NUM || tid < 0 || threads[tid] == nullptr)
    {
        std::cerr << LIBRARY_ERROR_PREFIX << INVALID_TID_ERROR_MSG << std::endl;
        unmask_timer();
        return FAILURE_RETURN_VALUE;
    }

    // Check if it's the main thread
    if (tid == 0)
    {
        free_threads();
        unmask_timer();
        exit(0);
    }

    // Terminate READY or BLOCKED thread
    if (threads[tid]->state == READY || threads[tid]->state == BLOCKED)
    {
        free(threads[tid]->stack);
        threads[tid]->stack = nullptr;
        free(threads[tid]);
        threads[tid] = nullptr;
        min_heap_id.push(tid);
        remove_tid_from_ready_queue(tid); // Relevant for READY state.
        unmask_timer();

        return SUCCESS_RETURN_VALUE;
    }

    // Terminate SLEEPING thread
    if (threads[tid]->state == SLEEPING)
    {
        sleeping_threads.erase(sleeping_threads.find(tid));
        free(threads[tid]->stack);
        threads[tid]->stack = nullptr;
        free(threads[tid]);
        threads[tid] = nullptr;
        min_heap_id.push(tid);
        unmask_timer();
        return SUCCESS_RETURN_VALUE;
    }

    // Terminate RUNNING thread
    if (threads[tid]->state == RUNNING)
    {
        free(threads[tid]->stack);
        threads[tid]->stack = nullptr;
        free(threads[tid]);
        threads[tid] = nullptr;
        min_heap_id.push(tid);
        curr_thread_id = RUNNING_TERMINATED;
        set_ready_to_run();
    }

    unmask_timer();
    return SUCCESS_RETURN_VALUE;
}

/**
 * @brief Block a thread.
 * @param tid Thread ID of the thread to block
 * @return SUCCESS_RETURN_VALUE on success, FAILURE_RETURN_VALUE on failure.
 */
int uthread_block(int tid)
{
    mask_timer();

    bool legal_id = 0 < tid && tid < MAX_THREAD_NUM && threads[tid] != nullptr;
    if (!legal_id)
    {
        std::cerr << LIBRARY_ERROR_PREFIX << INVALID_TID_ERROR_MSG << std::endl;
        unmask_timer();
        return FAILURE_RETURN_VALUE;
    }

    // Handle blocked and not sleeping thread
    if (threads[tid]->state == BLOCKED)
    {
        unmask_timer();
        return SUCCESS_RETURN_VALUE;
    }

    // Handle blocked and sleeping thread
    if (threads[tid]->state == SLEEPING)
    {
        threads[tid]->state = BLOCKED;
        unmask_timer();
        return SUCCESS_RETURN_VALUE;
    }

    // Handle running thread
    if (threads[tid]->state == RUNNING)
    {
        threads[tid]->state = BLOCKED;
        int ret_val = sigsetjmp(threads[curr_thread_id]->env, 1);
        //TODO: test that a running that blocks itself resume from the point it was blocked.
        bool did_just_save_bookmark = ret_val == 0;
        if (did_just_save_bookmark)
        {
            set_ready_to_run();
        }
        unmask_timer();
        return SUCCESS_RETURN_VALUE;
    }

    // Handle ready thread
    if (threads[tid]->state == READY)
    {
        remove_tid_from_ready_queue(tid);
        threads[tid]->state = BLOCKED;
        unmask_timer();
        return SUCCESS_RETURN_VALUE;
    }

    unmask_timer();
    return SUCCESS_RETURN_VALUE;
}

/**
 * @brief Resume a blocked thread.
 * @param tid Thread ID of the thread to resume
 * @return SUCCESS_RETURN_VALUE on success, FAILURE_RETURN_VALUE on failure.
 */
int uthread_resume(int tid)
{
    mask_timer();
    bool legal_id = 0 < tid && tid < MAX_THREAD_NUM && threads[tid] != nullptr;
    if (!legal_id)
    {
        std::cerr << LIBRARY_ERROR_PREFIX << INVALID_TID_ERROR_MSG << std::endl;
        unmask_timer();
        return FAILURE_RETURN_VALUE;
    }
    if (threads[tid]->state == RUNNING || threads[tid]->state == READY)
    {
        unmask_timer();
        return SUCCESS_RETURN_VALUE;
    }

    if (threads[tid]->state == BLOCKED)
    {
        if (threads[tid]->sleeping_time == NOT_SLEEPING)
        {
            threads[tid]->state = READY;
            ready_threads.push(tid);
        }
        else // Blocked and sleep thread
        {
            threads[tid]->state = SLEEPING;
        }
    }

    unmask_timer();
    return SUCCESS_RETURN_VALUE;
}

/**
 * @brief Put the current thread to sleep.
 * @param num_quantums Number of quantums to sleep
 * @return SUCCESS_RETURN_VALUE on success, FAILURE_RETURN_VALUE on failure.
 */
int uthread_sleep(int num_quantums)
{
    mask_timer();

    if (curr_thread_id == 0)
    {
        std::cerr << LIBRARY_ERROR_PREFIX << SLEEP_MT_ERROR_MSG << std::endl;
        unmask_timer();
        return FAILURE_RETURN_VALUE;
    }

    // Save the current state, including the program counter
    int ret_val = sigsetjmp(threads[curr_thread_id]->env, 1);
    bool did_just_save_bookmark = ret_val == 0;

    if (did_just_save_bookmark)
    {
        threads[curr_thread_id]->sleeping_time = num_quantums;
        threads[curr_thread_id]->state = SLEEPING;
        sleeping_threads.insert(curr_thread_id);
        set_ready_to_run();
    }
    unmask_timer();
    return SUCCESS_RETURN_VALUE;
}

/**
 * @brief Get the ID of the currently running thread.
 * @return Thread ID of the currently running thread.
 */
int uthread_get_tid()
{
    return curr_thread_id;
}

/**
 * @brief Get the total number of quantums elapsed.
 * @return Total number of quantums elapsed.
 */
int uthread_get_total_quantums()
{
    return total_quantom;
}

/**
 * @brief Get the number of quantums a thread has run.
 * @param tid Thread ID
 * @return Number of quantums the thread has run,
 *         or FAILURE_RETURN_VALUE on failure.
 */
int uthread_get_quantums(int tid)
{
    bool legal_id = 0 <= tid && tid < MAX_THREAD_NUM && threads[tid] != nullptr;
    if (legal_id)
    {
        return threads[tid]->quantum;
    }
    return FAILURE_RETURN_VALUE;
}
