
#include "../Trigger.h"
#include <pthread.h>

//#define exePath    "/home/paul/mysql-5.1.44/sql/mysqld"

DEFINE_TRIGGER( AfterUnlockTrigger )
{
public:
  struct UnlockInfo {
    char file[256];
    int line;
  };

public:
  AfterUnlockTrigger();
  void Init(xmlNodePtr initData);
  bool Eval(const string& functionName, ...);

private:
  int lineCount;
  string exePath;
  map<pthread_t, UnlockInfo> lastUnlockInfo;
};
