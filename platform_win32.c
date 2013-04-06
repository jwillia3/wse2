#define WIN32_LEAN_AND_MEAN
#define STRICT
#define UNICODE
#include <Windows.h>
#include "wse.h"

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