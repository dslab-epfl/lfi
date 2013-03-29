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
#ifdef __APPLE__
#define STUBEX	((char *) "intercept.stub.dylib")
#else
#define STUBEX	((char *) "intercept.stub.so")
#endif

#define LOGFILE		"inject.log"
#define	REPLAYFILE	"replay.xml"


#define CRASH_METRIC		(int)1e8
#define FAILURE_METRIC		(int)1e6
#define TIME_MULTIPLIER		1

static void
usage(char* me)
{
    cout << "Usage: ";
    cout << me << " [-t <targetExecutable>] <configurationFile>" << endl;
}

static void
print_attribute_names(xmlAttrPtr a_node, ofstream& out)
{
    for (; a_node; a_node = a_node->next) {
		out << " " << a_node->name << "=\\\"" << a_node->children->content << "\\\"";
		if (a_node->next)
			out << " ";
	}
}

static void
print_element_names(xmlNodePtr a_node, ofstream& out)
{
    xmlNodePtr cur_node = NULL;
	xmlChar ch;
	int i;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE) {
            out << "<" << cur_node->name;
			print_attribute_names(cur_node->properties, out);
			out << ">";
		} else if (cur_node->type == XML_TEXT_NODE) {
			i = 0;
			while (0 != (ch = cur_node->content[i++]))
			{
				if (ch == '\r' || ch == '\n')
					out << "\\";
				out << ch;
			}
		}

        print_element_names(cur_node->children, out);

        if (cur_node->type == XML_ELEMENT_NODE) {
            out << "</" << cur_node->name << ">";
        }
    }
}

static void
print_triggers(xmlNodeSetPtr nodes, ofstream& out)
{
	xmlNodePtr cur;
	xmlChar *triggerId;
	xmlChar *triggerClass;

	int size;
	int i, fn_count;
	set<string> functionsUsed;

	size = (nodes) ? nodes->nodeNr : 0;
	fn_count = 0;

	for(i = 0; i < size; ++i)
	{
        assert(nodes->nodeTab[i]);

		cur = nodes->nodeTab[i];
        if(cur->type == XML_ELEMENT_NODE)
        {
            triggerId = xmlGetProp(cur, (xmlChar*)"id");
			triggerClass = xmlGetProp(cur, (xmlChar*)"class");

			if (triggerId && triggerClass)
			{
				out << "struct TriggerDesc trigger_" << triggerId << " = { \"" << triggerId << "\", ";
				out << "\"" << triggerClass << "\", NULL, ";
				
				xmlFree(triggerId);
				xmlFree(triggerClass);

				cur = cur->children;
				while (cur && xmlStrcmp(cur->name, (const xmlChar *)"args"))
				{
					cur = cur->next;
				}
				if (cur)
				{
					out << "\"";
					print_element_names(cur, out);
					out << "\"";
				} else {
					out << "\"\"";
				}
				out << " };" << endl;
			}
		}
	}
}

static void
print_function(xmlNodePtr fn, int triggerListId, ofstream& out)
{
	const char defErrno[] = "0";
	const char defCallOriginal[] = "0";
	const char defArgc[] = "0";

	xmlChar* functionName, *return_value,
		*errno_value, *call_original, *argc;
	
	functionName = xmlGetProp(fn, (xmlChar*)"name");
	errno_value = xmlGetProp(fn, (xmlChar*)"errno");
	return_value = xmlGetProp(fn, (xmlChar*)"retval");
	call_original = xmlGetProp(fn, (xmlChar*)"calloriginal");
	argc = xmlGetProp(fn, (xmlChar*)"argc");

	if (functionName && return_value)
	{
		out << "\t{ \"" << functionName << "\", ";
		out << return_value << ", ";
		out << (errno_value ? (char*)errno_value : defErrno) << ", ";
		out << (call_original ? (char*)call_original : defCallOriginal) << ", ";
		out << (argc ? (char*)argc : defArgc) << ", ";
		out << "triggerList_" << triggerListId;
		out << " }," << endl;
	}

	if (functionName)
		xmlFree(functionName);
	if (errno_value)
		xmlFree(errno_value);
	if (return_value)
		xmlFree(return_value);
	if (call_original)
		xmlFree(call_original);
	if (argc)
		xmlFree(argc);
}

static void
print_trigger_list(xmlNodePtr fn, int triggerListId, ofstream& out)
{
	xmlNodePtr cur;
	xmlChar *triggerId;

	cur = fn->children;
	out << "TriggerDesc* triggerList_" << triggerListId << "[] = { ";

	while (cur)
	{
		if (cur->type == XML_ELEMENT_NODE &&
			0 == xmlStrcmp(cur->name, (const xmlChar *)"triggerx"))
		{
			triggerId = xmlGetProp(cur, (xmlChar*)"ref");
			if (triggerId)
			{
				out << "&trigger_" << triggerId << ", ";
				xmlFree(triggerId);
			}
		}
		cur = cur->next;
	}
	out << "NULL };" << endl;
}

