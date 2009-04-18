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
#include <queue>
#include <string>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

using namespace std;

#define PROFILEREX			"profiler"
#define REFFILE				"function_references"
#define DASMPATH			"disassembly/"
#define PROFILEPATH			"profiles/"
#define MAGIC_FNPTR_CALL	"function_pointer_call"

char targetLibrary[1024];

void usage(char* me)
{
    cout << "Usage: ";
    cout << me << " <function name>" << endl;
}

void getReturnValues(const char* function);


int isIntrinsic(string function)
{
	return 0;
}

void appendInstrinsic(const ostream& out, string function)
{
}

void handleReference(const char* function, const char* address, const char* profile)
{
	/*
		Logic:
		1. PROFILEPATH/function exists?
			1.1 No  -> {
				- generate the DASMPATH/function (or assume it already exists in the current implementation)
				- call getReturnValues to generate PROFILEPATH/function
				}
		2. Append PROFILEPATH/function to profile
	*/

	string dasmPath(DASMPATH), profilePath(PROFILEPATH), line;
	char dasmCmd[1024];
	ifstream infProfile, infDasm;
	ofstream outfProfile;
	size_t index;
	int startOffset, endOffset;
	string actual_function = function;

	/* check if this is a call to a @plt function and doesn't have an offset
	 * strip the @plt part
	 */
	if (string::npos == actual_function.find("+") && string::npos != (index = actual_function.find("@plt")))
	{
		actual_function = actual_function.substr(0, index);
	}


	dasmPath += actual_function;
	profilePath += actual_function;

	cerr << "handleReferece to " << function << ". appending to " << profile << endl;
	if (isIntrinsic(actual_function))
	{
		outfProfile.open(profile, ios_base::out|ios_base::app);
		appendInstrinsic(outfProfile, actual_function);
	}
	else
	{
		infProfile.open(profilePath.c_str());
		if (!infProfile.is_open())
		{
			infDasm.open(dasmPath.c_str());
			if (!infDasm.is_open())
			{
				cerr << "WARNING: Disassembly not found for " << function << " trying to disassemble code. Return values set may be incomplete" << endl;
				
				startOffset = strtol(address, (char**)NULL, 16);
				/* assume function is at most 30K */
				endOffset = startOffset + 30720;

				sprintf(dasmCmd, "objdump -d -M intel --start-address=%d --stop-address=%d %s | egrep -v \"(^Disassembly|^/|^$|efi-app-ia32)\" > %s%d.tmp", startOffset, endOffset, targetLibrary, DASMPATH, startOffset);
				cerr << "Executing " << dasmCmd << endl;
				system(dasmCmd);
				
				sprintf(dasmCmd, "%d.tmp", startOffset);
				actual_function = dasmCmd;
				profilePath = PROFILEPATH;
				profilePath += actual_function;

				sprintf(dasmCmd, "%s%d.tmp", DASMPATH, startOffset);

				infDasm.open(dasmCmd);
			}
			if (infDasm.is_open())
			{
				getReturnValues(actual_function.c_str());
				infProfile.clear();
				infProfile.open(profilePath.c_str());
			}
		}

		if (infProfile.is_open())
		{
			outfProfile.open(profile, ios_base::out|ios_base::app);
			while (1)
			{
				getline(infProfile, line);
				if (infProfile.eof())
					break;
				outfProfile << line << endl;
			}
		}
	}
	cerr << "handleReferece to " << function << "(" << actual_function << ") - done" << endl;
}

void getReturnValues(const char* function)
{
	/*
		we're now asssuming DASMPATH/function exists (dasmfn.sh can be run to generate the files)
		on-demand disassembly is to be implemented in a future version
	*/
	/*
		We're now
		1. running PROFILEREX "DASMPATH/function" "REFFILE" > "PROFILEPATH/function"
		2. foreach (referenced_function in REFFILE) {
			getReturnValues(referenced_function);
			PROFILEPATH/function += PROFILEPATH/referenced_function
		}
		3. handle circular references (if a circular reference exists,
		all functions involved have the same possible return values) - TO BE IMPLEMENTED
	*/

	string dasmPath(DASMPATH), profilePath(PROFILEPATH), reference, reference_address;
	pid_t pid;
	char buf, profiler_path[512], reffile[512];
	int pfd[2];
	int out_file;

	dasmPath += function;
	profilePath += function;

	sprintf(reffile, "%s_%s", REFFILE, function);

	if (-1 == pipe(pfd))
	{
		perror("pipe");
		exit(EXIT_FAILURE);
	}
	if (-1 == (pid = fork()))
	{
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if (0 == pid)
	{
		cerr << "Child starting..." << endl;
		/* close reading end */
		close(pfd[0]);
		/* redirect stdout to pfd[1] */
		dup2(pfd[1], STDOUT_FILENO);
		/* no need for pfd[1] anymore */
		close(pfd[1]);
		
		if (getcwd(profiler_path, sizeof(profiler_path) - strlen(PROFILEREX)))
		{
			strcat(profiler_path, "/");
			strcat(profiler_path, PROFILEREX);
			cerr << "Executing " << profiler_path << " " << dasmPath << endl;
			execlp(profiler_path, PROFILEREX, dasmPath.c_str(), reffile, (char*)NULL);
			perror("execlp");
			cerr << "ERROR: profiler could not be executed" << endl;
		}
		_exit(EXIT_FAILURE);
	} else {
		/* close write end */
		close(pfd[1]);

		out_file = open(profilePath.c_str(), O_CREAT|O_WRONLY|O_TRUNC, 0644);
		while (read(pfd[0], &buf, 1) > 0)
		{
			write(out_file, &buf, 1);
		}

		close(pfd[0]);
		close(out_file);
		
		wait(NULL);

		cerr << "Profiler done" << endl;
	}

	ifstream inf(reffile);

	if (inf.is_open())
	{
		while (1) {
			inf >> reference >> reference_address >> reference;
			if (!inf.eof())
			{
				reference = reference.substr(1, reference.size()-2);
				cerr << reference_address << " " << function << endl;
				if (0 == strcmp(reference.c_str(), MAGIC_FNPTR_CALL))
				{
					cerr << "WARNING. fnptr call detected referencing " << reference << endl;
				} else if (0 == strcmp(reference.c_str(), function) ||
						   (strtol(reference_address.c_str(), (char**)NULL, 16) == atoi(function) && atoi(function) != 0))
				{
					/* recursive call. ignore */
				} else {
					cerr << "referencing " << reference << endl;
					handleReference(reference.c_str(), reference_address.c_str(), profilePath.c_str());
				}
			} else
				break;
		}
		inf.close();
		unlink(reffile);
	} else {
		cerr << "WARNING: reference file " << reffile << " not found" << endl;
	}
	cerr << "after profiler done" << endl;
}

int main(int argc, char* argv[], char* envp[])
{
	int c;

	opterr = 0;

	while ((c = getopt (argc, argv, "rf:t:")) != -1)
		switch (c)
		{
		case 'r':
			break;
		case 'f':
			break;
		case 't':
			break;
		case '?':
			if (optopt == 'f')
				fprintf (stderr, "Option -f requires an argument.\n");
			if (optopt == 't')
				fprintf (stderr, "Option -t requires an argument.\n");
			else if (isprint (optopt))
				fprintf (stderr, "Unknown option `-%c'.\n", optopt);
			else
				fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
			return 1;
		default:
			abort ();
		}

	if (argc > 2)
	{
		strncpy(targetLibrary, argv[2], sizeof(targetLibrary));
		getReturnValues(argv[1]);
	}
	else
		usage(argv[0]);
    return 0;
}
