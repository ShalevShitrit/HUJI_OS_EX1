#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <sys/time.h>
#include <setjmp.h>
#include <unistd.h>

#include "uthreads.h"
#include "Thread.h"
#include "ThreadsManager.h"

#define EXIT_RETURN_VALUE (-1)

typedef struct Thread
{
    unsigned int id;
    thread_state state;
    char stack[STACK_SIZE];
    int quantum; // TODO need??
    sigjmp_buf env;
} Thread;

std::vector<Thread *> _threads; // TODO size
std::priority_queue<int, std::vector<int>, std::greater<>> _min_heap_id;
struct itimerval _timer;
std::queue<Thread> _ready_threads;

void init_min_heap_id ()
{
  // initialize the id min heap (id from 0 to 99)
  for (int i = 0; i < MAX_THREAD_NUM; ++i)
  {
    _min_heap_id.push (i);
  }
}

int uthread_init (int quantum_usecs)
{
  if (quantum_usecs <= 0)
  {
    fprintf (stderr, "system error: quantum must be positive value in ms.\n");
    return EXIT_RETURN_VALUE;
  }

  ThreadsManager *threads_manager = new ThreadsManager ();

  //Thread *main_thread = new Thread (0, RUNNING, NULL, NULL, 1);






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