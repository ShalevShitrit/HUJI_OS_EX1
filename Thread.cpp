//
// Created by Shalev Shitrit on 04/06/2024.
//

#include "Thread.h"
#include <memory>

Thread::Thread (unsigned int id, thread_state state, thread_entry_point
entry_point, char *stack, int quantum) :
    _id (id), _state (state), _quantum (quantum)
{
  address_t sp = (address_t) stack + STACK_SIZE - sizeof (address_t);
  address_t pc = (address_t) entry_point;

  sigsetjmp (_env, 1);
  (_env->__jmpbuf)[JB_SP] = translate_address (sp);
  (_env->__jmpbuf)[JB_PC] = translate_address (pc);
  sigemptyset (&_env->__saved_mask);
}
