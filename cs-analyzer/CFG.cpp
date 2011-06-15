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

#include <fstream>
#include <iostream>
#include <string>
#include <queue>
#include <set>

#include "CFGBuilder.h"
#include <string.h>
#include <stdlib.h>

#define MAGIC_FNPTR_CALL	"call 00000000 <function_pointer_call>"

#ifdef __x86_64__
	#error "x86_64 target not supported"
#endif

using namespace std;

struct BFPathElement
{
	int bbId;
	string target;
	
	int bbIdParent;
	string targetParent;
};

struct RetCheckElement
{
	int bbId;
	set<string> retLocations;
	set<string> errnoLocations;
};

bool operator<(const BFPathElement& lhs, const BFPathElement& rhs)
{
	if (lhs.bbId < rhs.bbId)
		return true;
	if (lhs.bbId == rhs.bbId && lhs.target < rhs.target)
		return true;
	
	return false;
}

struct SideEffectElement
{
	int bbId;
	int type; /* 0 - global variable, 1 - argument */
	set<string> targets;
};


int CompareSEElements(const SideEffectElement& e1, const SideEffectElement& e2)
{
	set<string>::const_iterator it1, it2;

	for (it1 = e1.targets.begin(),it2 = e2.targets.begin();
		it1 != e1.targets.end() && it2 != e2.targets.end();
		++it1, ++it2)
	{
		if (*it1 < *it2)
			return -1;
		if (*it1 > *it2)
			return 1;
	}

	return 0;
}

bool operator<(const SideEffectElement& lhs, const SideEffectElement& rhs)
{
	if (lhs.bbId < rhs.bbId)
		return true;
	if (lhs.bbId == rhs.bbId && lhs.targets.size() < rhs.targets.size())
		return true;
	
	if (lhs.bbId == rhs.bbId && lhs.targets.size() == rhs.targets.size())
		return (-1 == CompareSEElements(lhs, rhs));
	
	return false;
}
void prel(ostream& outf, const SideEffectElement& e);

void prel(const SideEffectElement& e)
{
	return prel(cerr, e);
}

void prel(ostream& outf, const SideEffectElement& e)
{
	outf << "id " << e.bbId << endl;
	outf << "size " << e.targets.size() << endl;
	for (set<string>::const_iterator it = e.targets.begin(); 
		it != e.targets.end();
		++it)
	{
		outf << *it << " " << endl;
	}
}

bool IsIncluded(SideEffectElement& s, const SideEffectElement& e)
{
	set<string>::const_iterator it1;
	bool rv = true;

	it1 = e.targets.begin();
	
	for (; it1 != e.targets.end(); ++it1)
	{
		if (s.targets.insert(*it1).second)
			rv = false;
	}

	return rv;
}

typedef vector<BFPathElement> BFPath;

string GetText(const char* pszAsmPath)
{
	string line, contents;
	ifstream inf(pszAsmPath);

	if (inf.is_open())
	{
		while (!inf.eof() && inf.good())
		{
			getline(inf, line);
			contents += line;
			contents += "\n";
		}
	}
	return contents;
}

int IsRegisterx86(string target)
{
	return (target[0] != 'D' && target[0] != 'W' && target[0] != '[');
}

