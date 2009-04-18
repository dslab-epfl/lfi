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
#include <string.h>
#include "inter.h"

/*
   these will be defined in the automatically generated .c file
   (intercept.stub.c)
*/

extern int trace_fd, log_fd, replay_fd;

extern int init_done;
extern int random_inject_probability;
extern void* exe_start, *exe_end;
extern time_t tt;
extern struct fninfov1 function_infov1[];
extern struct fninfov2 function_infov2[];
extern char TARGET_EXE[];

extern int fn_count;

void __attribute__ ((constructor)) 
my_init(void)
{
	log_fd = open(LOGFILE, 577, 0644);
	replay_fd = open(REPLAYFILE, 577, 0644);

	write(replay_fd, "<plan>\n", 7);
	
	tt = time(NULL);
	srand((unsigned)tt);
	/* find_range(); */
	init_done = 1;
}

void __attribute__ ((destructor))
my_fini(void)
{
	write(replay_fd, "</plan>\n", 8);
	close(replay_fd);
	close(log_fd);
}


/************************************************************************/
/* retrieves information on the function function_name when running in  */
/* random exploration mode                                              */
/************************************************************************/
struct fninfov1* get_function_infov1(__in char* function_name)
{
	int i;

	for (i = 0; i < fn_count; ++i)
	{
		if (0 == strcmp(function_name, function_infov1[i].function_name))
			return &function_infov1[i];
	}
	return NULL;
}


/************************************************************************/
/* retrieves information on the pair (function_name, call_count) when   */
/* running a preset plan                                                */
/************************************************************************/
struct fninfov2* get_function_infov2(__in char* function_name, __in int call_count)
{
	int i;
	
	for (i = 0; i < fn_count; ++i)
	{
		if (0 == strcmp(function_name, function_infov2[i].function_name) && call_count == function_infov2[i].call_count)
		return &function_infov2[i];
	}
	return NULL;
}

/************************************************************************/
/* returns the action that should be taken for the call to function fn  */
/* in random exploration mode                                           */
/************************************************************************/
void determine_actionv1(__in char* fn, int call_count, __out int* call_original, __out int *return_error, __out int* return_code, __out int* return_errno)
{
	struct fninfov1* fn_details;
	int err_index;
	char message[256];

    *call_original = 1;
	*return_error = 0;
    *return_code = -1;
	*return_errno = 0;

	fn_details = get_function_infov1(fn);


	if (fn_details)
	{
		if ((rand() % 1000) < random_inject_probability)
		{
		 	err_index = rand() % fn_details->err_count;
			*return_error = 1;
			*return_code = fn_details->errors[err_index].return_value;
			*return_errno = fn_details->errors[err_index].errno_value;
			
			/* the writes should be serialized (between threads) to avoid corruption */
			/*
			sprintf(message, "[%s] Returning code %d; setting errno to %d\n", fn, *return_code, *return_errno);
			write(log_fd, message, strlen(message));
			
			sprintf(message, "<function name=\"%s\" inject=\"%d\" retval=\"%d\" errno=\"%d\" calloriginal=\"0\" />\n", fn, call_count, *return_code, *return_errno);
			write(replay_fd, message, strlen(message));
			*/
		}
		else
		{
			/*
			sprintf(message, "[%s] Allowing normal execution\n", fn);
			write(log_fd, message, strlen(message));
			*/
		}
	}
	else
	{
		/*
		sprintf(message, "[%s] Entry not found\n", fn);
		write(log_fd, message, strlen(message));
		*/
	}
}

/************************************************************************/
/* returns the action that should be taken for the call to function fn  */
/* in random exploration mode - optimized to receive the fninfov1 struct*/
/************************************************************************/
void determine_actionv1_o(struct fninfov1* fn_details, __in char* function_name, int call_count, int* call_original, __out int *return_error, __out int* return_code, __out int* return_errno)
{
	int err_index;
	char message[256];
	
	*call_original = 1;
	*return_error = 0;
    *return_code = -1;
	*return_errno = 0;
	
	// printf("determine_actionv1_o %s\n", function_name);
	// sleep(1);
	if (fn_details)
	{
		if ((rand() % 1000) < random_inject_probability)
		{
		 	err_index = rand() % fn_details->err_count;
			*return_error = 1;
			*return_code = fn_details->errors[err_index].return_value;
			*return_errno = fn_details->errors[err_index].errno_value;
			
			/* the writes should be serialized (between threads) to avoid corruption */
			
			sprintf(message, "[%s] Returning code %d; setting errno to %d\n", function_name, *return_code, *return_errno);
			write(log_fd, message, strlen(message));
			
			sprintf(message, "<function name=\"%s\" inject=\"%d\" retval=\"%d\" errno=\"%d\" calloriginal=\"0\" />\n", function_name, call_count, *return_code, *return_errno);
			write(replay_fd, message, strlen(message));
			
		}
	}
	else
	{
		/*
		sprintf(message, "[%s] Entry not found\n", function_name);
		write(log_fd, message, strlen(message));
		*/
	}
}



