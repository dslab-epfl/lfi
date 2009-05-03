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

#include <iostream>
#include <fstream>
#include <set>
#include <string.h>
#include <assert.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <sys/types.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

using namespace std;

#define STUBC	((char *) "intercept.stub.c")
#define STUBEX	((char *) "intercept.stub.so")

#define LOGFILE		"inject.log"
#define	REPLAYFILE	"replay.xml"


#define VERSION2

#define CRASH_METRIC		(int)1e8
#define FAILURE_METRIC		(int)1e6
#define TIME_MULTIPLIER		1

void usage(char* me)
{
    cout << "Usage: ";
    cout << me << " <configFile> -t <targetExecutable> [-m <targetModule>] [-r <injectionProb>] [-f <crashFile>]" << endl;
}

/************************************************************************/
/*	void print_xpath_nodesv2(xmlNodeSetPtr nodes, ofstream& out)        */
/*	                                                                    */
/*	XML file parser for the targeted injection format                   */
/*	outputs C code based on the XML nodes received                      */
/************************************************************************/

void print_xpath_nodesv2(xmlNodeSetPtr nodes, ofstream& out)
{
	xmlNodePtr cur;
	xmlChar* functionName, *call_count, *return_value, *errno_value, *call_original;
	int size;
	int i, fn_count;

	size = (nodes) ? nodes->nodeNr : 0;
	fn_count = 0;

	out << "struct fninfov1 " << "function_infov1[1];" << endl;
	out << "struct fninfov2 " << "function_infov2[] = {" << endl;
	for(i = 0; i < size; ++i)
	{
		assert(nodes->nodeTab[i]);

		if(nodes->nodeTab[i]->type == XML_ELEMENT_NODE)
		{
			cur = nodes->nodeTab[i];   	    
			functionName = xmlGetProp(cur, (xmlChar*)"name");
			call_count = xmlGetProp(cur, (xmlChar*)"inject");
			errno_value = xmlGetProp(cur, (xmlChar*)"errno");
			return_value = xmlGetProp(cur, (xmlChar*)"retval");
			call_original = xmlGetProp(cur, (xmlChar*)"calloriginal");

			if (functionName && call_count && errno_value && return_value && call_original)
			{
				out << "\t{ \"" << functionName << "\", ";
				out << call_count << ", ";
				out << return_value << ", ";
				out << errno_value << ", ";
				out << call_original << "}," << endl;
				++fn_count;
			}
			if (functionName)
				xmlFree(functionName);
			if (call_count)
				xmlFree(call_count);
			if (errno_value)
				xmlFree(errno_value);
			if (return_value)
				xmlFree(return_value);
			if (call_original)
				xmlFree(call_original);
		}
	}
	out << "};" << endl;
	out << "int fn_count = " << fn_count << ";" << endl;


	set<string> generated_stubs;
	for(i = 0; i < size; ++i)
	{
		assert(nodes->nodeTab[i]);

		if(nodes->nodeTab[i]->type == XML_ELEMENT_NODE)
		{
			cur = nodes->nodeTab[i];   	    
			functionName = xmlGetProp(cur, (xmlChar*)"name");
			if (functionName && generated_stubs.end() == generated_stubs.find((char*)functionName))
			{
				out << "#ifdef __x86_64__" << endl;
				out << "GENERATE_STUBv2_x64(" << (char*)functionName << ")" << endl << endl;
				out << "#else" << endl;
				out << "GENERATE_STUBv2(" << (char*)functionName << ")" << endl;
				out << "#endif" << endl << endl;
				generated_stubs.insert((char*)functionName);
				xmlFree(functionName);
			}
		}
	}
}