void CheckEax(CFGraph* graph, int start, vector<int>& checkedAgainst, vector<int>& errnoCheckedAgainst)
{
	CFGraph::AdjListElement* head;
	queue<RetCheckElement> bfQueue;
	
	RetCheckElement anElement;

	BasicBlock* aBB;
	list<string>::const_iterator itr;
	set<int> setVisited;

	ofstream outref;
	int directCalls = 0, indirectCalls = 0;
	
	string aTarget, anotherTarget, instruction;

	int i, j, ival;
	int retcall;

	checkedAgainst.clear();
	errnoCheckedAgainst.clear();
	if (head = graph->GetHead(start))
	{
		anElement.retLocations.insert("eax");
		do {
			anElement.bbId = head->m_bbIndex;
			bfQueue.push(anElement);
			setVisited.insert(anElement.bbId);
		} while (head = head->m_pNext);
	}
	while (!bfQueue.empty())
	{
		anElement = bfQueue.front();
		bfQueue.pop();

		aBB = graph->m_vBasicBlocks[anElement.bbId];

		retcall = 0;
		for (itr = aBB->m_listInstructions.begin();
			itr != aBB->m_listInstructions.end();
			++itr)
		{
			i = 0;
			while (' ' != (*itr)[++i]);
			instruction = itr->substr(0, i);

			if ("mov" == instruction)
			{
				/* moving to `target`? */
				i = 3;
				while(' ' == (*itr)[++i]);
				j = i;
				while(',' != (*itr)[++j]);
				aTarget = itr->substr(i, j-i);

				if (anElement.retLocations.find(aTarget) != anElement.retLocations.end())
				{
					anElement.retLocations.erase(aTarget);
					/* cerr << "removing " << aTarget << endl; */
				}

				/* moving a target somewhere? */
				while(' ' == (*itr)[++j]);
				i = j;
				while(/* ' ' != (*itr)[j] && */ '\0' != (*itr)[j]) ++j;
				anotherTarget = itr->substr(i, j-i);
				if (anElement.retLocations.find(anotherTarget) != anElement.retLocations.end())
				{
					anElement.retLocations.insert(aTarget);
					/* cerr << "adding " << aTarget << " due to " << anotherTarget << endl; */
				} else {
					/* cerr << "NOT adding " << aTarget << " due to " << anotherTarget << endl; */
				}
				if (anElement.errnoLocations.find(anotherTarget) != anElement.errnoLocations.end())
				{
					anElement.errnoLocations.insert(aTarget);
					/* cerr << "adding errno" << aTarget << " due to " << anotherTarget << endl; */
				} else {
					/* cerr << "NOT adding errno" << aTarget << " due to " << anotherTarget << endl; */
				}
			} else if ("lea" == instruction) {
				/* loading to `target`? */
				/* may also want to check if a watched location is loaded to target */
				i = 3;
				while(' ' == (*itr)[++i]);
				j = i;
				while(',' != (*itr)[++j]);
				aTarget = itr->substr(i, j-i);

				if (anElement.retLocations.find(aTarget) != anElement.retLocations.end())
				{
					anElement.retLocations.erase(aTarget);
				}
			} else if ("call" == instruction) {
				if (anElement.retLocations.find("eax") != anElement.retLocations.end())
				{
					anElement.retLocations.erase("eax");
				}
				// check for errno
				i = 4;
				while(' ' == (*itr)[++i]);
				while(' ' != (*itr)[i++]);
				j = i+1;
				while('\0' != (*itr)[++j]);
				aTarget = itr->substr(i, j-i);

				if ("<__errno_location@plt>" == aTarget) {
					// errno is retrieved
					anElement.errnoLocations.insert("DWORD PTR [eax]");
				}
			} else if ("ret" == instruction) {
				if (anElement.retLocations.find("eax") != anElement.retLocations.end())
				{
					checkedAgainst.push_back(-667);
				}
			} else if ("cmp" == instruction || "test" == instruction) {
				/* moving to `target`? */
				i = 3;
				if ("test" == instruction)
					++i;
				while(' ' == (*itr)[++i]);
				j = i;
				while(',' != (*itr)[++j]);
				aTarget = itr->substr(i, j-i);

				/* moving a target somewhere? */
				while(' ' == (*itr)[++j]);
				i = j;
				while(' ' != (*itr)[j] && '\0' != (*itr)[j]) ++j;
				anotherTarget = itr->substr(i, j-i);

				/* cerr << instruction << " " << aTarget << " " << anotherTarget << endl;*/
				
				/* test eax, eax for example */
				if ("test" == instruction &&
					aTarget == anotherTarget && 
					anElement.retLocations.find(aTarget) != anElement.retLocations.end()) {
						// cerr << "comparing to 0" << endl;
						checkedAgainst.push_back(0);
				} else {
					if (anElement.retLocations.find(aTarget) != anElement.retLocations.end())
					{
						// cerr << "comparing to " << anotherTarget << endl;
						if (anotherTarget.size() < 3 || anotherTarget[0] != '0')
							cerr << "Comparing to non-const val: " << anotherTarget << endl;
						else {
							checkedAgainst.push_back(strtoul(anotherTarget.c_str(), NULL, 16));
							// cerr << "pushing " << anotherTarget << endl;
						}
					}
					if (anElement.retLocations.find(anotherTarget) != anElement.retLocations.end())
					{
						// cerr << "comparing to " << aTarget << endl;
						if (aTarget.size() < 3 || aTarget[0] != '0')
							cerr << "Comparing to non-const val: " << aTarget << endl;
						else {
							checkedAgainst.push_back(strtoul(aTarget.c_str(), NULL, 16));
							// cerr << "pushing " << aTarget << endl;
						}
					}

					//same thing for errno
					if (anElement.errnoLocations.find(aTarget) != anElement.errnoLocations.end())
					{
						// cerr << "comparing to " << anotherTarget << endl;
						if (anotherTarget.size() < 3 || anotherTarget[0] != '0')
							cerr << "Comparing to non-const val: " << anotherTarget << endl;
						else {
							errnoCheckedAgainst.push_back(strtoul(anotherTarget.c_str(), NULL, 16));
							// cerr << "pushing " << anotherTarget << endl;
						}
					}
					if (anElement.errnoLocations.find(anotherTarget) != anElement.errnoLocations.end())
					{
						// cerr << "comparing to " << aTarget << endl;
						if (aTarget.size() < 3 || aTarget[0] != '0')
							cerr << "Comparing to non-const val: " << aTarget << endl;
						else {
							errnoCheckedAgainst.push_back(strtoul(aTarget.c_str(), NULL, 16));
							// cerr << "pushing " << aTarget << endl;
						}
					}

				}
			}
		}
		
		if (head = graph->GetHead(anElement.bbId))
		{
			do {
				anElement.bbId = head->m_bbIndex;
				if (anElement.bbId != EXIT_NODE && setVisited.insert(anElement.bbId).second)
				{
					bfQueue.push(anElement);
				}
			} while (head = head->m_pNext);
		}
	}
}

