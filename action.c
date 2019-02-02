/* vim: set noexpandtab:tabstop=8 */
/*
 *		Actions
 * These routines are the middle level between the low-level
 * buffer operations in and the direct user interface
 * manipulation routines. For example, when the user strikes
 * a key, the UI level will change the title bar to mark the
 * file's modification status then call this routine to handle
 * interfacing with the buffer. These routines coordinate
 * multiple low-level operations. They call the low-level
 * routines to actually manipulate the buffer.
 */
#include <iso646.h>
#include <setjmp.h>
#include <stdlib.h>
#include <wchar.h>
#include "wse.h"
#include "action.h"
#include "conf.h"
#include "bre.h"

extern act(void); /* deliberately mis-declared */
static join(Buf *b, int lo, int hi, int space);

/* Insert considering tab-to-space */
static int instabb(Buf *b, int c) {
	if (c=='\t' && !file.usetabs) {
		int i = 0;
		do {
			insb(b,' ');
			i++;
		} while ((ind2col(b, LN, IND)-1) % file.tabc);
		return i;
	} else if (overwrite) {
		if (IND != lenb(b, LN))
			delb(b);
		return insb(b,c);
	} else
		return insb(b,c);
}

static int searchupline(Buf *b, struct match *m, int ln, int before, wchar_t *query) {
	wchar_t	*txt;
	int	i;
	
	txt=getb(b, ln, 0);
	for (i=before; i>=0 && !match(m, query, txt+i); i--);
	
	if (i<0)
		return 0;
	
	gob(b, ln, m->p[0]-txt);
	_act(b, EndSelection);
	_act(b, StartSelection);
	gob(b, ln, m->lim[0]-txt);
	return 1;
}

static int searchup(Buf *b, struct match *m, int ln, int ind, wchar_t *query) {
	int	i,x;
	Loc	lo,hi;
	
	x = ordersel(b, &lo,&hi)? lo.ind-1: ind-1;
	if (x>=0 && searchupline(b, m, ln, x, query))
		return 1;
	for (i=ln-1; i>0; i--)
		if (searchupline(b, m, i, lenb(b,i), query))
			return 1;
	for (i=NLINES; i>ln; i--)
		if (searchupline(b, m, i, lenb(b,i), query))
			return 1;
	return 0;
}

static int searchline(Buf *b, struct match *m, int ln, int after, wchar_t *query) {
	wchar_t	*txt;
	int	len,ok;
	
	txt=getb(b, ln, &len);
	if (!search(m, query, txt+(len<after? len: after)))
		return 0;
	
	gob(b, ln, m->p[0]-txt);
	_act(b, EndSelection);
	_act(b, StartSelection);
	gob(b, ln, m->lim[0]-txt);
	return 1;
}

static int find(Buf *b, struct match *m, int ln, int ind, wchar_t *query, int down) {
	struct match	_m;
	jmp_buf		trap;
	int		i;
	
	if (!m) {
		m=&_m;
		if (setjmp(*(m->trap=&trap))) {
			alertabort(m->err_msg, m->err_re);
			return 0;
		}
	}
	
	if (!down)
		return searchup(b, m, ln, ind, query);
	
	if (searchline(b, m, ln, ind, query))
		return 1;
	for (i=ln+1; i<=NLINES; i++)
		if (searchline(b, m, i, 0, query))
			return 1;
	for (i=1; i<ln; i++)
		if (searchline(b, m, i, 0, query))
			return 1;
	return 0;
}

_actquery(Buf *b, wchar_t *query, int down, int sens) {
	return find(b, NULL, LN, IND, query, down);
}

static int isearchsearchline(Buf *b, int ln, int ind, int end, wchar_t *query) {
	if (!query) return 0;
	wchar_t	*text = getb(b, ln, 0);
	wchar_t *at = wcsistr(text + ind, query);
	if (!at || at - text >= end)
		return 0;
	gob(b, ln, at - text);
	return 1;
}