void print_xpath_nodesv2_fromv1(xmlNodeSetPtr nodes, ofstream& out)
{
	xmlNodePtr cur;
	xmlChar* functionName, *functionName2, *call_count, *return_value, *errno_value, *call_original;
	int size;
	int i, j, fn_count;
	set<string> functionsUsed;

	size = (nodes) ? nodes->nodeNr : 0;
	fn_count = 0;

	out << "struct fninfov1 " << "function_infov1[1];" << endl;
	out << "struct fninfov2 " << "function_infov2[1];" << endl;
	for(i = 0; i < size; ++i)
	{
        assert(nodes->nodeTab[i]);

        if(nodes->nodeTab[i]->type == XML_ELEMENT_NODE)
        {
            cur = nodes->nodeTab[i];
            functionName = xmlGetProp(cur, (xmlChar*)"name");
			if (functionsUsed.find((char*)functionName) != functionsUsed.end())
				continue;
			call_count = xmlGetProp(cur, (xmlChar*)"inject");
			errno_value = xmlGetProp(cur, (xmlChar*)"errno");
			return_value = xmlGetProp(cur, (xmlChar*)"retval");
			call_original = xmlGetProp(cur, (xmlChar*)"calloriginal");


			out << "struct fninfov2 function_info_" << functionName << "[] = {\n";

			out << "\t{ \"" << functionName << "\", ";
			out << call_count << ", ";
			out << return_value << ", ";
			out << errno_value << ", ";
			out << call_original << "}," << endl;

			functionsUsed.insert((char*)functionName);

			if (call_count)
				xmlFree(call_count);
			if (errno_value)
				xmlFree(errno_value);
			if (return_value)
				xmlFree(return_value);
			if (call_original)
				xmlFree(call_original);

			for(j = i+1; j < size; ++j)
			{
				assert(nodes->nodeTab[j]);
				if(nodes->nodeTab[j]->type == XML_ELEMENT_NODE)
				{
					cur = nodes->nodeTab[j];
		            functionName2 = xmlGetProp(cur, (xmlChar*)"name");
					if (0 == strcmp((char*)functionName, (char*)functionName2))
					{
						call_count = xmlGetProp(cur, (xmlChar*)"inject");
						errno_value = xmlGetProp(cur, (xmlChar*)"errno");
						return_value = xmlGetProp(cur, (xmlChar*)"retval");
						call_original = xmlGetProp(cur, (xmlChar*)"calloriginal");

						out << "\t{ \"" << functionName << "\", ";
						out << call_count << ", ";
						out << return_value << ", ";
						out << errno_value << ", ";
						out << call_original << "}," << endl;

						if (functionName2)
							xmlFree(functionName2);
						if (call_count)
							xmlFree(call_count);
						if (errno_value)
							xmlFree(errno_value);
						if (return_value)
							xmlFree(return_value);
						if (call_original)
							xmlFree(call_original);
					}
				}

			}

			++fn_count;

			out << "};\n";

			if (functionName)
				xmlFree(functionName);
		}
	}
	
	out << "int fn_count = " << fn_count << ";" << endl;


	set<string> generated_stubs;
	for(i = 0; i < size; ++i)
	{
		assert(nodes->nodeTab[i]);

		if(nodes->nodeTab[i]->type == XML_ELEMENT_NODE)
		{
			cur = nodes->nodeTab[i];   	    
			functionName = xmlGetProp(cur, (xmlChar*)"name");
			if (functionName && generated_stubs.end() == generated_stubs.find((char*)functionName))
			{
				out << "#ifdef __x86_64__" << endl;
				out << "GENERATE_STUBv2_x64(" << (char*)functionName << ")" << endl << endl;
				out << "#else" << endl;
				out << "GENERATE_STUBv2(" << (char*)functionName << ")" << endl;
				out << "#endif" << endl << endl;
				generated_stubs.insert((char*)functionName);
				xmlFree(functionName);
			}
		}
	}
}