/************************************************************************/
/* returns the action that should be taken for the call_count-th call   */
/* to function fn when running a preset plan                            */
/************************************************************************/
void determine_actionv2(__in char* fn, __in int call_count, __out int* call_original, __out int *return_error, __out int* return_code, __out int* return_errno)
{
	struct fninfov2* fn_details;
	int err_index;
	char message[256];

	*call_original = 1;
	*return_error = 0;
	*return_code = -1;
	*return_errno = 0;
	
	fn_details = get_function_infov2(fn, call_count);
	/*
	if (only_trace)
	{
		write(trace_fd, fn, strlen(fn));
		write(trace_fd, "\n", 1);
		return;
	}
	*/
	if (fn_details)
	{
		*return_error = 1;
		*return_code = fn_details->return_value;
		*return_errno = fn_details->errno_value;
		*call_original = fn_details->call_original;
	
		/* the writes should be serialized (between threads) to avoid corruption */
		/*
		sprintf(message, "[%s] Returning code %d; setting errno to %d\n", fn, *return_code, *return_errno);
		write(log_fd, message, strlen(message));

		sprintf(message, "<function name=\"%s\" inject=\"%d\" retval=\"%d\" errno=\"%d\" calloriginal=\"0\" />\n", fn, call_count, *return_code, *return_errno);
		write(replay_fd, message, strlen(message));
		*/

	}
	else
	{
		/* printf("[%s] Entry not found\n", fn); */\
	}
}

/************************************************************************/
/* returns the action that should be taken for the call_count-th call   */
/* to function fn when running a preset plan                            */
/************************************************************************/
void determine_actionv2_o(struct fninfov2* fn_details, __in char* function_name, __in int call_count, __out int* call_original, __out int *return_error, __out int* return_code, __out int* return_errno)
{
	int err_index, i;
	char message[256];

	*call_original = 1;
	*return_error = 0;
	*return_code = -1;
	*return_errno = 0;
	
	i = 0;
	while (fn_details[i].call_count)
	{
		if (fn_details[i].call_count == call_count)
		{
			*return_error = 1;
			*return_code = fn_details->return_value;
			*return_errno = fn_details->errno_value;
			*call_original = fn_details->call_original;
			break;
		}		
		++i;
	}

	/* the writes should be serialized (between threads) to avoid corruption */
	if (fn_details[i].call_count)
	{
		sprintf(message, "[%s] Returning code %d; setting errno to %d\n", function_name, *return_code, *return_errno);
		write(log_fd, message, strlen(message));

		sprintf(message, "<function name=\"%s\" inject=\"%d\" retval=\"%d\" errno=\"%d\" calloriginal=\"0\" />\n", function_name, call_count, *return_code, *return_errno);
		write(replay_fd, message, strlen(message));
	}
}



/************************************************************************/
/* Search /proc/self/maps for the address range at which the executable */
/* section of this library is loaded                                    */
/************************************************************************/

int find_range()
{
	char buffer[65536];
	char *line;
	int cb, rb;
	int fd = open(MAPFILE, 0);
	
	rb = 0;
	while (rb < sizeof(buffer)-1 && (cb = read(fd, &buffer[rb], sizeof(buffer)-rb-1)) > 0) {
		rb += cb;
	}
	printf("rd = %d\n", rb);
	if (rb > 0)
	{
		buffer[rb] = 0;
		line = strtok(buffer, "\n");
		do
		{
			if (strstr(line, "r-xp") && strstr(line, TARGET_EXE))
			{
				printf("injecting in: %s\n", line);
				sscanf(line, "%x-%x", &exe_start, &exe_end);
				
				break;
			}
		} while (line = strtok(0, "\n"));
	}
	return 0;
}

/*
int isbtinrange(__in void** backtrace, __in int btsize)
{
	int i;
	for (i = 1; i < btsize; ++i)
	{
		if (ignore_start < (unsigned)backtrace[i] && ignore_end > (unsigned)backtrace[i])
		{
			backtrace_symbols_fd(backtrace, btsize, 1);
			return 1;
		}
	}
	backtrace_symbols_fd(backtrace, btsize, 1);
	return 0;
}
*/