static void
print_stubs(xmlNodeSetPtr nodes, ofstream& out)
{
	xmlNodePtr cur;
	xmlChar *functionName;
	xmlChar *functionName2;
	int size;
	int i, j, triggerListId, triggerListIdBase;
	set<string> functionsUsed;

	size = (nodes) ? nodes->nodeNr : 0;

	triggerListId = 1;
	for(i = 0; i < size; ++i)
	{
        assert(nodes->nodeTab[i]);

        if(nodes->nodeTab[i]->type == XML_ELEMENT_NODE)
        {
            cur = nodes->nodeTab[i];
            functionName = xmlGetProp(cur, (xmlChar*)"name");
			if (!functionName || functionsUsed.find((char*)functionName) != functionsUsed.end())
				continue;

			functionsUsed.insert((char*)functionName);
			
			triggerListIdBase = triggerListId;
			print_trigger_list(cur, triggerListId++, out);
			for(j = i+1; j < size; ++j)
			{
				assert(nodes->nodeTab[j]);
				if(nodes->nodeTab[j]->type == XML_ELEMENT_NODE)
				{
					cur = nodes->nodeTab[j];
		            functionName2 = xmlGetProp(cur, (xmlChar*)"name");
					if (functionName2)
					{
						if (0 == strcmp((char*)functionName, (char*)functionName2))
						{
							print_trigger_list(cur, triggerListId++, out);
						}
						xmlFree(functionName2);
					}
				}
			}

			out << "struct fninfov2 function_info_" << functionName << "[] = {\n";
			
			triggerListId = triggerListIdBase;
			
			cur = nodes->nodeTab[i];
			print_function(cur, triggerListId++, out);
			for(j = i+1; j < size; ++j)
			{
				assert(nodes->nodeTab[j]);
				if(nodes->nodeTab[j]->type == XML_ELEMENT_NODE)
				{
					cur = nodes->nodeTab[j];
		            functionName2 = xmlGetProp(cur, (xmlChar*)"name");
					if (functionName2)
					{
						if (0 == strcmp((char*)functionName, (char*)functionName2))
						{
							print_function(cur, triggerListId++, out);
						}
						xmlFree(functionName2);
					}
				}
			}

			out << "\t{ \"\", 0, 0, 0, 0, NULL }" << endl;
			out << "};\n";

			xmlFree(functionName);
		}
	}
	ofstream symbols("symbols");
	out << "extern \"C\" {" << endl;
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
				xmlChar* aliasName = xmlGetProp(cur, (xmlChar*)"alias");
				out << "#ifdef __x86_64__" << endl;
				if (aliasName) {
					out << "GENERATE_STUB_x64(" << (char*)functionName << ", " << (char*)aliasName << ")" << endl;
					symbols << "_" << aliasName << endl;
				} else {
					out << "GENERATE_STUB_x64(" << (char*)functionName << ", " << (char*)functionName << ")" << endl;
					symbols << "_" << functionName << endl;
				}
				out << "#else" << endl;
				out << "GENERATE_STUBv2(" << (char*)functionName << ")" << endl;
				out << "#endif" << endl << endl;
				generated_stubs.insert((char*)functionName);
				xmlFree(functionName);
				if (aliasName)
					xmlFree(aliasName);
			}
		}
	}
	out << "}" << endl;
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
#ifdef __APPLE__
	sprintf(cmd, "g++  -g -o %s %s inter.c Trigger.cpp triggers/*.cpp `xml2-config --cflags` `xml2-config --libs` -O0 -shared -Xlinker -exported_symbols_list -Xlinker symbols", outfile, cfile);
#else
	sprintf(cmd, "g++ -g -o %s %s inter.c Trigger.cpp triggers/*.cpp `xml2-config --cflags` `xml2-config --libs` -O0 -shared -fPIC -lrt -ldl", outfile, cfile);
#endif
        // use the following line instead if you need debug information support (after installing libelf, libdwarf and the appropriate trigger)
        //sprintf(cmd, "g++ -g -o %s %s inter.c Trigger.cpp triggers/*.cpp `xml2-config --cflags` `xml2-config --libs` -O0 -shared -fPIC -lrt -ldl -ldwarf -lelf", outfile, cfile);

	cerr << "Compiling stub library " << outfile << " from " << cfile << endl;
	cerr << cmd << " ..." << endl;

	status = system(cmd);
	if (0 != WEXITSTATUS(status))
	{
		cerr << "Compile failed" << endl;
		return -1;
	}
	else
	{
		cerr << "Compiled successfully..." << endl;
	}
    return 0;
}

