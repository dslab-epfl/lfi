/*
     Created by Paul Marinescu and George Candea
     Copyright (C) 2009 EPFL (Ecole Polytechnique Federale de Lausanne)

     This file is part of LFI (Library-level Fault Injector).

     LFI is free software: you can redistribute it and/or modify it  
     under the terms of the GNU General Public License as published by the  
     Free Software Foundation, either version 3 of the License, or (at  
     your option) any later version.

     LFI is distributed in the hope that it will be useful, but  
     WITHOUT ANY WARRANTY; without even the implied warranty of  
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU  
     General Public License for more details.

     You should have received a copy of the GNU General Public  
     License along with LFI. If not, see http://www.gnu.org/licenses/.

     EPFL
     Dependable Systems Lab (DSLAB)
     Room 330, Station 14
     1015 Lausanne
     Switzerland
*/

#include <errno.h>
#include <stdint.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef __USE_GNU
#define __USE_GNU
#endif

#include <dlfcn.h>
#include <execinfo.h>
#include <time.h>

class Trigger;

/* the maximum number of frames in a stack trace */
#define TRACE_SIZE  100
/* human readable log file (overwritten at each run) */
#define LOGFILE    "inject.log"
#define LOGGING    0
/* machine readable log file used for injection replay (overwritten at each run) */
#define  REPLAYFILE  "replay.xml"

#define MAXINJECT  2000000

/* only used to enhance readbility */
#ifndef __in
#define __in
#endif

#ifndef __out
#define __out
#endif

#ifndef NULL
#define NULL  (void*)0
#endif

struct TriggerDesc
{
  char id[128];
  char tclass[128];
  Trigger* trigger;
  char init[4096];
};

struct fninfov2
{
  char function_name[256];
  int return_value;
  int errno_value;
  int call_original;
  int argc;

  /* custom triggers */
  TriggerDesc **triggers;
};

/* stores the return address across the original library function call
#ifdef __APPLE__
pthread_key_t return_address_key;
#else
static __thread long return_address;
#endif
*/
/* avoid intercepting our function calls
#ifdef __APPLE__
pthread_key_t no_intercept_key;
#else
static __thread int no_intercept;
#endif
*/

long get_return_address();
void set_return_address(long);
long get_no_intercept();
void set_no_intercept(long);

/*
   avoid including the standard headers because the compiler will likely
   generate warnings when injecting faults in an already declared function.
   However, this won't allow us to inject faults in these functions
   ALTERNATIVE: use dlsym to get pointers to the functions
*/

extern "C" {
  int printf(const char * _Format, ...);
}

void determine_action(struct fninfov2 fn_details[],
            __in const char* function_name,
            __out int* call_original,
            __out int *return_error,
            __out int* return_code,
            __out int* return_errno);

void print_backtrace(void* bt[], int nptrs, int log_fd);

