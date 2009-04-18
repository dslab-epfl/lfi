// errordiff.cpp : Defines the entry point for the console application.
//

#include <set>
#include <iostream>
#include "man_errors.h"
#include "profiler_errors.h"

#ifdef SYSCALLS
#define MAGIC_END	0
#else
#define MAGIC_END	12345
#endif
using namespace std;

int main(int argc, char* argv[])
{
	int i, j, k, cnt, sum, fnfound;
	int found, missing, fp, accuracy;
	set<int> man_errors, profiler_errors;
	set<int>::iterator it;

	cnt = sum = 0;

	for (i = 0; i < sizeof(errors_man)/sizeof(errors_man[0]); ++i)
	{
		man_errors.clear();
		profiler_errors.clear();
		j = 0;
		fnfound = 0;
		while (errors_man[i].error_values[j] != MAGIC_END)
		{
			man_errors.insert(errors_man[i].error_values[j]);
			++j;
		}
		for (j = 0; j < sizeof(errors_profiler)/sizeof(errors_profiler[0]); ++j)
		{
#ifdef SYSCALLS
			if (errors_man[i].sysno == errors_profiler[j].sysno)
#else
			if (0 == strcmp(errors_man[i].name, errors_profiler[j].name))
#endif
			{
				fnfound = 1;
				k = 0;
				while (errors_profiler[j].error_values[k] != MAGIC_END)
				{
#ifdef SYSCALLS
					if (errors_profiler[j].error_values[k] < -500)
					{
						profiler_errors.insert(EINTR);
					}
					else
					{
						profiler_errors.insert(-errors_profiler[j].error_values[k]);
					}
#else
					profiler_errors.insert(errors_profiler[j].error_values[k]);
#endif
					++k;
				}
				break;
			}
		}
		if (fnfound)
		{
			found = missing = fp = 0;
			for (it = profiler_errors.begin(); it != profiler_errors.end(); ++it)
			{
				if (man_errors.find(*it) != man_errors.end())
				{
					++found;
				}
				else
				{
					++fp;
				}
			}
			missing = man_errors.size() - found;
			accuracy = (found) * 100 / ((fp+man_errors.size() > 0 ) ? (fp+man_errors.size()) : 1);
			if (!found && !fp && !man_errors.size())
				accuracy = 100;
#ifdef SYSCALLS
			cout << "|-\n| " << errors_man[i].sysno << " || " << found << " || " << missing << " || " << fp << " || " << accuracy << "%" << endl;
#else
			cout << "|-\n| " << errors_man[i].name << " || " << found << " || " << missing << " || " << fp << " || " << accuracy << "%" << endl;
#endif
			++cnt;
			sum += accuracy;
		}
	}
	cout << "Avg(accuracy): " << (float)sum/cnt << "% over " << cnt << " values" << endl;
	return 0;
}

