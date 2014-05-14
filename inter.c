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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <stdint.h>
#include <pthread.h>

#include "Trigger.h"
#include "inter.h"


#ifdef __x86_64__
  #define REG_BP  "rbp"
#else
  #define REG_BP "ebp"
#endif

/*
   these will be defined in the automatically generated .c file
   (intercept.stub.c)
*/

extern int log_fd, replay_fd;
extern int init_done;

/* stores the return address across the original library function call */ 
#ifdef __APPLE__ 
pthread_key_t return_address_key; 
#else 
static __thread long return_address; 
#endif 
 
/* avoid intercepting our function calls */ 
#ifdef __APPLE__
pthread_key_t no_intercept_key;
#else
static __thread int no_intercept;
#endif

uint64_t tsc() {
  uint32_t low, high;
  __asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high));
  uint64_t r = ((uint64_t) high<<32) | low;
  return r;
}

void lfi_segv_handler(int, siginfo_t *, void *)
{
#ifdef WITH_LOGS
  struct timespec t = {0, 0};
  char message[256];

  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t);

  sprintf(message, "[ %lld %ld %ld] SIGSEGV received\n", tsc(), (long)t.tv_sec, t.tv_nsec);
  write(log_fd, message, strlen(message));
  fdatasync(log_fd);
#endif
  abort();
}

void __attribute__ ((constructor)) 
my_init(void)
{
#ifdef WITH_LOGS
  log_fd = open(LOGFILE, 577, 0644);
  replay_fd = open(REPLAYFILE, 577, 0644);

  write(replay_fd, "<plan>\n", 7);
#endif
#ifdef WITH_SIGHANDLER
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = lfi_segv_handler;
  sigaction(SIGSEGV, &sa, NULL);
#endif
#ifdef __APPLE__
  int err;
  err = pthread_key_create(&return_address_key, NULL);
  err |= pthread_key_create(&no_intercept_key, NULL);
  if (err)
    write(2, "Failed to create thread keys\n", 29);
#endif
  init_done = 1;
}

void __attribute__ ((destructor))
my_fini(void)
{
#ifdef WITH_LOGS
  write(replay_fd, "</plan>\n", 8);
  close(replay_fd);
  close(log_fd);
#endif
}

long get_return_address()
{
  long r;
#ifdef __APPLE__
  r = (long)pthread_getspecific(return_address_key);
#else
  r = return_address;
#endif
  return r;
}

long get_no_intercept()
{
  long r;
#ifdef __APPLE__
  r = (long)pthread_getspecific(no_intercept_key);
#else
  r = no_intercept;
#endif
  return r;
}

void set_return_address(long value)
{
#ifdef __APPLE__
  pthread_setspecific(return_address_key, (void*)value);
#else
  return_address = value;
#endif
}

void set_no_intercept(long value)
{
#ifdef __APPLE__
  pthread_setspecific(no_intercept_key, (void*)value);
#else
  no_intercept = value;
#endif
}


