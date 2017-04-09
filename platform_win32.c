#define WIN32_LEAN_AND_MEAN
#define STRICT
#define UNICODE
#pragma warning(push)
#pragma warning(disable: 4005)
#include <Windows.h>
#pragma warning(pop)
#include <io.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "wse.h"

uint64_t platform_time_ms() {
	SYSTEMTIME systime;
	FILETIME	filetime;
	GetSystemTime(&systime);
	SystemTimeToFileTime(&systime, &filetime);
	return (((uint64_t)filetime.dwHighDateTime << 32) + filetime.dwLowDateTime) / 10000;
}

wchar_t *platform_normalize_path(wchar_t *filename) {
	for (wchar_t *p = filename; *p; p++)
		if (*p == '\\')
			*p = '/';
	return filename;
}

wchar_t **platform_list_directory(wchar_t *root, int *countp) {
	wchar_t *dir = root;
	wchar_t **queue = calloc(1, sizeof *queue);
	int queued = 1;
	queue[0] = wcsdup(root);
	
	wchar_t **list = calloc(1, sizeof *list);
	int count = 0;
	
	while (queued && (dir = queue[--queued])) {
		wchar_t spec[MAX_PATH * 2];
		swprintf(spec, MAX_PATH * 2, L"%ls/*", dir);
		
		struct _wfinddata64_t file;
		intptr_t handle = _wfindfirst64(spec, &file);
		
		if (handle < 0) return NULL;
		
		do {
			if (!wcscmp(L".", file.name) || !wcscmp(L"..", file.name))
				continue;
			if (file.attrib & _A_SUBDIR) {
				queue = realloc(queue, ++queued * sizeof *queue);
				wchar_t *next = malloc(MAX_PATH * 2);
				swprintf(next, MAX_PATH * 2, L"%ls/%ls", dir, file.name);
				queue[queued - 1] = next;
				continue;
			}
			list = realloc(list, (++count + 1) * sizeof *list);
			wchar_t *entry = malloc(MAX_PATH * 2);
			swprintf(entry, MAX_PATH * 2, L"%ls/%ls", dir, file.name);
			list[count - 1] = platform_normalize_path(entry);
		} while (!_wfindnext64(handle, &file));
		free(dir);
	}
	if (countp) *countp = count;
	list[count] = NULL;
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
	*wcsrchr(path, L'\\') = 0;
	return path;
}

void platform_cd(wchar_t *path) {
	SetCurrentDirectory(path);
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