int actisearch(Buf *b, wchar_t *query, int down, int skip) {
	int		i;
	
	if (!query)
		return 0;
	
	if (down) {
		if (isearchsearchline(b, LN, IND+skip, lenb(b, LN), query))
			return 1;
		for (i=LN+1; i<=NLINES; i++)
			if (isearchsearchline(b, i, 0, lenb(b, i), query))
				return 1;
		for (i=1; i<LN; i++)
			if (isearchsearchline(b, i, 0, lenb(b, i), query))
				return 1;
		return isearchsearchline(b, LN, 0, lenb(b, LN), query);
	} else {
		if (isearchsearchline(b, LN, 0, IND-skip, query))
			return 1;
		for (i=LN-1; i>=1; i--)
			if (isearchsearchline(b, i, 0, lenb(b, i), query))
				return 1;
		for (i=NLINES; i>LN; i--)
			if (isearchsearchline(b, i, 0, lenb(b, i), query))
				return 1;
		return isearchsearchline(b, LN, 0, lenb(b, i), query);
	}
}

int _actreplace(Buf *b, wchar_t *query, wchar_t *repl, int down, int sens) {
	wchar_t		*txt,*src;
	Loc		lo,unused;
	int		len;
	struct match	m;
	jmp_buf		trap;

	/* If there is a selection, search from the beginning
	 * of the selection instead of wherever the caret is
	 */	
	if (!ordersel(b, &lo, &unused))
		lo=CAR;
	lo.ind = lo.ind? lo.ind-1: 0;
	
	if (setjmp(*(m.trap=&trap))) {
		alertabort(m.err_msg, m.err_re);
		return 0;
	}
	
	if (!find(b, &m, lo.ln, lo.ind, query, down))
		return 0;
	
	/* Must build substitute before deleting */
	len=_subst(0, &m, repl);
	src=malloc((len+1) * sizeof(wchar_t));
	_subst(src, &m, repl);

	Undo *oldtop = b->undo;
	_act(b, DeleteSelection);
	record(b, UndoSwap, LN, LN);
	record(b, UndoGroup, 0, undosuntil(b, oldtop));
		
	for (txt=src; *txt; txt++)
		instabb(b, *txt);
	
	free(src);
	
	return 1;
}

int _actreplaceall(Buf *b, wchar_t *query, wchar_t *repl, int down, int sens) {
	Undo	*oldtop = b->undo;
	int	n;
	for (n=0; autoreplace(); n++);
	if (!n)
		return n;
	record(b, UndoGroup, 0, undosuntil(b, oldtop));
	return n;
}

int delete_formatting_space(Buf *b, const wchar_t *txt, int start) {
	int i;
	for (i = start; txt[i] && !(txt[i] == ' ' && txt[i + 1] == ' '); i++);
	if (txt[i]) {
		delatb(b, i);
		return 1;
	}
	return 0;
}
int insert_formatting_space(Buf *b, const wchar_t *txt, int start) {
	int i;
	for (i = start; txt[i] && !(txt[i] == ' ' && txt[i + 1] == ' '); i++);
	if (txt[i]) {
		insatb(b, i, ' ');
		return 1;
	}
	return 0;
}

int _actins(Buf *b, int c) {
	Loc	lo, hi;
	bool 	was_selecting = ordersel(b, &lo, &hi);
	wchar_t	*txt = getb(b, LN, 0);
	
	if (get_closing_brace(c) and was_selecting) {
		record(b, UndoSwap, LN, SLN);
		int same_line = lo.ln == hi.ln;
		gob(b, lo.ln, lo.ind);
		instabb(b, c);
		gob(b, hi.ln, hi.ind + same_line);
		instabb(b, get_closing_brace(c));
		gob(b, lo.ln, lo.ind);
		SEL = CAR;
		gob(b, hi.ln, hi.ind + 1 + same_line);
		return true;
	} else if (lang.autoClose and get_closing_brace(c) and not get_closing_brace(txt[IND])) {
		record(b, UndoSwap, LN, LN);
		instabb(b, c);
		instabb(b, get_closing_brace(c));
		gob(b, LN, IND - 1);
		return true;
	} else if (lang.typeover and not was_selecting and get_opening_brace(c) and txt[IND] == c)
		return _act(b, MoveRight);
	else {
		if (was_selecting)
			_act(b, DeleteSelection);
		if (c=='\n')
			return _act(b, BreakLine);
		
		record(b, UndoSwap, LN, LN);
		if (!overwrite)
			delete_formatting_space(b, getb(b, LN, 0), IND);
		return instabb(b, c);
	}
}

