/* vim: set noexpandtab:tabstop=8 */
/*
 *	Low-Level Buffer Manipulation
 *
 *	These do not update the screen nor do they provide
 *	any editor-like behaviour besides those that maintain
 *	consistency like moving to another line when the
 *	caret line has been deleted or inserted upon.
 */
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "wse.h"

sat(int lo, int x, int hi) {
	if (x<=lo)
		return lo;
	if (hi<=x)
		return hi;
	return x;
}

gob(Buf *b, int ln, int ind) {
	LN=sat(1, ln, NLINES);
	IND=sat(0, ind, lenb(b, ln));
	return ln==LN && IND==ind;
}

inslb(Buf *b, int ln, wchar_t *txt, int len) {
	int	i,sz;
	Bookmark	*bm;
	
	if (ln<1 || NLINES+1 < ln) /* allow overhang */
		return 0;
	ln--;

	if (NLINES==b->max) {
		b->max += 1;
		sz = (1+b->max) * sizeof (Line);
		b->dat = realloc(b->dat, sz);
	}
	
	for (bm = b->bookmarks; bm; bm = bm->next)
		if (bm->line > ln)
			bm->line++;
	
	for (i=NLINES; i>ln; i--)
		b->dat[i] = b->dat[i-1];
	
	b->dat[i].dat = malloc(sizeof (wchar_t) * (len+1));
	wcsncpy(b->dat[i].dat, txt, len);
	b->dat[i].dat[len] = 0;
	b->dat[i].len = len;
	b->dat[i].max = len;
	NLINES++;
	
	/* Make sure we have a valid caret */
	if (LN==ln)
		gob(b, ln, IND);
	
	return 1;
}

dellb(Buf *b, int ln) {
	int	i;
	Bookmark	*bm, *doomedbm;

	if (ln<1 || NLINES<ln)
		return 0;
	
	deletebookmark(b, ln);
	for (bm = b->bookmarks; bm; bm = bm->next)
		if (bm->line > ln)
			bm->line--;	
	ln--;
	
	free(b->dat[ln].dat);
	
	NLINES--;
	for (i=ln; i<NLINES; i++)
		b->dat[i] = b->dat[i+1];
	
	/* Don't let the buffer be empty */
	if (NLINES==0)
		inslb(b, 1, L"", 0);
	
	/* Make sure we have a valid caret */
	if (LN==ln)
		gob(b, ln, IND);
	return 1;
}

insb(Buf *b, int c) {
	Line	*l;
	int	i,sz;

	l = b->dat + (LN-1);
	if (l->len+1 >= l->max) {
		l->max += 32;
		sz = l->max * sizeof(wchar_t);
		l->dat = realloc(l->dat, sz);
	}
	
	for (i=l->len; i>IND; i--)
		l->dat[i] = l->dat[i-1];
	l->dat[++l->len]=0;
	l->dat[IND++] = c;
	return 1;
}

delatb(Buf *b, int index) {
	Line	*l;
	int	i;

	l = b->dat + (LN-1);
	if (index >= l->len)
		return 0;
	
	for (i=index; i < l->len; i++)
		l->dat[i] = l->dat[i+1];
	l->dat[--l->len];
	return 1;
}
delb(Buf *b) {
	return delatb(b, IND);
}

lenb(Buf *b, int ln) {
	return (ln<1 || NLINES<ln)? 0: b->dat[ln-1].len;
}

wchar_t*
getb(Buf *b, int ln, int *len) {
	if (len)
		*len = lenb(b, ln);
	return (ln<1 || NLINES<ln)? L"": b->dat[ln-1].dat;
}

Buf *newb() {
	Buf *buf= calloc(1, sizeof *buf);
	initb(buf);
	return buf;
}

initb(Buf *b) {
	b->dat=0;
	b->nlines=0;
	b->max=0;
	b->car.ln=1;
	b->car.ind=0;
	b->sel.ln=0;
	b->sel.ind=0;
	b->undo=0;
	b->redo=0;
	b->changes=0;
	LN=1;
	IND=0;
	inslb(b, 1, L"", 0);
	return 1;
}

void freeb(Buf *b) {
	clearb(b);
	free(b->dat);
}