/************************************************************************/
/* returns the action that should be taken based on the triggers        */
/* associated with the function fn when running an injection scenario   */
/************************************************************************/
void determine_action(struct fninfov2 fn_details[],
              __in const char* function_name,
              __in void* arg1,
              __in void* arg2,
              __in void* arg3,
              __in void* arg4,
              __in void* arg5,
              __in void* arg6,
              __out int* call_original,
              __out int* return_error,
              __out int* return_code,
              __out int* return_errno)
{
  int err_index, i, j;
  bool ev;
  char message[256];
  static long c;

  xmlDocPtr initDataDoc;
  xmlNodePtr initData;

  *call_original = 1;
  *return_error = 0;
  *return_code = 0;
  *return_errno = 0;
  TriggerDesc **triggers;

  /*
     you can think of the triggers for a function as a jagged array - fn_details[].triggers[]
     if all the triggers on one line (fn_details[i]) of the array evaluate to true,
     the error associated with that line is injected
     */
  for (i = 0; fn_details[i].function_name[0]; ++i)
  {
    triggers = fn_details[i].triggers;
    // if no triggers are defined the default behavior is to inject
    ev = true;
    for (j = 0; triggers[j]; ++j) {
      /*
         c++;
         if (0 == c % 100000)
         printf("%d funcs\n", c);
         */

      if (!triggers[j]->trigger)
      {
        /* TODO: make operation atomic i.e. never instantiate twice */
        triggers[j]->trigger = Class::newI(triggers[j]->tclass);

        if (!triggers[j]->trigger)
        {
          printf( "Trigger class %s not found or not yet registered while intercepting %s\n",
                  triggers[j]->tclass, fn_details[i].function_name);
          return;
        }
        else
        {
          if (triggers[j]->init[0])
          {
            initDataDoc = xmlParseDoc((xmlChar*)triggers[j]->init);

            if (initDataDoc)
              initData = xmlDocGetRootElement(initDataDoc);
          } else {
            initData = NULL;
          }
          triggers[j]->trigger->Init(initData);
        }
      }

#if defined(__i386)
      /*
         considering first arg to be at prev_ebp+2xsizeof(long)
         (not always the case. not really portable)
      */
      void* _ebp;
       __asm__ __volatile__ ("movl %%ebp, %0"  : "=m"(_ebp) : );

      long* prev_ebp = *((long**)_ebp);
      switch(fn_details[i].argc)
      {
      case 6:
        arg6 = *(prev_ebp+7);
      case 5:
        arg5 = *(prev_ebp+6);
      case 4:
        arg4 = *(prev_ebp+5);
      case 3:
        arg3 = *(prev_ebp+4);
      case 2:
        arg2 = *(prev_ebp+3);
      case 1:
        arg1 = *(prev_ebp+2);
      }
#endif

      switch (fn_details[i].argc)
      {
      case -1:
      case 0:
        ev = triggers[j]->trigger->Eval(fn_details[i].function_name);
        break;
      case 1:
        ev = triggers[j]->trigger->Eval(fn_details[i].function_name, arg1);
        break;
      case 2:
        ev = triggers[j]->trigger->Eval(fn_details[i].function_name, arg1,
                                        arg2);
        break;
      case 3:
        ev = triggers[j]->trigger->Eval(fn_details[i].function_name, arg3,
                                        arg2, arg3);
        break;
      case 4:
        ev = triggers[j]->trigger->Eval(fn_details[i].function_name, arg1,
                                        arg2, arg3, arg4);
        break;
      case 5:
        ev = triggers[j]->trigger->Eval(fn_details[i].function_name, arg1,
                                        arg2, arg3, arg4, arg5);
        break;
      case 6:
        ev = triggers[j]->trigger->Eval(fn_details[i].function_name, arg1,
                                        arg2, arg3, arg4, arg5, arg6);
        break;
      default:
        printf("A maximum of 6 arguments are supported in a trigger call\n");
        ev = false;
      }
      if (!ev) {
        break;
      }
    }
    if (ev)
    {
      *return_error = 1;
      *return_code = fn_details->return_value;
      *return_errno = fn_details->errno_value;
      *call_original = fn_details->call_original;
      break;
    }
  }

  /* the writes should be serialized (between threads) to avoid corruption */

  if (*return_error)
  {
#ifdef WITH_LOGS
    struct timespec t = {0, 0};
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t);

    sprintf(message, "[ %s, %lld %ld %ld] Returning code %d; setting errno to %d\n", function_name, tsc(), (long)t.tv_sec, t.tv_nsec, *return_code, *return_errno);
    write(log_fd, message, strlen(message));
    fdatasync(log_fd);

    sprintf(message, "<function name=\"%s\" inject=\"%d\" retval=\"%d\" errno=\"%d\" calloriginal=\"0\" />\n", function_name, call_count, *return_code, *return_errno);
    write(replay_fd, message, strlen(message));
#endif
  }
}
