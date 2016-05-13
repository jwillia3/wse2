/* vim: set noexpandtab:tabstop=8 */
/*
 *		Input/Output
 * This handles loading and storing files.
 * An important note is that decoders receive the entire file
 * as input, and is responsible for freeing it with free().
 * This is so 16-bit encodings can be done in-place.
 *
 */
#include <stdlib.h>
#include <wchar.h>
#include "wse.h"
#include "conf.h"
static
	sign_cp1252(unsigned char *buf),
	sign_utf8(unsigned char *buf),
	sign_utf16(unsigned char *buf);
static wchar_t
	*dec_cp1252(unsigned char *src, int sz),
	*dec_utf8(unsigned char *src, int sz),
	*dec_utf16(unsigned char *src, int sz);
static
	enc_cp1252(unsigned char *buf, wchar_t *src, int len),
	enc_utf8(unsigned char *buf, wchar_t *src, int len),
	enc_utf16(unsigned char *buf, wchar_t *src, int len);

static Codec	codecs[] = {
	{L"utf-8", sign_utf8, dec_utf8, enc_utf8 },
	{L"utf-16", sign_utf16, dec_utf16, enc_utf16 },
	{L"cp1252", sign_cp1252, dec_cp1252, enc_cp1252},
	{0, 0, 0, 0},
};
Codec		*codec=codecs;

/* Mapping CP-1252 0x80-0xA0 to Unicode */
static wchar_t cp1252[] = {
	0x20ac, /* euro sign */
	'?',
	0x201a, /* single low-9 quotation mark */
	0x0192, /* Latin small letter f with hook */
	0x201e, /* double low-9 quotation mark */
	0x2026, /* horizontal ellipsis */
	0x2020, /* dagger */
	0x2021, /* double dagger */
	0x02C6, /* modifier letter circumflex accent */
	0x2030, /* per mille sign */
	0x0160, /* lattin capital letter s with carron */
	0x2039, /* single left-pointing angle quotation mark */
	0x0152, /* latin capital ligature oe */
	'?',
	0x017d, /* latin capital letter z with carron */
	'?',
	'?',
	0x2018, /* left single quotation mark */
	0x2019, /* right single quotation mark */
	0x201c, /* left double quotation mark */
	0x201d, /* right double quotation mark */
	0x2022, /* bullet */
	0x2013, /* en dash */
	0x2014, /* em dash */
	0x20dc, /* small tilde */
	0x2122, /* trade mark sign */
	0x0161, /* latin small letter s with carron */
	0x203a, /* single right-pointing angle quotation mark */
	0x0153, /* latin small ligature oe */
	'?',
	0x017e, /* latin capital letter z with carron */
	0x0178 /* latin capital letter y with diaressis */
};

static
sign_cp1252(unsigned char *buf) {
	return 0;
}

static wchar_t*
dec_cp1252(unsigned char *src, int sz) {
	wchar_t	*dst,*odst;
	unsigned char *osrc=src;
	odst=dst = malloc((sz+1) * sizeof(wchar_t));
	while (sz--)
		*dst++ =
			(0x80 <= *src && *src < 0xa0)
			? cp1252[*src++-0x80]
			: *src++;
	*dst = 0;
	free(osrc);
	return odst;
}

static
enc_cp1252(unsigned char *buf, wchar_t *src, int len) {
	unsigned char	*dst=buf;
	wchar_t	*old=src;
	int	i;
	src = malloc((len+1) * sizeof(wchar_t));
//	NormalizeString(NormalizationC, old, len+1, src, len+1);
	old = src;
	for ( ; *src; src++)
		if (*src < 0x100) /* Pass latin-1 through */
			*dst++ = *src;
		else { /* Search for characters cp1252 has */
			for (i=0; i<32 && *src != cp1252[i]; i++);
			*dst++ = i<32? i+0x80: *src;
		}
	free(old);
	return dst-buf;
}

static wchar_t*
dec_utf8(unsigned char *src, int sz) {
	wchar_t *dst;
	if (!memcmp(src, "\xef\xbb\xbf", 3)) {
		src+=3;
		file.usebom = 1;
	}
	dst = decodeutf8(src, src+sz);
	free(src);
	return dst;
}

static
enc_utf8(unsigned char *buf, wchar_t *src, int len) {
	return encodeutf8to(buf, src, src+len);
}

static
sign_utf8(unsigned char *buf) {
	memcpy(buf, "\xef\xbb\xbf", 3);
	return 3;
}

static wchar_t*
dec_utf16(unsigned char *src, int sz) {
	return (wchar_t*)src;
}