/************************************************************************/
/*	void print_xpath_nodesv1(xmlNodeSetPtr nodes, ofstream& out)        */
/*                                                                      */
/*	XML file parser for the random injection file format                */
/*	outputs C code based on the XML nodes received                      */
/************************************************************************/
void print_xpath_nodesv1(xmlNodeSetPtr nodes, ofstream& out)
{
    xmlNodePtr cur;
    xmlChar* functionName;
	xmlChar* nodeValue;
	xmlChar* returnValue;
    int size;
    int i, error_count, fn_count, total_errors;
    
    size = (nodes) ? nodes->nodeNr : 0;
    fn_count = 0;
    total_errors = 0;

    for(i = 0; i < size; ++i)
    {
        assert(nodes->nodeTab[i]);

        if(nodes->nodeTab[i]->type == XML_ELEMENT_NODE)
        {
            cur = nodes->nodeTab[i];   	    
            functionName = xmlGetProp(cur, (xmlChar*)"name");
            if (functionName)
            {
				if (cur->children && cur->children->next && cur->children->next->children)
				{
					error_count = 0;
					returnValue = xmlGetProp(cur->children->next, (xmlChar*)"retval");
					out << "struct error_description " << (char*)functionName << "_error_description[] = {" << endl;
					for (cur = cur->children->next->children; cur; cur = cur->next)
					{
						if (XML_ELEMENT_NODE == cur->type)
						{
							nodeValue = xmlNodeGetContent(cur);
							if (NULL != nodeValue)
							{
								++error_count;
								out << "\t{" << returnValue << ", " << (char*)nodeValue << "}," << endl;
								++total_errors;
								xmlFree(nodeValue);
							}
						}
					}
					out << "};" << endl;
				}
                
                xmlFree(functionName);
            }
        }
    }
	out << "struct fninfov2 " << "function_infov2[1];" << endl;
	out << "struct fninfov1 " << "function_infov1[] = {" << endl;
	for(i = 0; i < size; ++i)
	{
		assert(nodes->nodeTab[i]);

		if(nodes->nodeTab[i]->type == XML_ELEMENT_NODE)
		{
			cur = nodes->nodeTab[i];   	    
			functionName = xmlGetProp(cur, (xmlChar*)"name");
			if (functionName)
			{
				if (cur->children && cur->children->next && cur->children->next->children)
				{
					error_count = 0;
					for (cur = cur->children->next->children; cur; cur = cur->next)
					{
						if (XML_ELEMENT_NODE == cur->type)
						{
							nodeValue = xmlNodeGetContent(cur);
							if (NULL != nodeValue)
							{
								++error_count;
								xmlFree(nodeValue);
								++total_errors;
							}
						}
					}
					out << "\t{ \"" << functionName << "\", ";
					out << error_count << ", ";
					out << (char*)functionName << "_error_description }," << endl;
					
					++fn_count;
				}

				xmlFree(functionName);
			}
		}
	}
	out << "};" << endl;
	out << "int fn_count = " << fn_count << ";" << endl;

	/* for the optimized random version */
	for(i = 0; i < size; ++i)
	{
		assert(nodes->nodeTab[i]);

		if(nodes->nodeTab[i]->type == XML_ELEMENT_NODE)
		{
			cur = nodes->nodeTab[i];   	    
			functionName = xmlGetProp(cur, (xmlChar*)"name");
			if (functionName)
			{
				if (cur->children && cur->children->next && cur->children->next->children)
				{
					error_count = 0;
					for (cur = cur->children->next->children; cur; cur = cur->next)
					{
						if (XML_ELEMENT_NODE == cur->type)
						{
							nodeValue = xmlNodeGetContent(cur);
							if (NULL != nodeValue)
							{
								++error_count;
								xmlFree(nodeValue);
								++total_errors;
							}
						}
					}
					out << "struct fninfov1 function_info_" << functionName << " = {\"";
					out << functionName << "\", " << error_count << ", ";
					out << (char*)functionName << "_error_description };" << endl;
				}

				xmlFree(functionName);
			}
		}
	}

	set<string> generated_stubs;
	for(i = 0; i < size; ++i)
	{
		assert(nodes->nodeTab[i]);

		if(nodes->nodeTab[i]->type == XML_ELEMENT_NODE)
		{
			cur = nodes->nodeTab[i];   	    
			functionName = xmlGetProp(cur, (xmlChar*)"name");
			if (functionName)
			{
				if (cur->children && cur->children->next && cur->children->next->children &&
					generated_stubs.end() == generated_stubs.find((char*)functionName))
				{
					out << "GENERATE_STUBv2(" << (char*)functionName << ")" << endl << endl;
					generated_stubs.insert((char*)functionName);
				}

				xmlFree(functionName);
			}
		}
	}
	cout << "total errors: " << total_errors << endl;
}

/************************************************************************/
/*	int compile_file(char* cfile, char* outfile)                        */
/*                                                                      */
/*	Compiles cfile (dynamically generated) along with its dependencies  */
/*	to outfile as a shared object using appropriate flags               */
/************************************************************************/
int compile_file(char* cfile, char* outfile)
{
	char cmd[1024];
	int status;

	sprintf(cmd, "gcc -g -o %s %s inter.c -O0 -shared -dynamic -fPIC -ldl", outfile, cfile);
	
	cerr << "compiling stub library " << outfile << " from " << cfile << endl;
	cerr << cmd << " ..." << endl;

	status = system(cmd);
	if (0 != WEXITSTATUS(status))
	{
		cerr << "Compile failed" << endl;
		return -1;
	}
	else
	{
		cerr << "compiled successfully..." << endl;
	}
	sprintf( cmd, "rm -f %s.* inter.c.*", cfile );
	system( cmd );
    return 0;
}

