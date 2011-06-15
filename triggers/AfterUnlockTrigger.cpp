
#include "AfterUnlockTrigger.h"
#include <fstream>
#include <stdarg.h>
#include <execinfo.h>
#include <string.h>

using namespace std;

AfterUnlockTrigger::AfterUnlockTrigger()
{
}

void AfterUnlockTrigger::Init(xmlNodePtr initData)
{
	xmlNodePtr nodeElement, textElement;

	nodeElement = initData->children;
	while (nodeElement)
	{
		if (XML_ELEMENT_NODE == nodeElement->type)
		{
			textElement = nodeElement->children;
			if (XML_TEXT_NODE == textElement->type) {
				if (!xmlStrcmp(nodeElement->name, (const xmlChar*)"lines"))
					lineCount = atoi((char*)textElement->content);
				else if (!xmlStrcmp(nodeElement->name, (const xmlChar*)"module"))
					exePath = (char*)textElement->content;
			}
		}
		nodeElement = nodeElement->next;
	}
}

struct layout
{
  struct layout *ebp;
  void *ret;
};


bool get_file_line(const char* binary, unsigned long address, char* src, int* line)
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


bool AfterUnlockTrigger::Eval(const string& functionName, ...)
{
	map<pthread_t, UnlockInfo>::iterator it;
	UnlockInfo ui;
	pthread_t self;

	// XXX assuming stack frames & arch dependent
	register void *ebpx __asm__ ("ebp");
	struct layout *ebp = (struct layout *)ebpx;

	// reaching to the function calling the interceptor
	ebp = ebp->ebp->ebp;

	// for mysql, go one more time to skip the wrappers
	ebp = ebp->ebp;
	
	if (functionName == "pthread_exit")
	{
		it = lastUnlockInfo.find(self);
		if (it != lastUnlockInfo.end())
			lastUnlockInfo.erase(it);
	}
	// run addr2line -e executable ebp->ret
	else if (get_file_line(exePath.c_str(), (unsigned long)ebp->ret, ui.file, &ui.line)) {
		self = pthread_self();

		if (functionName == "pthread_mutex_unlock")
		{
			lastUnlockInfo[self] = ui;
		}
		else
		{
			it = lastUnlockInfo.find(self);
/*			
			if (it != lastUnlockInfo.end())
			{	ofstream outf("/home/paul/kk");
				outf << "uTrigger called for " << ui.file << ":" << ui.line << endl;
			
				outf << "last unlock at " << it->second.file << ":" << it->second.line << endl;
				outf.close();
			}
*/			
			if (it != lastUnlockInfo.end()) {
				if (0 == strcmp(it->second.file, ui.file) && 
					ui.line - it->second.line < lineCount)
					return true;
			}
		}
	}

	return false;
}
