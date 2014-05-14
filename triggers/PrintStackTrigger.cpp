
#include "PrintStackTrigger.h"
#include <string.h>
#include <execinfo.h>
#include <iostream>

PrintStackTrigger::PrintStackTrigger()
{
}

void PrintStackTrigger::Init(xmlNodePtr initData)
{
  xmlNodePtr nodeElement, textElement;

  nodeElement = initData->children;
  while (nodeElement)
  {
    if (XML_ELEMENT_NODE == nodeElement->type &&
      !xmlStrcmp(nodeElement->name, (const xmlChar*)"file"))
    {
      textElement = nodeElement->children;
      if (XML_TEXT_NODE == textElement->type)
      {
        remove((char*)textElement->content);
        file = fopen((char*)textElement->content,"a");        
      }
    }
    nodeElement = nodeElement->next;
  }
}

bool PrintStackTrigger::Eval(const string*, ...)
{
  void *array[10];
    size_t size;

  size = backtrace(array, 10);  
  backtrace_symbols_fd(array, size, fileno(file));
  fclose(file);
  return true;
}