static int isbrk(int c) {
	return brktbl[c&0xffff];
}

static int getindent(Buf *b, int ln) {
	wchar_t	*txt;
	int	i;
	
	txt=getb(b, ln, 0);
	for (i=0; iswspace(txt[i]) && txt[i] != 12; i++);
	return ind2col(b, ln, i);
}

static int insprefix(Buf *b, int lo, int hi, wchar_t *txt) {
	Loc	old=CAR;
	int	i, advance = 0;
	wchar_t* p;

	record(b, UndoSwap, lo, hi);
	for (i=lo; i<=hi; i++) {
		gob(b, i, 0);
		advance = 0;
		for (p=txt; *p; p++)
			advance += instabb(b, *p);
	}
	
	if (SLN)
		SIND += advance;
	gob(b, old.ln, old.ind+advance);
	return hi-lo+1;
}

static int delprefix(Buf *b, int lo, int hi, wchar_t *pre) {
	Loc	old=CAR;
	wchar_t	*txt;
	int	i, j, n, plen;
	
	plen=wcslen(pre);
	record(b, UndoSwap, lo, hi);
	n=0;
	for (i=lo; i<=hi; i++) {
		gob(b, i, 0);
		txt=getb(b, i, 0);
		
		if (wcsncmp(txt, pre, plen))
			continue;
		for (j=0; j<plen; j++)
			delb(b);
		n++;
		
		if (i==old.ln)
			old.ind -= plen;
		else if (i==SLN)
			SIND -= plen;
	}
	gob(b, old.ln, old.ind);
	return n;
}

static int autoindent(Buf *b, int ln, int lvl) {
	int	x, nt, ns;
	
	x=getindent(b, ln);
	if (x>=lvl)
		return 0;
	
	gob(b, ln, 0);

	nt=(lvl-1)/file.tabc;
	ns=(lvl-1)%file.tabc;
	
	while (nt--)
		instabb(b, L'\t');
	while (ns--)
		instabb(b, L' ');
	return 1;
}

static int pastetext(Buf *b, wchar_t *txt) {
	Undo 	*oldtop = b->undo;
	wchar_t	*s;
	int	len, n, sel;
	int	lf;

	if (!txt || !*txt)
		return 0;
	
	if (sel=!!SLN)
		_act(b, DeleteSelection);
	
	lf = wcscspn(txt, L"\r\n");
	if (txt[lf]==L'\r')
		lf++;
	
	/* Single line */
	if (!txt[lf]) {
		record(b, UndoSwap, LN, LN);
		if (sel)
			record(b, UndoGroup, 0, undosuntil(b, oldtop));
		for (s=txt; s < txt+lf; s++)
			instabb(b, *s);
		return 1;
	}
	
	/* Count the number of NEW lines */
	for (n=0, s=txt+lf+1; *s++; n++) {
		s += wcscspn(s, L"\r\n");
		if (*s=='\r')
			s++;
	}
	record(b, UndoSwap, LN, LN);
	record(b, UndoInsert, LN+1, LN+n+1);
	
	/* Break first line */
	s=getb(b, LN, &len)+IND;
	inslb(b, LN+1, s, len-IND);
	while (delb(b));
	
	/* Append first line from board */
	for (s=txt; *s!=L'\r' && *s!=L'\n'; s++)
		insb(b, *s);
	
	/* Insert other lines one-by-one */
	n=1;
	s = txt+lf+1;
	do {
		len=wcscspn(s, L"\r\n");
		inslb(b, LN+n++, s, len);
		s += len + (s[len]==L'\r');
	} while (*s++);
	
	gob(b, LN + n-1, len);
	join(b, LN, LN+1, 0);
	record(b, UndoGroup, 0, undosuntil(b, oldtop));
	return 1;
}