int generate_stub(char* config)
{
    xmlDocPtr doc;
    xmlXPathContextPtr xpathCtx;
    xmlXPathObjectPtr xpathObjTriggers;
	xmlXPathObjectPtr xpathObj;
    xmlChar *xpathExpr = (xmlChar*)"//function";
	xmlChar *xpathExprTriggers = (xmlChar*)"//trigger";

	cerr << "Generating stub file " << STUBC << " from " << config << endl;

    /* Print results */
    ofstream outf(STUBC);

	outf << "#include \"inter.h\"" << endl;
	outf << "STUB_VAR_DECL" << endl << endl;

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

    xpathObjTriggers = xmlXPathEvalExpression(xpathExprTriggers, xpathCtx);
    if(xpathObjTriggers == NULL) {
        cerr << "Error: unable to evaluate xpath expression \"" << xpathExprTriggers << "\"" << endl;
        xmlXPathFreeContext(xpathCtx); 
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

	print_triggers(xpathObjTriggers->nodesetval, outf);
	print_stubs(xpathObj->nodesetval, outf);

    /* Cleanup */
    xmlXPathFreeObject(xpathObj);
	xmlXPathFreeObject(xpathObjTriggers);
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);

    return compile_file(STUBC, STUBEX);
}

/***************************************************************************/
/*	run_subject(int argc, char** argv,                                 */
/*              char* preload_library, char *envp[])                       */
/*                                                                         */
/*	Runs subject program defined by (argc, argv[]) in the parent's     */
/*        (our) environment + LD_PRELOAD                                   */
/*                                                                         */
/*	Returns:                                                           */
/*		-1 - failed to start program                               */
/*		0  - child program exited normally with a 0 return code    */
/*		FAILURE_METRIC - child program exited normally with a non 0  */
/*                                                     return code         */
/*		CRASH_METRIC - child program was terminated by a signal    */
/***************************************************************************/
int run_subject(int argc, char** argv, char* preload_library, char *envp[])
{
#ifdef __APPLE__
    const char *preload = "DYLD_INSERT_LIBRARIES";
    const char *apple_env_flat = "DYLD_FORCE_FLAT_NAMESPACE";
    const char *apple_no_dyldcache = "DYLD_SHARED_REGION";
    /* this needs to be specified explictly in the flat namespace */
    const char *apple_explicit_libs = "/System/Library/Frameworks/ApplicationServices.framework/Versions/A/Frameworks/ATS.framework/Versions/A/Resources/libFontRegistry.dylib";
#else
    const char *preload = "LD_PRELOAD";
#endif
    char preload_path[1024];

    char **newarg;
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
    if (getcwd(preload_path, sizeof(preload_path) - strlen(preload_library) - 1))
    {
        strcat(preload_path, "/");
        strcat(preload_path, preload_library);
#if __APPLE__
        strlcat(preload_path, ":", sizeof(preload_path));
        strlcat(preload_path, apple_explicit_libs, sizeof(preload_path));
#endif
        cerr << "[LFI] Preloading " << preload_path << endl;

        newarg = (char**)malloc((argc+1)*sizeof(char*));
		for (i = 0; i < argc; ++i)
			newarg[i] = argv[i];
        newarg[argc] = NULL;

	gettimeofday(&tvstart, NULL);
        if (0 == (monitor = fork()))
        {
#ifdef __APPLE__
            setenv(apple_env_flat, "", 1);
            setenv(apple_no_dyldcache, "avoid", 1);
#endif
            setenv(preload, preload_path, 1);
            execv(argv[0], newarg);
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
							return_value = exit_status;
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
						
						return_value = 128+WTERMSIG(status);
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
    char *crash_create, *run_target, *token;
	char *run_argv[64];
	int run_argc;
	int status, crash_check, test_score;
	
	int c;

	crash_check = 0;
	run_target = NULL;
	
	opterr = 0;
	while ((c = getopt (argc, argv, "t:f:")) != -1)
	{
		switch (c)
		{
		case 'f':
		  crash_check = 1;
		  crash_create = optarg;
		  break;
		case 't':
		  run_target = optarg;
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
	if (optind >= argc)
	{
		usage(argv[0]);
		return -1;
	}

	//++optind
	run_argc = 0;
	token = strtok(run_target, "\t ");
	while (token)
	{
		run_argv[run_argc++] = token;
		token = strtok(NULL, "\t ");
	}
	
	LIBXML_TEST_VERSION
	status = generate_stub(argv[optind]);
	xmlCleanupParser();
	test_score = 0;
	if (run_target) {
		if (0 == status) {
			if ((test_score = run_subject(run_argc, run_argv, STUBEX, envp)) < 0)
				cerr << "A problem occurred starting the target" << endl;
		}
	}

	return test_score;
}
