//
// DIRCMD loops through a filemask in a single folder and runs your
// command for each file found. 
// 1.3 add subfolder support
//

#include <afxwin.h>

int threads = 1;
int arg=1;
bool recurse = false;
bool verbose = true;

int doFolder(const char *path, int argc, char* argv[]) {
    char pathbuf[4096];

    if (recurse) {
        // do subfolders first
        sprintf(pathbuf, "%s\\*", path);
        WIN32_FIND_DATA data;
	    HANDLE hSrch=FindFirstFile(pathbuf, &data);
        if (hSrch != INVALID_HANDLE_VALUE) do {
            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if ((0 == strcmp(data.cFileName,".")) || (0 == strcmp(data.cFileName,".."))) continue;
                if (data.dwFileAttributes&FILE_ATTRIBUTE_SYSTEM) continue;

                sprintf(pathbuf, "%s\\%s", path, data.cFileName);
                if (doFolder(pathbuf, argc, argv) < 0) return -1;
            }
        } while (FindNextFile(hSrch, &data));
		FindClose(hSrch);
		hSrch=INVALID_HANDLE_VALUE;
    }

    sprintf(pathbuf, "%s\\%s", path, argv[arg]);
    if (verbose) printf("Checking %s\n", pathbuf);
    WIN32_FIND_DATA data;
	HANDLE hSrch=FindFirstFile(pathbuf, &data);
	HANDLE processwait[MAXIMUM_WAIT_OBJECTS];
	HANDLE dummywait = CreateMutex(NULL, FALSE, NULL);

	for (int idx=0; idx<MAXIMUM_WAIT_OBJECTS; idx++) {
		processwait[idx] = dummywait;
	}

	while (INVALID_HANDLE_VALUE != hSrch) {
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
					if (threads > 1) if (verbose) printf("* DEBUG * waiting for free slot\n");
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
                CString nameonly = data.cFileName;
                int p = nameonly.ReverseFind('.');
                if (p != -1) {
                    nameonly = nameonly.Left(p);
                }

				if (i>arg+1) cmd+=" ";
				tmp=argv[i];
				tmp.Replace("$f", data.cFileName);
				tmp.Replace("$p", path);
                tmp.Replace("$n", nameonly);
				cmd+=tmp;
			}
 
			if (verbose) printf(">%s\n", (const char*)cmd);

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
			FindClose(hSrch);
			hSrch=INVALID_HANDLE_VALUE;
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

int main(int argc, char* argv[])
{
	if (argc < 3) {
		printf("DirCmd <dirspec> <command...>\n");
		printf("  ie: dircmd [/s] [/q] [-x] *.txt type $f\n");
        printf("  /s to recurse subfolders\n");
		printf("  'x' is number of threads to run, 1-%d\n", MAXIMUM_WAIT_OBJECTS);
        printf("  Note: use the number, like -9, not '-x' literally\n");
		printf("  Replacements: $f - current filename\n");
        printf("  Replacements: $p - current path\n");
        printf("  Replacements: $n - current name (filename with no extension)\n");
        printf("  v1.4\n");
		return 1;
	}

    // order matters
    if (0 == strcmp(argv[arg], "/s")) {
        recurse = true;
        ++arg;
    }

    if (0 == strcmp(argv[arg], "/q")) {
        verbose = false;
        ++arg;
    }

	if (argv[arg][0] == '-') {
		threads = atoi(&argv[arg][1]);
		if (threads < 1) {
			printf("Thread count out of range.\n");
			return -1;
		}
		if (threads > MAXIMUM_WAIT_OBJECTS) {
			printf("Thread count too large, limiting to %d\n", MAXIMUM_WAIT_OBJECTS);
			threads = MAXIMUM_WAIT_OBJECTS;
		}
		++arg;
	}

    return doFolder(".", argc, argv);
}
