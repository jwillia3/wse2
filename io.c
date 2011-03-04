#
/*
 *		Input/Output
 * This handles loading and storing files.
 * An important note is that decoders receive the entire file
 * as input, and is responsible for freeing it with LocalFree().
 * This is so 16-bit encodings can be done in-place.
 *
 */
#define WIN32_LEAN_AND_MEAN
#define STRICT
#define UNICODE
#include <Windows.h>
#include <stdlib.h>
#include "wse.h"

static wchar_t
	*dec_utf8(char *src, int sz),
	*dec_utf16(char *src, int sz);
static
	enc_utf8(char *buf, wchar_t *src, int len, int lf),
	enc_utf16(char *buf, wchar_t *src, int len, int lf);

static Codec	codecs[] = {
	{L"utf-8", dec_utf8, enc_utf8 },
	{L"utf-16", dec_utf16, enc_utf16 },
	{0, 0, 0},
};
Codec		*codec=codecs;

static wchar_t*
dec_utf8(char *src, int sz) {
	wchar_t	*dst;
	dst = LocalAlloc(0, (sz+1) * sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, src, sz+1, dst, sz+1);
	LocalFree(src);
	return dst;
}

static
enc_utf8(char *buf, wchar_t *src, int len, int lf) {
	int	sz;
	sz = WideCharToMultiByte(CP_UTF8, 0,
		src, len, buf, len*3, 0, 0);
	if (lf)
		buf[sz]='\n';
	return sz+!!lf;
}

static wchar_t*
dec_utf16(char *src, int sz) {
	return (wchar_t*)src;
}

static
enc_utf16(char *buf, wchar_t *src, int len, int lf) {
	memcpy(buf, src, len*sizeof(wchar_t));
	if (lf)
		((wchar_t*)buf)[len]=L'\n';
	return (len+!!lf)*sizeof(wchar_t);
}

static
detectenc(BYTE *buf, DWORD sz) {
	if (*buf==0xef && buf[1]==0xbb && buf[2]==0xbf) {
		memmove(buf,buf+3,sz);
		buf[sz-3]=0;
		setcodec(L"utf-8");
	} else if (*buf==0xff && buf[1]==0xfe) {
		memmove(buf,buf+2,sz);
		((wchar_t*)buf)[sz/2 - 1] = 0;
		setcodec(L"utf-16");
	} else if (memchr(buf,0,sz))
		setcodec(L"utf-16");
	else
		setcodec(L"utf-8");
}

Codec*
setcodec(wchar_t *name) {
	Codec *c;
	for (c=codecs; c->name && wcscmp(c->name, name); c++);
	if (c->name)
		codec=c;
	return codec;
}

load(wchar_t *fn, wchar_t *encoding) {
	wchar_t	*dst, *odst, *eol, eolc;
	char	*src;
	HANDLE	f;
	int	n;
	DWORD	sz;
	
	f = CreateFile(fn, GENERIC_READ,
		FILE_SHARE_READ|FILE_SHARE_WRITE, 0,
		OPEN_EXISTING, 0, 0);
	if (f==INVALID_HANDLE_VALUE)
		return 0;
	
	sz = GetFileSize(f, 0);
	src = LocalAlloc(0, sz+sizeof(wchar_t));
	ReadFile(f, src, sz, &sz, 0);
	src[sz] = 0;
	src[sz+1] = 0;
	
	if (!encoding || !setcodec(encoding))
		detectenc(src,sz);
	dst=odst = codec->dec(src,sz);
	
	CloseHandle(f);
	
	n=0;
	while (*dst) {
		n++;
		eol=dst;
		while (*eol && *eol!=L'\n' && *eol!=L'\r')
			eol++;
		eolc=*eol;
		inslb(b, n, dst, eol-dst);
		
		if (*eol==L'\r')
			eol++;
		if (*eol==L'\n')
			eol++;
		dst=eol;
	}
	LocalFree(odst);
	!eolc && dellb(b,NLINES); /* initial line wasn't in file */
	return 1;
}

save(wchar_t *fn) {
	HANDLE	*f;
	char	*buf;
	wchar_t	*src;
	int	i,len,sz,max;
	DWORD	ign;
	
	f = CreateFile(fn, GENERIC_WRITE, 0, 0,
		CREATE_ALWAYS, 0, 0);
	if (f==INVALID_HANDLE_VALUE)
		return 0;

	max=0;
	buf=malloc(sizeof(wchar_t));
	*buf=0;
	for (i=1; i<=NLINES; i++) {
		src=getb(b, i, &len);
		if (max<len) {
			max=len;
			free(buf);
			buf=malloc(max*3+sizeof(wchar_t));
		}
		sz = codec->enc(buf,src,len,i!=NLINES);
		WriteFile(f, buf, sz, &ign, 0);
	}
	
	CloseHandle(f);
	free(buf);
	return 1;
}



