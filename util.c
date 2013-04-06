/* vim: set noexpandtab:tabstop=8 */
#include <stdlib.h>
#include <wchar.h>
#include "wse.h"
#include "conf.h"

samerange(Loc *lo1, Loc *hi1, Loc *lo2, Loc *hi2) {
	return sameloc(lo1, lo2) && sameloc(hi1, hi2);
}

sameloc(Loc *x, Loc *y) {
	return x->ln==y->ln && x->ind==y->ind;
}

cmploc(Loc *x, Loc *y) {
	if (sameloc(x,y))
		return 0;
	if (x->ln < y->ln || (x->ln==y->ln && x->ind < y->ind))
		return -1;
	return 1;
}

ordersel(Loc *lo, Loc *hi) {
	int	sello = SLN<LN || (SLN==LN && SIND<IND);
	*lo = sello? SEL: CAR;
	*hi = sello? CAR: SEL;
	return SLN!=0;
}

nextcol(int vc, int c) {
	if (c!='\t')
		return 1;
	return file.tabc - (vc + file.tabc-1) % file.tabc;
}

col2ind(int ln, int col) {
	wchar_t	*txt;
	int	i,vc;
	
	txt=getb(b, ln, 0);
	vc=1;
	for (i=0; vc<col && txt[i]; i++)
		vc += nextcol(vc, txt[i]);
	return i;
}

ind2col(int ln, int ind) {
	wchar_t	*txt;
	int	i,vc;
	
	txt=getb(b, ln, 0);
	vc=1;
	for (i=0; i<ind; i++)
		vc += nextcol(vc, txt[i]);
	return vc;
}

encodeutf8to(unsigned char *out, wchar_t *in, wchar_t *end) {
	unsigned char *o=out;
	for ( ; in<end; in++)
		if (*in<0x80)
			*o++=*in;
		else if (*in<0x800) {
			*o++=((*in>>6)&0x1f)|0xc0;
			*o++=(*in&0x3f)|0x80;
		} else {
			*o++=((*in>>12)&0x0f)|0xe0;
			*o++=((*in>>6)&0x3f)|0x80;
			*o++=(*in&0x3f)|0x80;
		}
	*o=0;
	return o-out;
}

unsigned char*
encodeutf8(wchar_t *in, wchar_t *end) {
	unsigned char *buf=malloc((end-in)*3+1);
	int len=encodeutf8to(buf, in, end);
	return realloc(buf, len+1);
}

wchar_t*
decodeutf8(unsigned char *in, unsigned char *end) {
	wchar_t *out=malloc((strlen(in)+1)*sizeof(wchar_t));
	wchar_t *o=out;
	int len;
	
	/* Byte-order mark will be rejected by main routine. */
	if (end-in >= 3 && *in==0xef && in[1]==0xbb && in[2]==0xbf) {
		in+=3;
		*o++=0xfeff;
	}
	
	#define phase() in>=end || (*in&0xc0) != 0x80
	#define overlong(x) *o<x
	while (in<end)
		if (*in<0x80)
			*o++=*in++;
		else if (~*in & 0x20) {
			*o=(*in++ & 0x1f)<<6;
			if (phase()) goto bad;
			*o+=(*in++ & 0x3f);
			if (overlong(0x80)) goto bad;
			*o++;
		} else if (~*in & 0x10) {
			*o=(*in++ & 0xf)<<12;
			if (phase()) goto bad;
			*o+=(*in++ & 0x3f)<<6;
			if (phase()) goto bad;
			*o+=*in++ & 0x3f;
			if (overlong(0x800)) goto bad;
			*o++;
		} else {
			/* Discard non-BMP characters */
			in++;
			while ((*in++&0xc0)==0x80)
				in++;
			bad:
			*o++=0xfffd; /* replacment character */
		}
	*o=0;
	len=o-out;
	return realloc(out, (len+1)*sizeof(wchar_t));
}

