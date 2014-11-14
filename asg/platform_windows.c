#define UNICODE
#define WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>
#pragma comment(lib, "advapi32")
#include "asg.h"
#include "platform.h"

typedef struct {
    HANDLE file;
    HANDLE mapping;
    HANDLE view;
} Host;

static void *load_file(Host *host, const wchar_t *filename) {
    host->file = CreateFile(filename,
        GENERIC_READ,
        FILE_SHARE_DELETE | FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);
    if (host->file == INVALID_HANDLE_VALUE)
        return NULL;
    size_t size = GetFileSize(host->file, NULL);
    
    host->mapping = CreateFileMapping(host->file,
        NULL,
        PAGE_READONLY,
        0, size,
        NULL);
    
    if (!host->mapping) {
        CloseHandle(host->file);
        return NULL;
    }
    
    host->view = MapViewOfFile(host->mapping, FILE_MAP_READ, 0, 0, 0);
    if (!host->view) {
        CloseHandle(host->mapping);
        CloseHandle(host->file);
        return NULL;
    }
    
    return host->view;
}

static void free_file_mapping(Host *host) {
    UnmapViewOfFile(host->view);
    CloseHandle(host->mapping);
    CloseHandle(host->file);
}

void platform_scan_directory(const wchar_t *dir, void per_file(const wchar_t *name, void *data)) {
    WIN32_FIND_DATA data;
    if (!dir)
        dir = L"C:\\Windows\\Fonts";
    
    wchar_t search[MAX_PATH];
    if (wcslen(dir) >= MAX_PATH - 2)
        return;
    wcscpy(search, dir);
    wcscat(search, L"/*");
    
    HANDLE h = FindFirstFile(search, &data);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            wchar_t full[MAX_PATH*2];
            Host host;
            
            swprintf(full, MAX_PATH*2, L"%ls\\%ls", dir, data.cFileName);
            if (load_file(&host, full)) {
                per_file(full, host.view);
                free_file_mapping(&host);
            }
        } while (FindNextFile(h, &data));
        FindClose(h);
    }
}

static void host_free(AsgFont *font) {
    free_file_mapping(font->host);
    free(font->host);
}

wchar_t **platform_list_fonts(int *countp) {
    const wchar_t *path = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";
    
    wchar_t **out = calloc(1, sizeof *out);
    int nout = 0;
    
    HKEY key = NULL;
    RegOpenKey(HKEY_LOCAL_MACHINE, path, &key);
    
    DWORD nchild;
    RegQueryInfoKey(key, 0,0, 0, 0,0, 0, &nchild, 0, 0, 0, 0);
    
    AsgFont *font = NULL;
    
    for (int i = 0; i < nchild; i++) {
        wchar_t name_buffer[4096];
        wchar_t *name = name_buffer;
        wchar_t *x;
        
        DWORD len = 4096;
        RegEnumValue(key, i, name, &len, 0, 0, 0, 0);
        
        if (x = wcsstr(name, L" (TrueType)"))
            *x = 0;
        
        wchar_t *next = NULL;
        int font_index = 0;
        do {
            next = wcsstr(name, L" & ");
            if (next) {
                *next = 0;
                next += 3;
            }
            
            out = realloc(out, (nout + 2) * sizeof *out);
            out[nout++] = wcsdup(name);
            
            font_index++;
        } while (name = next);
    }
    if (countp)
        *countp = nout;
    out[nout] = NULL;
    RegCloseKey(key);
    return out;
}

AsgFont *platform_open_font_file(const wchar_t *filename, int font_index, bool scan_only) {
    Host *host = calloc(1, sizeof *host);
    void *data = load_file(host, filename);
    if (data) {
        AsgFont *font = asg_load_font(data, font_index, false);
        if (font) {
            font->host = host;
            font->host_free = host_free;
        }
        return font;
    } else {
        free(host);
        return NULL;
    }
}