int generate_stub(char* config, bool random_injection, char* inject_probability, char* module_target)
{
    xmlDocPtr doc;
    xmlXPathContextPtr xpathCtx;
    xmlXPathObjectPtr xpathObj;
    xmlChar *xpathExpr = (xmlChar*)"//function";

	cerr << "generating stub file " << STUBC << " from " << config << endl;

    doc = xmlParseFile(config);
    if (doc == NULL) {
        cerr << "Unable to open " << config << endl;
        return -1;
    }

    xpathCtx = xmlXPathNewContext(doc);
    if(xpathCtx == NULL) {
        cerr << "Error: unable to create new XPath context" << endl;
        xmlFreeDoc(doc); 
        return(-1);
    }

    xpathObj = xmlXPathEvalExpression(xpathExpr, xpathCtx);
    if(xpathObj == NULL) {
        cerr << "Error: unable to evaluate xpath expression \"" << xpathExpr << "\"" << endl;
        xmlXPathFreeContext(xpathCtx); 
        xmlFreeDoc(doc); 
        return(-1);
    }

    /* Print results */
    ofstream outf(STUBC);

	outf << "#include \"inter.h\"" << endl;
	outf << "STUB_VAR_DECL" << endl << endl;

	/* outf << "int only_trace = " << 0 << ";" << endl; */
	outf << "char TARGET_EXE[] = \"" << module_target << "\";" << endl;
	if (random_injection)
	{
		outf << "int random_inject_probability = " << inject_probability << ";" << endl;
		outf << "#define determine_action determine_actionv1_o" << endl;
	}
	else
	{
		/* random_inject_probability is unused in plan-injection mode*/
		outf << "int random_inject_probability = 0;" << endl;
		outf << "#define determine_action determine_actionv2_o" << endl;
	}
	
	if (random_injection)
		print_xpath_nodesv1(xpathObj->nodesetval, outf);
	else
		print_xpath_nodesv2_fromv1(xpathObj->nodesetval, outf);

    /* Cleanup */
    xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);

    return compile_file(STUBC, STUBEX);
}

/************************************************************************/
/*	run_subject(int argc, char** argv,                                  */
/*              char* preload_library, char *envp[])                    */
/*                                                                      */
/*	Runs subject program defined by (argc, argv[]) in the parent's      */
/*        (our) environment + LD_PRELOAD                                */
/*                                                                      */
/*	Returns:                                                            */
/*		-1 - failed to start program                                    */
/*		0  - child program exited normally with a 0 return code         */
/*		FAILURE_METRIC - child program exited normally with a non 0     */
/*                                                     return code      */
/*		CRASH_METRIC - child program was terminated by a signal         */
/************************************************************************/
int run_subject(int argc, char** argv, char* preload_library, char *envp[])
{
    char preload_path[1024] = "LD_PRELOAD=";
    char **newenv, **newarg;
    int i, return_value;
	int fd;
    pid_t monitor;
	int status, exit_status, exit_signal;
	struct timeval tvstart, tvend;
	
	key_t key = getpid();
	int* runstatus;
	int shmid = -1;

	if ((shmid = shmget( key, 1024, IPC_CREAT | 0666 )) < 0 )
		perror("shmget");
	if ((runstatus = (int*)shmat( shmid, NULL, 0 )) == (int*) -1)
		perror("shmat");
	
	return_value = CRASH_METRIC;
    if (getcwd(&preload_path[11], sizeof(preload_path) - strlen(preload_library) - 12))
    {
        strcat(preload_path, "/");
        strcat(preload_path, preload_library);
        cerr << preload_path << endl;

        i = 0;
        while (envp[++i]);
        newenv = (char**)malloc((i+2)*sizeof(char*));
        newenv[i] = &preload_path[0];
        newenv[i+1] = NULL;
        for (--i; i >= 0; --i)
            newenv[i] = envp[i];

        newarg = (char**)malloc((argc+1)*sizeof(char*));
		for (i = 0; i < argc; ++i)
			newarg[i] = argv[i];
        newarg[argc] = NULL;

		gettimeofday(&tvstart, NULL);
        if (0 == (monitor = fork()))
        {
            execve(argv[0], newarg, newenv);
			*runstatus = 1;
			shmdt(runstatus);
			_exit(0);
        }
        else
        {
            /* monitor */
			if (-1 == monitor)
			{
				cerr << "Unable to fork" << endl;
			}
			else
			{
				waitpid(monitor, &status, 0);
				gettimeofday(&tvend, NULL);
				if (0 != *runstatus)
				{
					return_value = -1;
				}
				else
				{
					if (WIFEXITED(status))
					{
						exit_status = WEXITSTATUS(status);
						cerr << "Process exited normally. Exit status: " << exit_status << endl;
						if (exit_status)
						{
							return_value = FAILURE_METRIC;
						}
						else
						{
							/* leave the timing to the external sensor
							return_value = (tvend.tv_sec-tvstart.tv_sec)*100 + (tvend.tv_usec - tvstart.tv_usec)/10000;
							*/
							return_value = 0;
						}
						
					}
					else if (WIFSIGNALED(status))
					{
						exit_signal = WTERMSIG(status);
						cerr << "Process terminated by signal " << exit_signal << endl;

						fd = open(REPLAYFILE, O_WRONLY|O_APPEND);
						write(fd, "</plan>\n", 8);
						close(fd);
						fd = open(LOGFILE, O_WRONLY|O_APPEND);
						close(fd);
						
						return_value = CRASH_METRIC;
					}
				}
			}
        }
    }
	if (shmdt(runstatus) < 0)
	{
		perror("shmdt");
	}

	if ( shmctl( shmid, IPC_RMID, NULL ) < 0 )
	{
		perror("shmctl");
	}
	return return_value;
}