static
sign_utf16(unsigned char *buf) {
	memcpy(buf, "\xff\xfe", 2);
	return 2;
}

static
enc_utf16(unsigned char *buf, wchar_t *src, int len) {
	memcpy(buf, src, len*2);
	return len*2;
}

static
detectenc(unsigned char *buf, int sz) {
	if (*buf==0xef && buf[1]==0xbb && buf[2]==0xbf) {
		memmove(buf,buf+3,sz);
		buf[sz-3]=0;
		setcodec(L"utf-8");
		file.usebom = 1;
	} else if (*buf==0xff && buf[1]==0xfe) {
		memmove(buf,buf+2,sz);
		((wchar_t*)buf)[sz/2 - 1] = 0;
		setcodec(L"utf-16");
		file.usebom = 1;
	} else if (memchr(buf,0,sz)) {
		setcodec(L"utf-16");
		file.usebom = 0;
	} else {
		setcodec(L"utf-8");
		file.usebom = 0;
	}
}

Codec*
setcodec(wchar_t *name) {
	Codec *c;
	for (c=codecs; c->name && wcscmp(c->name, name); c++);
	if (c->name)
		codec=c;
	return codec;
}

static
ex_settings(Buf *b, int line) {
	int	len;
	wchar_t *settab;
	wchar_t	*txt = getb(b,line,&len);
	if ((wcsstr(txt,L" ex:") && (txt+=4))
	  || (wcsstr(txt,L" vi:") && (txt+=4))
	  || (wcsstr(txt,L" vim:") && (txt+=5))) {
		if (settab = wcsstr(txt, L"tabstop="))
			file.tabc = wcstol(settab+8, 0, 0);
		if (settab = wcsstr(txt, L"ts="))
			file.tabc = wcstol(settab+3, 0, 0);
		if (wcsstr(txt, L"noexpandtab") || wcsstr(txt, L"noet"))
			file.usetabs = 1;
		else if (wcsstr(txt, L"expandtab") || wcsstr(txt, L"et"))
			file.usetabs = 0;
	}
}

int load(Buf *b, wchar_t *fn, wchar_t *encoding) {
	wchar_t	*dst, *odst, *eol, eolc;
	unsigned char	*src;
	int	sz,n,cr=0;
	void	*f;
	int	has_tab=0;
	
	defperfile();
	
	f=platform_openfile(fn,0,&sz);
	if (!f)
		return 0;
	
	src=malloc(sz+sizeof(wchar_t));
	platform_readfile(f,src,sz);
	src[sz]=0;
	src[sz+1]=0;
	
	if (!encoding || !setcodec(encoding))
		detectenc(src,sz);
	dst=odst = codec->dec(src,sz);
	
	if (wcschr(dst, L'\t'))
		file.usetabs = 1;
	
	platform_closefile(f);
	
	n=0;
	while (*dst) {
		n++;
		eol=dst + wcscspn(dst, L"\r\n");
		eolc=*eol;
		inslb(b, n, dst, eol-dst);
		if (*eol==L'\r')
			eol++, cr++;
		if (*eol==L'\n')
			eol++;
		dst=eol;
	}
	file.usecrlf = !!cr;
	free(odst);
	!eolc && dellb(b,NLINES); /* initial line wasn't in file */
	
	for (n=1; n<=5 && n<=NLINES; n++)
		ex_settings(b, n);
	for (n=NLINES-5; n>1 && n<=NLINES; n++)
		ex_settings(b, n);	
	return 1;
}

int save(Buf *b, wchar_t *fn) {
	wchar_t *linebreak = file.usecrlf? L"\r\n": L"\n";
	unsigned char	*buf;
	wchar_t	*src;
	int	i,len,sz,max;
	void	*f;
	
	f=platform_openfile(fn,1,0);
	
	if (file.usebom) {
		unsigned char signature[16];
		sz = codec->sign(signature);
		platform_writefile(f, signature, sz);
	}

	max=0;
	buf=malloc(sizeof(wchar_t));
	*buf=0;
	for (i=1; i<=NLINES; i++) {
		src=getb(b, i, &len);
		if (max<len) {
			max=len+2; /* +crlf */
			free(buf);
			buf=malloc(max*3+sizeof(wchar_t));
		}
		sz = codec->enc(buf,src,len);
		if (i != NLINES)
			sz += codec->enc(buf+sz,
				linebreak, wcslen(linebreak));
		platform_writefile(f,buf,sz);
	}
	
	platform_closefile(f);
	free(buf);
	return 1;
}