wchar_t* copysel(Buf *b) {
	Loc	lo,hi;
	int	i,len,total;
	wchar_t	*s, *out;
	
	if (!ordersel(b, &lo, &hi))
		return wcscpy(malloc(sizeof *out),L"");
	
	/* Single line */
	if (hi.ln==lo.ln) {
		len = hi.ind - lo.ind;
		out=malloc((len+1) * sizeof (wchar_t));
		s=getb(b, lo.ln, 0);
		wcsncpy(out, s+lo.ind, len);
		out[len]=0;
		return out;
	}
	
	/* Sum the size of all the text */
	total=lenb(b, lo.ln) - lo.ind;
	for (i=lo.ln+1; i<hi.ln; i++)
		total += lenb(b, i)+2;
	total += 2 + hi.ind;
	
	/* Copy */
	out=malloc(sizeof (wchar_t) * (total+1));
	wcscpy(out, getb(b, lo.ln, &len)+lo.ind);
	s = out+(len - lo.ind);
	for (i=lo.ln+1; i<hi.ln; i++) {
		*s++=L'\r';
		*s++=L'\n';
		wcscpy(s, getb(b, i, &len));
		s += len;
	}
	*s++=L'\r';
	*s++=L'\n';
	wcsncpy(s, getb(b, hi.ln, 0), hi.ind);
	s[hi.ind]=0;
	return out;
}

static int delend(Buf *b, int ln, int ind) {
	if (ind==lenb(b, ln))
		return 0;
	gob(b, ln, ind);
	record(b, UndoSwap, ln, ln);
	while (delb(b));
	return 1;
}

static int delbefore(Buf *b, int ln, int ind) {
	if (ind==0)
		return 0;
	record(b, UndoSwap, ln, ln);
	gob(b, ln, 0);
	while (ind--)
		delb(b);
	return 1;
}

static int delexlines(Buf *b, int lo, int hi) {
	int i;
	if (lo+1 >= hi)
		return 0;
	record(b, UndoDelete, lo+1, hi-1);
	for (i=lo+1; i<hi; i++)
		dellb(b, lo+1);
	return 1;
}

static int delbetween(Buf *b, int ln, int lo, int hi) {
	int	len;
	if (lo==hi)
		return 0;
	len=hi-lo;
	gob(b, ln, lo);
	record(b, UndoSwap, LN, LN);
	while (len--)
		delb(b);
	return 1;
}

static int join(Buf *b, int lo, int hi, int space) {
	Undo 	*oldtop = b->undo;
	wchar_t	*txt;
	int	oldind, i;
	
	if (hi>NLINES || lo<1)
		return 0;
	
	for (i=0; i<hi-lo; i++) {
		Undo	*oldtop = b->undo;
		txt = getb(b, lo+1, 0);
		oldind = IND;
		
		gob(b, lo, lenb(b, lo));
		record(b, UndoSwap, lo, lo);
		
		if (space) {
			while (iswspace(*txt))
				txt++;
			instabb(b, L' ');
		}
		
		while (*txt)
			instabb(b, *txt++);
		
		gob(b, lo, oldind);
		record(b, UndoDelete, lo+1, lo+1);
		record(b, UndoGroup, 0, undosuntil(b, oldtop));
		dellb(b, lo+1);
	}
	
	if (hi-lo > 1) {
		SLN=lo;
		SIND=0;
		_act(b, MoveEnd);
		record(b, UndoGroup, 0, undosuntil(b, oldtop));
	}
	return 1;
}

static int skiptabspaces(Buf *b, wchar_t *txt, int ln, int ind, int dir) {
	int col=ind2col(b, ln, ind);
	int i;
	
	if (dir > 0) {
		int n=file.tabc - (col-1)%file.tabc;
		for (i=0; i<n && txt[ind++]==' '; i++);
		return i;
	} else {
		int n=(col-1)%file.tabc;
		if (n==0) n=file.tabc;
		for (i=0; i<n && ind>0 && txt[--ind]==' '; i++);
		return i;
	}
}

