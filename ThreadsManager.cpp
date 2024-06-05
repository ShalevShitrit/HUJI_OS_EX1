//
// Created by Shalev Shitrit on 04/06/2024.
//

#include "ThreadsManager.h"
#include <queue>
#include <signal.h>

#define SECOND 1000000

// See full documentation in "ThreadsManager.h"
ThreadsManager::ThreadsManager ()
{
  // initialize the id min heap (id from 0 to 99)
  for (int i = 0; i < MAX_THREAD_NUM; ++i)
  {
    _min_heap_id.push (i);
  }
}

void

void ThreadsManager::start_timer (int quantum_usecs)
{

  struct sigaction sa = {0};
  // Install timer_handler as the signal handler for SIGVTALRM.
  sa.sa_handler = &save_time_vars;
  if (sigaction (SIGVTALRM, &sa, NULL) < 0)
  {
    fprintf (stderr, "system error: setitimer error.");
    exit (1);
  }

  // Configure the timer to expire after 1 sec... */
  // first time interval, seconds part
  _timer.it_value.tv_sec = quantum_usecs / SECOND;
  // first time interval, microseconds part
  _timer.it_value.tv_usec = quantum_usecs % SECOND;

  // configure the timer to expire every 3 sec after that.
  // following time intervals, seconds part
  _timer.it_interval.tv_sec = quantum_usecs / SECOND;
  // following time intervals, microseconds part
  _timer.it_interval.tv_usec = quantum_usecs % SECOND;

  if (setitimer (ITIMER_VIRTUAL, &_timer, NULL))
  {
    fprintf (stderr, "system error: setitimer error.\n");
    exit (1)
  }
}

bool ThreadsManager::add_new_thread ()
{
  unsigned int new_id = _min_heap_id.top ();
  _min_heap_id.pop ();

  // check if has to create the main thread
  if (new_id == 0)
  {
    Thread *new_thread = new Thread (new_id, RUNNING, NULL, 1);
  }
  else
  {
    Thread *new_thread =
  }

  _threads.push_back (new_thread);
  return true;
}