bool get_file_line(char* binary, unsigned long address, char* src, int* line)
{
	int pipefd[2];
	char addr2linecmd[1024];
	
	char buffer[1024];
	ssize_t readc;

	char* colon;

	src[0] = 0;
	if (-1 != pipe(pipefd)) {
		sprintf(addr2linecmd, "addr2line -e %s %lx 1>&%d", binary, address, pipefd[1]);
		system(addr2linecmd);
		close(pipefd[1]);

		readc = read(pipefd[0], buffer, sizeof(buffer));
		buffer[readc] = 0;
		close(pipefd[0]);

		colon = strchr(buffer, ':');
		if (colon) {
			colon[0] = 0;
			strcpy(src, buffer);
			*line = atoi(colon+1);
		}
	}

	if (0 == src[0] || 0 == strcmp(src, "??"))
		return false;
	return true;
}

int get_times_executed(char* file, int line)
{
	char* slash;
	char gcov[1024], dir[1024] = "";

	if ('/' == file[0]) {
		// good, absolute path
		slash = strrchr(file, '/');
		slash[0] = '\0';
		strcpy(dir, file);
		slash[0] = '/';

		strcpy(gcov, file);
	} else {
		
	}

	strcpy(gcov, file);
	strcat(gcov, ".gcov");

	char cmdline[1024];
	int pipefd[2];
	char buffer[1024];
	ssize_t readc;
	
	char *colon;
	int execcountidx;
	int execcount = -1;

	if (-1 != pipe(pipefd)) {
		sprintf(cmdline, "cd %s; gcov %s > /dev/null", dir, file);
		system(cmdline);

		sprintf(cmdline, "cat %s 2>/dev/null | grep %d: 1>&%d", gcov, line, pipefd[1]);
		system(cmdline);
		close(pipefd[1]);

		readc = read(pipefd[0], buffer, sizeof(buffer));
		buffer[readc] = 0;
		close(pipefd[0]);
		
		colon = strchr(buffer, ':');
		if (colon) {
			if ('#' == *(colon-1)) {
				// not executed
				execcount = 0;
			} else if ('-' == *(colon-1)) {
				// not executable - shouldn't happen
			} else {
				execcountidx = 1;
				while (colon-execcountidx > &buffer[0] && isdigit(*(colon-execcountidx)))
					execcountidx++;
				execcount = atoi(colon-execcountidx);
			}
		}
	} else {
		perror("pipe");
	}
	return execcount;
}

