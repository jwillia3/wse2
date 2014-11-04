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


static void *load_file(wchar_t *filename) {
    FILE *file = _wfopen(filename, L"rb");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    rewind(file);
    void *data = malloc(size);
    int read = fread(data, 1, size, file);
    fclose(file);
    return data;
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

void *platform_open_font(wchar_t *face) {
    const wchar_t *path = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";
    
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
            
            if (!wcsicmp(name, face))
                break;
            
            font_index++;
        } while (name = next);
        
        if (!name)
            continue;
            
        wchar_t filename[MAX_PATH];
        GetWindowsDirectory(filename, MAX_PATH);
        wcscat(filename, L"\\Fonts\\");
        len = 4096;
        DWORD filename_len = MAX_PATH - wcslen(filename) - 1;
        RegEnumValue(key, i, name, &len, 0, 0,
            (char*)(filename + wcslen(filename)), &filename_len);
        
        void *data = load_file(filename);
        if (data) {
            font = asg_load_font(data, font_index);
            break;
        }
    }
    RegCloseKey(key);
    return font;
}