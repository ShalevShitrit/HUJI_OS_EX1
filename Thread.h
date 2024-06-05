//
// Created by Shalev Shitrit on 04/06/2024.
//

#ifndef _THREAD_H_
#define _THREAD_H_

#include <csetjmp>
#define STACK_SIZE 4096 /* stack size per thread (in bytes) */

//TODO need 32 and 64???
typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

address_t translate_address (address_t addr)
{
  address_t ret;
  asm volatile("xor    %%fs:0x30,%0\n"
               "rol    $0x11,%0\n"
      : "=g" (ret)
      : "0" (addr));
  return ret;
}

typedef void (*thread_entry_point) (void);

typedef enum
{
    RUNNING, READY, BLOCKED, SLEEPING
} thread_state;

class Thread
{
 private:
  unsigned int _id;
  thread_state _state;
  char _stack[STACK_SIZE];
  int _quantum; // TODO need??
  sigjmp_buf _env;

 public:
  Thread (unsigned int id, thread_state state, thread_entry_point
  entry_point, char *stack, int quantum);
};

#endif //_THREAD_H_