int main(int argc, char* argv[])
{
	CFGraph *graph, *tgraph;
	char *targetfn = NULL;
	char *infile = NULL;
	const char *defretval = "-1";
	const char *deferrno = "EINVAL";
	const char* fninterestdefault[] = {
		"opendir",
		"getcwd",
		"fdopen",
		"popen",
		"getlogin",
		"cuserid",
		"getspnam",
		"getspent"
	};
	vector<string> fninterest;
	
	CCFGBuilder builder;
	string asma;

	if (argc <= 2) {
		cerr << argv[0] << " <target executable> <function>|- [default retval] [default errno]" << endl;
		exit(1);
	}

	infile = argv[1];
	targetfn = argv[2];
	if (targetfn[0] != '-') {
		fninterest.push_back(targetfn);
	} else {
		int i;
		for (i = 0; i < sizeof(fninterestdefault)/sizeof(fninterestdefault[0]); ++i)
			fninterest.push_back(fninterestdefault[i]);
	}
	if (argc > 3)
		defretval = argv[3];
	if (argc > 4)
		deferrno = argv[4];
	char objdumpcmd[1024];
	sprintf(objdumpcmd, "objdump -d -M intel %s > x.asm", infile);

	system(objdumpcmd);

	asma = GetText("x.asm");
	if (asma.empty())
	{
		cerr << "Invalid or non-existing file: " << infile << endl;
		return -1;
	}
	system("rm -f x.asm");
	int cc = 1;
	long int callAddress = 0;
	int triggerc = 0;
	vector<int> checkedAgainst, errnoCheckedAgainst;

	cout << "<plan>" << endl;
	for (vector<string>::const_iterator it = fninterest.begin(), itend = fninterest.end();
		it != itend; ++it) {
		cc = 1;
		cerr << "Analyzing calls to function " << *it << endl;
	while(1) {
		graph = builder.Parse(asma.c_str(), it->c_str(), cc, &callAddress);
		if (!graph) {
			cerr << "function " << *it << " referenced " << dec << cc-1 << " times" << endl << endl;
			break;
		}
		cc++;

		CFGraph::AdjListElement* head;
		int i = 0;
		set<int> targetBBs;
		list<string>::const_iterator its;

		char* fn = strrchr(infile, '/');
		if (!fn) fn = infile;
		else fn = fn + 1;

		ofstream outf;
		int a, b;
		char addr2linecmd[1024];
		char src[1024];
		int line, lineexec;

		CheckEax(graph, 0, checkedAgainst, errnoCheckedAgainst);
		if (checkedAgainst.size()) {
			
			cerr << "Call at address " << hex << callAddress << " (";
			if (get_file_line(infile, callAddress, src, &line)) {
				cerr << src << ":" << dec << line;
				lineexec = get_times_executed(src, line);
				if (-1 == lineexec) {
					cerr << " - failed to retrieve line execution count. gcov file missing?";
				} else {
					cerr << " - exec " << lineexec << " times";
				}
			} else
				cerr << "no line number information";
			
			cerr << ") checks return value against: ";

			for (vector<int>::const_iterator it = checkedAgainst.begin(), itend = checkedAgainst.end();
				it != itend; ++it)
				if (*it == -667)
					cerr << "[ret] ";
				else
					cerr << dec << *it << " ";
			cerr << endl;

			if (errnoCheckedAgainst.size()) {
				cerr << "errno is checked against: ";
				for (vector<int>::const_iterator it = errnoCheckedAgainst.begin(), itend = errnoCheckedAgainst.end();
					it != itend; ++it)
						cerr << dec << *it << " ";
				cerr << endl;
			}
		} else {
			cerr << "call at address " << hex << callAddress << " (";
			if (get_file_line(infile, callAddress, src, &line)) {
				cerr << src << ":" << dec << line;
				lineexec = get_times_executed(src, line);
				if (-1 == lineexec) {
					cerr << " - failed to retrieve line execution count. gcov file missing?";
				} else {
					cerr << " - exec " << lineexec << " times";
				}
			} else {
				cerr << "no line number information";
				lineexec = -1; // force trigger
			}

			cerr << ") does not check the return value" << endl;

			if (lineexec) {
				cout << "  <trigger id=\"" << hex << callAddress << "\" class=\"CallStackTrigger\">" << endl;
				cout << "    <args>" << endl;
				cout << "      <frame>" << endl;
				cout << "        <module>" << infile << "</module>" << endl;
				cout << "        <offset>" << hex << callAddress << "</offset>" << endl;
				cout << "      </frame>" << endl;
				cout << "    </args>" << endl;
				cout << "  </trigger>" << endl;

				cout << "  <function name=\"" << it->c_str() << "\" retval=\"" << defretval << "\" errno=\"" << deferrno << "\">" << endl;
				cout << "    <triggerx ref=\"" << hex << callAddress << "\" />" << endl;
				cout << "  </function>" << endl;
			}
		}
	} // while(1)
	} // for
	cout << "</plan>" << endl;

	return 0;
}