int main(int argc, char* argv[], char* envp[])
{
        char *crash_create, *run_target, *module_target, *inject_probability, *token;
	char *run_argv[64];
	int run_argc;
	int status, crash_check, test_score, fd, prob;
	bool random_injection;
	
	int c;

	crash_check = 0;
	random_injection = false;
	module_target = NULL;
	run_target = NULL;
	inject_probability = NULL;
	
	opterr = 0;

	while ((c = getopt (argc, argv, "r:t:f:m:")) != -1)
	{
		switch (c)
		{
		case 'r':
		  random_injection = true;
		  inject_probability = optarg;
		  prob = atoi( inject_probability );
		  if( prob<=0 || prob>1000 ) {
		    fprintf( stderr, "Injection probability %u is invalid; must be in interval (0, 1000]\n", prob );
		    abort();
		  }
		  break;
		case 'f':
		  crash_check = 1;
		  crash_create = optarg;
		  break;
		case 't':
		  run_target = optarg;
		  if( !module_target )
		    module_target = run_target ;
		  break;
		case 'm':
		  module_target = optarg;
		  break;
		case '?':
		  if (optopt == 'f')
		    fprintf (stderr, "Option -f requires an argument.\n");
		  else if (isprint (optopt))
		    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
		  else
		    fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
		  return 1;
		default:
		  abort ();
		}
	}

	if (!run_target || optind >= argc)
	{
		usage(argv[0]);
		return -1;
	}
	assert( module_target );

	// Do some sanity checking on the arguments
	if ( access(run_target, X_OK) ) {
	  switch( errno ) 
	    {
	    case EACCES:
	      fprintf( stderr, "Permission denied on %s\n", run_target );
	      exit(-1);
	    case ELOOP:
	      fprintf( stderr, "Too many symbolic links while getting to %s\n", run_target );
	      exit(-1);
	    case ENAMETOOLONG:
	      fprintf( stderr, "File name too long: %s\n", run_target );
	      exit(-1);
	    case ENOENT:
	      fprintf( stderr, "No such file (did you provide the full path?): %s\n", run_target );
	      exit(-1);
	    default:
	      fprintf( stderr, "Cannot access %s\n", run_target );
	      exit(-1);
	    }
	}

	xmlInitParser();
	status = generate_stub(argv[optind], random_injection, inject_probability, module_target);
	xmlCleanupParser();

	++optind;
	if (0 == status)
	{
		run_argc = 0;
		token = strtok(run_target, "\t ");
		while (token)
		{
			run_argv[run_argc++] = token;
			token = strtok(NULL, "\t ");
		}
		if ((test_score = run_subject(run_argc, run_argv, STUBEX, envp)) < 0)
			cerr << "A problem occurred running the subject" << endl;
	}
	if (crash_check && CRASH_METRIC == test_score)
	{
		fd = open(crash_create, O_WRONLY|O_CREAT, 0644);
		close(fd);
	}
	cout << test_score << endl;
    return 0;
}