/************************************************************************/
/*  GENERATE_STUBv2 - macro to generate stub functions targeted for x86 */
/*  UNSAFE: clobbered non-volatile registeres may not be restored       */
/*    when `forcing' an exit (i.e. when doing a leave/ret or leave/jmp) */
/*  SOLUTION: manually push/pop all non-volatile registers OR           */
/*    check assembly listing generated by the compiler to determine if  */
/*    any registers need to be `pop`-ed (per-compiler solution)         */
/************************************************************************/
#define GENERATE_STUBv2(FUNCTION_NAME) \
  void FUNCTION_NAME (void) \
{ \
  int call_original, return_error; \
  int return_code, return_errno; \
  int initial_no_intercept; \
  static void * (*original_fn_ptr)(); \
  \
  /* defaults */ \
  call_original = 1; \
  return_error = 0; \
  return_code = 0; \
  return_errno = 0; \
  \
  initial_no_intercept = get_no_intercept(); \
  if (0 == initial_no_intercept && init_done /* don't hook open or write in the constructor */) { \
    set_no_intercept(1); /* allow all determine_action-called functions to pass-through */ \
    determine_action(function_info_ ## FUNCTION_NAME, #FUNCTION_NAME, &call_original, &return_error, &return_code, &return_errno); \
  } \
  \
  if(!original_fn_ptr) { \
    original_fn_ptr = (void *(*)()) dlsym(RTLD_NEXT, #FUNCTION_NAME); \
    if(!original_fn_ptr) \
      printf("Unable to get address for function %s\n", #FUNCTION_NAME); \
  } \
  \
  set_no_intercept(initial_no_intercept); \
  /* disabled - unlikely to be useful in practice  \
  if (0 && call_original && return_error) \
  { \
    /* save the original return value */ \
    __asm__ ("movl 0x4(%%ebp), %%eax;" : "=a"(return_address)); \
    \
    __asm__ ("leave"); \
    __asm__ ("addl $0x4, %esp"); \
    /* at this point the stack is gone */ \
    \
    /* make the call the original function with the same stack */ \
    __asm__ ("call *%%eax;" : : "a"(original_fn_ptr)); \
    \
    errno = return_errno; \
    \
    /* push back the original return value */ \
    __asm__ ("pushl %%eax;" : : "a"(return_address)); \
    \
    __asm__ ("ret" : : "a"(return_code)); \
  } \
  else */ if (return_error) \
  { \
    errno = return_errno; \
    __asm__ ("" : : "a"(return_code)); \
    return; \
  } \
  else if (call_original) \
  { \
    /* this must correspond to the compiler-generated prologue for this function */ \
    __asm__ ("nop" : : "a"(original_fn_ptr)); \
    __asm__ ("mov %ebp, %esp"); \
    __asm__ ("sub $0x4, %esp"); /* assuming the compiler-generated prologue saves only ebx */ \
    __asm__ ("pop %ebx"); \
    __asm__ ("pop %ebp"); \
    __asm__ ("jmp *%eax"); \
  } \
}

struct reg_backup {
  void* r15;
  void* r14;
  void* r13;
  void* r12;
  void* rdi;
  void* rsi;
  void* rbx;
  void* rcx;
  void* rdx;
  void* r8;
  void* r9;
};
  
/***********************************************************************/
/*    GENERATE_STUB_x64 - macro to generate stub functions on x64      */
/*                        for Linux/MacOS x86_64                       */
/***********************************************************************/
#ifdef __APPLE__
  #define GENERATE_STUB_x64(FUNCTION_NAME, SYMBOL_NAME) \
  void FUNCTION_NAME (void) __asm__ ( "_" #SYMBOL_NAME ); \
  FUNCTION_BODY_x64(FUNCTION_NAME, SYMBOL_NAME)
#else
  #define GENERATE_STUB_x64(FUNCTION_NAME, SYMBOL_NAME) \
  void FUNCTION_NAME (void) __asm__ ( #SYMBOL_NAME ); \
  FUNCTION_BODY_x64(FUNCTION_NAME, SYMBOL_NAME)
#endif

#define FUNCTION_BODY_x64(FUNCTION_NAME, SYMBOL_NAME) \
  void FUNCTION_NAME (void) \
{ \
  int nptrs; \
  void* buffer[TRACE_SIZE]; \
  int call_original, return_error; \
  int return_code, return_errno; \
  reg_backup regs; \
  static void * (*original_fn_ptr)(); \
  /* we can't call write directly because it would prevent us for injecting faults in `write`
       (injecting requires the creation of a function with the same name but the prototype is
     different). We, use dlsym instead
  */ \
  static int * (*original_write_ptr)(int, const void*, int); \
  int initial_no_intercept; \
  \
  register void *_r15 __asm__ ( "r15" ); \
  register void *_r14 __asm__ ( "r14" ); \
  register void *_r13 __asm__ ( "r13" ); \
  register void *_r12 __asm__ ( "r12" ); \
  register void *_rdi __asm__ ( "rdi" ); \
  register void *_rsi __asm__ ( "rsi" ); \
  register void *_rbx __asm__ ( "rbx" ); \
  register void *_rcx __asm__ ( "rcx" ); \
  register void *_rdx __asm__ ( "rdx" ); \
  register void *_r8 __asm__ ( "r8" ); \
  register void *_r9 __asm__ ( "r9" ); \
  \
  /* save non-volatiles */ \
  regs.r15 = _r15; \
  regs.r14 = _r14; \
  regs.r13 = _r13; \
  regs.r12 = _r12; \
  regs.rbx = _rbx; \
  /* save function arguments */ \
  regs.rdi = _rdi; \
  regs.rsi = _rsi; \
  regs.rcx = _rcx; \
  regs.rdx = _rdx; \
  regs.r8 = _r8; \
  regs.r9 = _r9; \
  \
  /* defaults */ \
  call_original = 1; \
  return_error = 0; \
  return_code = 0; \
  return_errno = 0; \
  nptrs = 0; \
  /* printf("intercepted %s\n", #FUNCTION_NAME); */ \
  \
  if (init_done) { \
    initial_no_intercept = get_no_intercept(); \
    if (0 == initial_no_intercept) { \
      set_no_intercept(1); \
      determine_action(function_info_ ## FUNCTION_NAME, #FUNCTION_NAME, &call_original, &return_error, &return_code, &return_errno); \
    } \
  } \
        \
  \
  if(!original_fn_ptr) { \
    original_fn_ptr = (void *(*)()) dlsym(RTLD_NEXT, #SYMBOL_NAME); \
    if(!original_fn_ptr) \
      printf("Unable to get address for function %s\n", #SYMBOL_NAME); \
  } \
  \
  if (init_done) \
    set_no_intercept(initial_no_intercept); \
  \
  if (return_error) \
  { \
    errno = return_errno; \
    __asm__ ("" : : "a"(return_code)); \
    return; \
  } \
  else if (call_original) \
  { \
    /* restore function arguments */ \
    __asm__ __volatile__ ("movq %0, %%r9" : : "m"(regs.r9)); \
    __asm__ __volatile__ ("movq %0, %%r8" : : "m"(regs.r8)); \
    __asm__ __volatile__ ("movq %0, %%rdx" : : "m"(regs.rdx)); \
    __asm__ __volatile__ ("movq %0, %%rcx" : : "m"(regs.rcx)); \
    /* restore non-volatiles */ \
    __asm__ __volatile__ ("movq %0, %%rbx" : : "m"(regs.rbx)); \
    __asm__ __volatile__ ("movq %0, %%rsi" : : "m"(regs.rsi)); \
    __asm__ __volatile__ ("movq %0, %%rdi" : : "m"(regs.rdi)); \
    __asm__ __volatile__ ("movq %0, %%r12" : : "m"(regs.r12)); \
    __asm__ __volatile__ ("movq %0, %%r13" : : "m"(regs.r13)); \
    __asm__ __volatile__ ("movq %0, %%r14" : : "m"(regs.r14)); \
    __asm__ __volatile__ ("movq %0, %%r15" : : "m"(regs.r15)); \
    \
    __asm__ ("leave"); \
    __asm__ ("jmp *%%rax" : : "a"(original_fn_ptr)); \
  } \
}


#define STUB_VAR_DECL \
int log_fd, replay_fd; \
int init_done; 