clearb(Buf *b) {
	int	n;
	
	clearundo(b, &b->undo);
	clearundo(b, &b->redo);
	b->changes=0;
	
	n=NLINES;
	while (n)
		dellb(b, n--);
	
	while (b->bookmarks)
		deletebookmark(b, b->bookmarks->line);
	
	SLN=0;
	LN=1;
	IND=0;
	return 0;
}

addbookmark(Buf *b, int line) {
	Bookmark *bm, *next = b->bookmarks, *prev = 0;
	
	for (next = b->bookmarks; next; prev = next, next = next->next)
		if (next->line > line)
			break;
			
	bm = malloc(sizeof *b->bookmarks);
	bm->line = line;
	bm->prev = prev;
	bm->next = next;
	if (bm->next)
		bm->next->prev = bm;
	if (bm->prev)
		bm->prev->next = bm;
	else
		b->bookmarks = bm;
	return 1;
}
deletebookmark(Buf *b, int line) {
	Bookmark *bm;
	for (bm = b->bookmarks; bm; bm = bm->next)
		if (bm->line == line) {
			if (bm->prev)
				bm->prev->next = bm->next;
			else
				b->bookmarks = bm->next;
			if (bm->next)
				bm->next->prev = bm->prev;
			free(bm);
			return 1;
		}
	return 0;
}
isbookmarked(Buf *b, int line) {
	Bookmark *bm;
	for (bm = b->bookmarks; bm; bm = bm->next)
		if (bm->line == line)
			return 1;
	return 0;
}
Scanner
getscanner(Buf *b, int ln, int ind) {
	if (b == NULL)
		return (Scanner){.b=NULL, .ln=0, .ind=0, .c=0};
	Scanner scan;
	int len;
	wchar_t *txt;
	
	txt = getb(b, ln, &len);
	scan.ln = ln;
	scan.ind = ind;
	scan.c = ind < len? txt[ind]: 0;
	scan.b = b;
	return scan;
}
Scanner
startscanner(Buf *b) {
	return getscanner(b, 1, -1);
}

Scanner
endscanner(Buf *b) {
	return getscanner(b, b->nlines, lenb(b, b->nlines));
}

int
forward(Scanner *scan) {
	if (scan->b == NULL) return 0;
	int len;
	wchar_t *txt = getb(scan->b, scan->ln, &len);
	
	if (scan->ln < 1)
		scan->ln = 1, scan->ind = -1;
	if (scan->ind < -1)
		scan->ind = -1;
	if (scan->ind < len - 1)
		return scan->c = txt[++scan->ind];
	if (scan->ln < scan->b->nlines)
		return scan->ln++, scan->ind = -1, forward(scan);
	scan->ln = scan->b->nlines + 1;
	scan->ind = 0;
	return scan->c = 0;
}
int
backward(Scanner *scan) {
	if (scan->b == NULL) return 0;
	int len;
	wchar_t *txt = getb(scan->b, scan->ln, &len);
	
	if (scan->ln > scan->b->nlines)
		scan->ln = scan->b->nlines, scan->ind = lenb(scan->b, scan->b->nlines);
	if (scan->ln < 1)
		return scan->ln = 0, scan->ind = 0, scan->c = 0;
	if (scan->ind > len)
		scan->ind = lenb(scan->b, scan->ln);
	if (scan->ind < 0)
		return scan->ln--, scan->ind = lenb(scan->b, scan->ln), backward(scan);
	return scan->c = txt[--scan->ind];
}
Scanner
matchbrace(Scanner scan, bool allow_back, bool allow_forward) {
	wchar_t c;
	bool in_quote = quote_table[scan.c];
	Scanner s = scan;
	if ((c = closetbl[s.c]) && allow_forward)
		while (forward(&s))
			if (s.c == c) 				return s;
			else if (closetbl[s.c] && !in_quote)	s = matchbrace(s, false, true);
			else if (opentbl[s.c] && !in_quote)	break;
			else;
	s = scan;
	if ((c = opentbl[s.c]) && allow_back)
		while (backward(&s))
			if (s.c == c) 				return s;
			else if (opentbl[s.c] && !in_quote)	s = matchbrace(s, true, false);
			else if (closetbl[s.c] && !in_quote)	break;
			else;
	return getscanner(NULL, 0, 0);
}