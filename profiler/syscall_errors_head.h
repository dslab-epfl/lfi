#include "errno.h"

struct sys_errors {
	int sysno;
	int *error_values;
};