void align_delimiters(Buf *b, wchar_t *delims) {
	if (*delims == 0) return;
	wchar_t	tmp[128], *delim = tmp;
	wcscpy(tmp, delims);
	delim = wcstok(tmp, L" ");
	Loc lo, hi;
	ordersel(b, &lo, &hi);
	int col = 0;
	
	do {
		// Find one of the delimiters and find the greatest column it first occurs on
		for (int ln = lo.ln; ln <= hi.ln; ln++) {
			wchar_t *line = getb(b, ln, NULL);
			wchar_t *found = wcsstr(line, delim);
			if (found)
				col = max(col, ind2col(b, ln, found - line));
		}
	} while (col == 0 && (delim = wcstok(NULL, L" ")));
	
	if (col) {
		record(b, UndoSwap, lo.ln, hi.ln);
		for (int ln = lo.ln; ln <= hi.ln; ln++) {
			wchar_t *line = getb(b, ln, NULL);
			wchar_t *found = wcsstr(line, delim);
			if (found == NULL) continue;
			gob(b, ln, found - line);
			while (ind2col(b, ln, IND) < col) insb(b, L' ');
		}
		gob(b, lo.ln, lo.ind);
		_act(b, StartSelection);
		gob(b, hi.ln, hi.ind);
	}
}

