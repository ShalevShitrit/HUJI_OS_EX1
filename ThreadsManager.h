//
// Created by Shalev Shitrit on 04/06/2024.
//

#ifndef _THREADSMANAGER_H_
#define _THREADSMANAGER_H_

#include "Thread.h"
#include <vector>
#include <queue>
#include <sys/time.h>


#define MAX_THREAD_NUM 100 /* maximal number of threads */




class ThreadsManager
{
 private:
  std::vector<Thread *> _threads; // TODO size
  std::priority_queue<int, std::vector<int>, std::greater<>> _min_heap_id;
  struct itimerval _timer;
  std::queue<Thread> _ready_threads;

 public:
  /**
   *
   */
  ThreadsManager ();

  /**
   *
   */
   void start_timer(int quantum_usecs);

  /**
   *
   */
  void add_new_thread ();
};

#endif //_THREADSMANAGER_H_
