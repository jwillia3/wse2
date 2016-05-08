#define WIN32_LEAN_AND_MEAN
#define STRICT
#define UNICODE
#pragma warning(push)
#pragma warning(disable: 4005)
#include <Windows.h>
#pragma warning(pop)
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "wse.h"

wchar_t **platform_list_directory(wchar_t *root, int *countp) {
	wchar_t *directory = wcsdup(root);
	wchar_t *queue[256];
	wchar_t **list = NULL;
	int count = 0;
	int queue_count = 0;
	do {
		WIN32_FIND_DATA data;
		wchar_t search[MAX_PATH];
		wcscpy(search, directory);
		wcscat(search, L"/*");
		HANDLE h = FindFirstFile(search, &data);
	    if (h != INVALID_HANDLE_VALUE) {
	        do {
				list = realloc(list, (count + 2) * sizeof *list);
				wchar_t full[MAX_PATH * 2];
				swprintf(full, MAX_PATH * 2, L"%ls/%ls", directory, data.cFileName);
				if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
					if (wcscmp(data.cFileName, L".") && wcscmp(data.cFileName, L"..")) {
						if (queue_count < sizeof queue / sizeof *queue)
							queue[queue_count++] = wcsdup(full);
					}
				} else {
					for (wchar_t *p = full; *p; p++) if (*p == '\\') *p = '/';
					list[count++] = wcsdup(full);
					list[count] = NULL;
				}
	        } while (FindNextFile(h, &data));
	        FindClose(h);
	    }
		free(directory);
	} while (queue_count-- && (directory = queue[queue_count]));
	if (countp) *countp = count;
	return list;
}

void *
platform_openfile(wchar_t *fn, int write, int *sz) {
    HANDLE f;
    if (write)
        f = CreateFile(fn, GENERIC_WRITE, 0, 0,
    		CREATE_ALWAYS, 0, 0);
    else
        f = CreateFile(fn, GENERIC_READ,
    		FILE_SHARE_READ|FILE_SHARE_WRITE, 0,
    		OPEN_EXISTING, 0, 0);
	if (f==INVALID_HANDLE_VALUE)
		return 0;
    if (!write)
        *sz = GetFileSize(f, 0);
    return f;
}
void
platform_closefile(void *f) {
    CloseHandle(f);
}

void
platform_writefile(void *f, void *buf, int sz) {
    DWORD ignore;
    WriteFile(f,buf,sz,&ignore,0);
}

void
platform_readfile(void *f, void *buf, int sz) {
    DWORD ignore;
    ReadFile(f,buf,sz,&ignore, 0);
}

wchar_t*
platform_bindir(wchar_t *path) {
    GetModuleFileName(0, path, 256);
    return path;
}

wchar_t *
wcsistr(wchar_t *big, wchar_t *substring) {
	wchar_t *i, *j;
	if (!*substring)
		return 0;
	for ( ; *big; big++)
		for (i=big, j=substring; ; i++, j++)
			if (!*j)
				return big;
			else if (!*i)
				return 0;
			else if (towlower(*i) != towlower(*j))
				break;
	return 0;
}