int _act(Buf *b, int action) {
	Undo		*oldtop = b->undo;
	int		indent, n, len, sel, oldln, oldind;
	int		undos;
	Loc		lo, hi;
	wchar_t		*txt;
	Scanner		s;
	
	sel=SLN;

	switch (action) {
	
	case MoveUp:
		if (LN==1)
			return 0;
		IND=col2ind(b, LN-1, ind2col(b, LN, IND));
		return gob(b, LN-1, IND);
	
	case MovePageUp:
		if (LN-vis+1 == 1)
			return 0;
		IND=col2ind(b, LN-vis+1, ind2col(b, LN, IND));
		return gob(b, LN-vis+1, IND);
	
	case MoveDown:
		if (LN==NLINES)
			return 0;
		IND=col2ind(b, LN+1, ind2col(b, LN, IND));
		return gob(b, LN+1, IND);
	
	case MovePageDown:
		if (LN+vis-1 >= NLINES)
			return 0;
		IND=col2ind(b, LN+vis-1, ind2col(b, LN, IND));
		return gob(b, LN+vis-1, IND);
	
	case MoveLeft:
		txt=getb(b, LN, 0);
		if (n=skiptabspaces(b, txt,LN,IND,-1))
			return gob(b,LN,IND-n);
		if (gob(b, LN, IND-1))
			return 1;
		if (LN==1)
			return 0;
		_act(b, MoveUp);
		_act(b, MoveEnd);
		return 0;
	
	case MoveWordLeft:
		if (IND==0) {
			_act(b, MoveLeft);
			return 0;
		}
		
		txt=getb(b, LN, 0);
		
		/* Skip spaces to a word */
		while(IND && isbrk(txt[IND-1])==2)
			IND--;
		
		/* Punctuation */
		if (isbrk(txt[IND-1])==1)
			while (IND && isbrk(txt[IND-1])==1)
				IND--;
		else
			/* Skip to the beginning of word */
			while (IND && !isbrk(txt[IND-1]))
				IND--;
		
		return 1;
	
	case MoveRight:
		txt=getb(b, LN, 0);
		if (n=skiptabspaces(b, txt,LN,IND,+1))
			return gob(b,LN,IND+n);
			
		if (gob(b, LN, IND+1))
			return 1;
		if (LN==NLINES)
			return 0;
		_act(b, MoveDown);
		_act(b, MoveHome);
		return 0;
	
	case MoveWordRight:
		txt=getb(b, LN, &len);
		
		if (IND==len) {
			_act(b, MoveRight);
			return 0;
		}
		
		/* Skip spaces to a word */
		if (isbrk(txt[IND])==2)
			while (IND<len && isbrk(txt[IND])==2)
				IND++;
		
		/* Punctuation */
		if (isbrk(txt[IND])==1)
			return _act(b, MoveRight);
		else
			/* Skip to the end of current word */
			while (IND<len && !isbrk(txt[IND]))
				IND++;
		return 1;
	
	case MoveEnd:
		return gob(b, LN, lenb(b, LN));
	
	case MoveHome:
		indent=getindent(b, LN);
		if (IND && ind2col(b, LN, IND)==indent)
			return gob(b, LN, 0);
		return gob(b, LN, col2ind(b, LN, indent));
	
	case MoveSof:
		return gob(b, 1, 0);
	
	case MoveEof:
		return gob(b, NLINES, lenb(b, NLINES));
		
	case MoveBrace:
		s = matchbrace(getscanner(b, LN, IND), true, true);
		if (s.c == 0) return 0;
		LN = s.ln;
		IND = s.ind;
		return 1;
	case MoveOpen:
		s = backtoenclosingbrace(getscanner(b, LN, IND));
		if (s.c == 0) return 0;
		LN = s.ln;
		IND = s.ind;
		return 1;
	case MoveClose:
		Scanner s = backtoenclosingbrace(getscanner(b, LN, IND));
		s = matchbrace(s, false, true);
		if (s.c == 0) return 0;
		LN = s.ln;
		IND = s.ind;
		return 1;
	
	case ToggleOverwrite:
		return overwrite = !overwrite;
	case DeleteChar:
		if (sel)
			return _act(b, DeleteSelection);
		
		if (IND==lenb(b,LN)) {
			join(b, LN, LN+1, 0);
			return 0;
		}
		
		record(b, UndoSwap, LN, LN);
		txt=getb(b, LN, 0);
		if ((n=skiptabspaces(b, txt,LN,IND,+1)) == file.tabc)
			while (n-->0)
				delb(b);
		else {
			wchar_t deleted = txt[IND];
			delb(b);
			if (deleted != ' ')
				insert_formatting_space(b, txt, IND);
		}
		return 1;
	
	case BackspaceChar:
		if (sel)
			return _act(b, DeleteSelection);
		
		if (IND==0) {
			if (LN==1)
				return 0;
			_act(b, MoveUp);
			_act(b, MoveEnd);
			join(b, LN, LN+1, 0);
			return 0;
		}
		
		oldind=IND;
		record(b, UndoSwap, LN, LN);
		_act(b, MoveLeft);
		txt=getb(b, LN, 0);
		if (IND+(n=skiptabspaces(b, txt,LN,IND,+1)) <= oldind && n)
			while (n-->0)
				delb(b);
		else {
			wchar_t deleted = txt[IND];
			delb(b);
			if (deleted != ' ')
				insert_formatting_space(b, txt, IND);
		}
		return 1;
	
	case SpaceAbove:
		if (sel) {
			ordersel(b, &lo, &hi);
			_act(b, EndSelection);
			n = lo.ln;
		} else
			n = LN;
		record(b, UndoInsert, n, n);
		inslb(b, n, L"", 0);
		gob(b, n, 0);
		autoindent(b, n, getindent(b, n + 1));
		_act(b, MoveEnd);
		break;
	
	case SpaceBelow:
		if (sel) {
			ordersel(b, &lo, &hi);
			_act(b, EndSelection);
			gob(b, hi.ln, 0);
		}
		_act(b, MoveEnd);
		_act(b, BreakLine);
		break;
		
	case SpaceBoth:
		if (sel)
			_act(b, DeleteSelection);
		oldln=LN;
		oldind=IND;
		record(b, UndoInsert, oldln, oldln);
		record(b, UndoInsert, oldln+2, oldln+2);
		record(b, UndoGroup, 0, undosuntil(b, oldtop));
		
		inslb(b, oldln, L"", 0);
		gob(b, oldln, 0);
		autoindent(b, oldln, getindent(b, oldln+1));
		
		inslb(b, oldln+2, L"", 0);
		gob(b, oldln+2, 0);
		autoindent(b, oldln+2, getindent(b, oldln+1));
		
		gob(b, oldln+1, oldind);
		break;
	
	case AlignDelimiters:
		align_delimiters(b, global.alignables);
		break;
	
	case DeleteLine:
		if (sel)
			_act(b, DeleteSelection);
		record(b, UndoDelete, LN, LN);
		dellb(b, LN);
		gob(b, LN, IND);
		break;
	
	case BreakLine:
		if (sel)
			_act(b, DeleteSelection);
		
		record(b, UndoSwap, LN, LN);
		record(b, UndoInsert, LN+1, LN+1);
		record(b, UndoGroup, 0, undosuntil(b, oldtop));
		
		txt = getb(b, LN, &len) + IND;
		inslb(b, LN+1, txt, len-IND);
		while (delb(b));
		gob(b, LN+1, 0);
		
		if (!autoindent(b, LN, getindent(b, LN-1)))
			_act(b, MoveHome);
		return 1;
	
	case JoinLine:
		if (sel) {
			ordersel(b, &lo, &hi);
			return join(b, lo.ln, hi.ln, 1);
		}
		return join(b, LN, LN+1, 1);
	
	case Duplicate:
		if (!sel) {
			txt = getb(b, LN, &len);
			record(b, UndoInsert, LN+1, LN+1);
			inslb(b, LN+1, txt, len);
		} else if (SLN == LN) {
			int select_lo = SIND < IND;
			txt = getb(b, LN, NULL);
			record(b, UndoSwap, LN, LN);
			ordersel(b, &lo, &hi);
			gob(b, LN, hi.ind);
			for (int i = lo.ind; i < hi.ind; i++)
				insb(b, txt[i]);
			if (select_lo) SIND = lo.ind, IND = hi.ind;
			else IND = lo.ind, SIND = hi.ind;
		} else {
			ordersel(b, &lo, &hi);
			int count = hi.ln - lo.ln + 1;
			record(b, UndoInsert, lo.ln + count, lo.ln + count * 2);
			for (int i = 0; i < count; i++) {
				int len;
				wchar_t *txt = getb(b, lo.ln + i, &len);
				inslb(b, lo.ln + count + i, txt, len);
			}
		}
		return 1;
	
	case ClearLeft:
		if (!sel || SLN == LN) {
			int to_index = IND;
			_act(b, EndSelection);
			record(b, UndoSwap, LN, LN);
			gob(b, LN, 0);
			for (int i = 0; i < to_index; i++) delb(b);
		} else {
			int to_index = IND;
			ordersel(b, &lo, &hi);
			_act(b, EndSelection);
			record(b, UndoSwap, lo.ln, hi.ln);
			for (int i = lo.ln; i <= hi.ln; i++) {
				gob(b, i, 0);
				for (int i = 0; i < to_index; i++) delb(b);
			}
		}	
		return 1;
	case ClearRight:
		if (!sel || SLN == LN) {
			_act(b, EndSelection);
			record(b, UndoSwap, LN, LN);
			while (delb(b));
		} else {
			int from_index = IND;
			ordersel(b, &lo, &hi);
			_act(b, EndSelection);
			record(b, UndoSwap, lo.ln, hi.ln);
			for (int i = lo.ln; i <= hi.ln; i++) {
				gob(b, i, from_index);
				while (delb(b));
			}
		}	
		return 1;
	
	case AscendLine:
		if (sel)
			_act(b, EndSelection);
		if (LN==1)
			return 0;
		
		oldln=LN;
		oldind=IND;
		
		record(b, UndoInsert, oldln-1, oldln-1);
		txt = getb(b, oldln, &len);
		inslb(b, oldln-1, txt, len);
				
		record(b, UndoDelete, oldln+1, oldln+1);
		record(b, UndoGroup, 0, undosuntil(b, oldtop));
		gob(b, oldln-1, IND);
		dellb(b, oldln+1);
		gob(b, oldln-1, oldind);
		return 1;
	
	case DescendLine:
		if (sel)
			_act(b, EndSelection);
		if (LN==NLINES)
			return 0;
		oldln=LN;
		oldind=IND;
		
		record(b, UndoInsert, oldln, oldln);
		txt = getb(b, oldln+1, &len);
		inslb(b, oldln, txt, len);
		
		record(b, UndoDelete, oldln+2, oldln+2);
		record(b, UndoGroup, 0, undosuntil(b, oldtop));
		gob(b, oldln+1, oldind);
		return dellb(b, oldln+2);
	
	case SelectAll:
		SLN=0;
		_act(b, MoveSof);
		_act(b, StartSelection);
		_act(b, MoveEof);
		return 1;
		
	case SelectWord:
		SLN=0;
		if (IND > 0 && !isbrk(getb(b, LN, NULL)[IND - 1]))
			_act(b, MoveWordLeft);
		_act(b, StartSelection);
		_act(b, MoveWordRight);
		return 1;
		
	case StartSelection:
		if (SLN)
			return 0;
		SLN=LN;
		SIND=IND;
		return 1;
	
	case EndSelection:
		SLN=0;
		return 1;
	
	case IndentSelection:
		if (!ordersel(b, &lo, &hi))
			return 0;
		return insprefix(b, lo.ln, hi.ln, L"\t");
	
	case UnindentSelection:
		{
			int i;
			if (!ordersel(b, &lo, &hi))
				return 0;
			for (i=0; i<file.tabc; i++)
				if (!delprefix(b, lo.ln, hi.ln, L" "))
					break;
			if (i) {
				record(b, UndoGroup,0, undosuntil(b, oldtop));
				return 1;
			}
			return delprefix(b, lo.ln, hi.ln, L"\t");
		}
	
	case CommentSelection:
		if (!*lang.comment)
			return 0;
			
		if (!ordersel(b, &lo, &hi)) {
			_act(b, StartSelection);
			_act(b, CommentSelection);
			_act(b, EndSelection);
			return 0;
		}
		txt=getb(b, lo.ln, 0);
		len=wcslen(lang.comment);
		if (!wcsncmp(txt, lang.comment, len)) {
			delprefix(b, lo.ln, hi.ln, lang.comment);
			return 0;
		}
		return insprefix(b, lo.ln, hi.ln, lang.comment);
	
	case DeleteSelection:
		if (!ordersel(b, &lo, &hi))
			return 0;
		SLN=0;
		
		if (hi.ln==lo.ln)
			return delbetween(b, lo.ln, lo.ind, hi.ind);
		
		undos=0;
		if (delend(b, lo.ln, lo.ind))
			undos++;
		if (delexlines(b, lo.ln, hi.ln))
			undos++;
		if (delbefore(b, lo.ln+1, hi.ind))
			undos++;
		gob(b, lo.ln, lo.ind);
		if (join(b, LN, LN+1, 0))
			undos++;
		record(b, UndoGroup, 0, undosuntil(b, oldtop));
		return 1;
	
	case CutSelection:
		if (!SLN)
			return 0;
		_act(b, CopySelection);
		_act(b, DeleteSelection);
		return 1;
	
	case CopySelection:
		if (!SLN)
			return 0;
		latch=copysel(b);
		return 1;
	
	case PasteClipboard:
		return pastetext(b, latch);
	
	case UndoChange:
		return undo(b, &b->undo);
	
	case RedoChange:
		return undo(b, &b->redo);
	
	case PromptFind:
	case PromptReplace:
		return 0;
	
	case ReloadConfig:		
		return config();
	
	case PrevConfig:
		if (nconfs == 0) return 0;
		curconf = curconf? curconf-1: nconfs-1;
		selectconfig(curconf);
		return 1;
	
	case NextConfig:
		if (nconfs == 0) return 0;
		curconf = abs(curconf+1) % nconfs;
		selectconfig(curconf);
		return 1;
	
	case AddBookmark:
		addbookmark(b, LN);
		return 1;
	case DeleteBookmark:
		return deletebookmark(b, LN);
	case ToggleBookmark:
		if (isbookmarked(b, LN))
			deletebookmark(b, LN);
		else
			addbookmark(b, LN);
		return 1;
	case NextBookmark:
		{
			Bookmark *bm;
			for (bm = b->bookmarks; bm; bm = bm->next)
				if (bm->line > LN)
					return gob(b, bm->line, 0);
			if (b->bookmarks)
				return gob(b, b->bookmarks->line, 0);
			return 0;
		}
	case PrevBookmark:
		{
			Bookmark *bm;
			if (!b->bookmarks)
				return 0;
			for (bm = b->bookmarks; bm; bm = bm->next)
				if (bm->line >= LN) 
					if (bm->prev)
						return gob(b, bm->prev->line, 0);
					else
						break;
			for (bm = b->bookmarks; bm->next; bm = bm->next);
			return gob(b, bm->line, 0);
		}
	}

	return 0;
}


