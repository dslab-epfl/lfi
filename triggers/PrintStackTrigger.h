
#include "../Trigger.h"

DEFINE_TRIGGER( PrintStackTrigger )
{
public:
  PrintStackTrigger();
  void Init(xmlNodePtr initData);
  bool Eval(const string* functionName, ...);
private:
  FILE* file;
};
