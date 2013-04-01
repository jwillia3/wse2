/* vim: set noexpandtab:tabstop=8 */
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
