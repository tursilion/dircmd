//
// DIRCMD loops through a filemask in a single folder and runs your
// command for each file found. 
//

#include <afxwin.h>

int threads = 1;
int arg=1;

int main(int argc, char* argv[])
{
	if (argc < 3) {
		printf("DirCmd <dirspec> <command...>\n");
		printf("  ie: dircmd [-x] *.txt type $f\n");
		printf("  'x' is number of threads to run, 1-%d\n", MAXIMUM_WAIT_OBJECTS);
		printf("  Replacements: $f - current filename\n");
		printf("  v1.2\n");
		return 1;
	}

	if (argv[1][0] == '-') {
		threads = atoi(&argv[1][1]);
		if ((threads < 1) || (threads > MAXIMUM_WAIT_OBJECTS)) {
			printf("Thread count out of range.\n");
			return -1;
		}
		arg=2;
	}

	WIN32_FIND_DATA data;
	HANDLE hSrch=FindFirstFile(argv[arg], &data);
	HANDLE processwait[MAXIMUM_WAIT_OBJECTS];
	HANDLE dummywait = CreateMutex(NULL, FALSE, NULL);

	for (int idx=0; idx<MAXIMUM_WAIT_OBJECTS; idx++) {
		processwait[idx] = dummywait;
	}

	while (NULL != hSrch) {
		CString cmd;
		int slot = -1;

		while (slot == -1) {
			for (int idx=0; idx<threads; idx++) {
				if (processwait[idx] == dummywait) {
					slot = idx;
					break;
				}
			}

			if (slot == -1) {
				DWORD ret = WaitForMultipleObjects(threads, processwait, FALSE, 1000);
				if (WAIT_TIMEOUT == ret) {
					if (threads > 1) printf("* DEBUG * waiting for free slot\n");
					ret = WaitForMultipleObjects(threads, processwait, FALSE, INFINITE);
				}

				if (WAIT_FAILED == ret) {
					printf("** WAIT FAILED - aborting.\n");
					return -1;
				}
				ret -= WAIT_OBJECT_0;
				CloseHandle(processwait[ret]);
				processwait[ret] = dummywait;
			}
		}

		if (!(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)) {
			cmd="";
			for (int i=arg+1; i<argc; i++) {
				CString tmp;

				if (i>arg+1) cmd+=" ";
				tmp=argv[i];
				tmp.Replace("$f", data.cFileName);
				cmd+=tmp;
			}
 
			printf(">%s\n", (const char*)cmd);

			STARTUPINFO info;
			PROCESS_INFORMATION process;
			memset(&info, 0, sizeof(STARTUPINFO));
			info.cb=sizeof(STARTUPINFO);

			if (!CreateProcess(NULL, (char*)(const char*)cmd, NULL, NULL, TRUE, 0, NULL, NULL, &info, &process)) {
				printf("Failed to start process!\n");
			} else {
				CloseHandle(process.hThread);
				processwait[slot] = process.hProcess;
			}
		}

		if (!FindNextFile(hSrch, &data)) {
			CloseHandle(hSrch);
			hSrch=NULL;
		}
	}

	// wait for all processes to exit
	// this is easy - it doesn't matter what order we wait in
	for (int idx=0; idx<threads; idx++) {
		if (processwait[idx] != dummywait) {
			WaitForSingleObject(processwait[idx], INFINITE);
			CloseHandle(processwait[idx]);
			processwait[idx] = dummywait;
		}
	}
	CloseHandle(dummywait);

	return 0;
}
