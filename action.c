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
#include <setjmp.h>
#include <stdlib.h>
#include <wchar.h>
#include "wse.h"
#include "action.h"
#include "conf.h"
#include "bre.h"

extern act(void); /* deliberately mis-declared */
static join(int lo, int hi, int space);

/* Insert considering tab-to-space */
static
instabb(int c) {
	if (c=='\t' && !file.usetabs) {
		int i = 0;
		do {
			insb(b,' ');
			i++;
		} while ((ind2col(LN, IND)-1) % file.tabc);
		return i;
	} else
		return insb(b,c);
}

static
searchupline(struct match *m, int ln, int before, wchar_t *query) {
	wchar_t	*txt;
	int	i;
	
	txt=getb(b, ln, 0);
	for (i=before; i>=0 && !match(m, query, txt+i); i--);
	
	if (i<0)
		return 0;
	
	gob(b, ln, m->p[0]-txt);
	_act(EndSelection);
	_act(StartSelection);
	gob(b, ln, m->lim[0]-txt);
	return 1;
}

static
searchup(struct match *m, int ln, int ind, wchar_t *query) {
	int	i,x;
	Loc	lo,hi;
	
	x = ordersel(&lo,&hi)? lo.ind-1: ind-1;
	if (x>=0 && searchupline(m, ln, x, query))
		return 1;
	for (i=ln-1; i>0; i--)
		if (searchupline(m, i, lenb(b,i), query))
			return 1;
	for (i=NLINES; i>ln; i--)
		if (searchupline(m, i, lenb(b,i), query))
			return 1;
	return 0;
}

static
searchline(struct match *m, int ln, int after, wchar_t *query) {
	wchar_t	*txt;
	int	len,ok;
	
	txt=getb(b, ln, &len);
	if (!search(m, query, txt+(len<after? len: after)))
		return 0;
	
	gob(b, ln, m->p[0]-txt);
	_act(EndSelection);
	_act(StartSelection);
	gob(b, ln, m->lim[0]-txt);
	return 1;
}

static
find(struct match *m, int ln, int ind, wchar_t *query, int down) {
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
		return searchup(m, ln, ind, query);
	
	if (searchline(m, ln, ind, query))
		return 1;
	for (i=ln+1; i<=NLINES; i++)
		if (searchline(m, i, 0, query))
			return 1;
	for (i=1; i<ln; i++)
		if (searchline(m, i, 0, query))
			return 1;
	return 0;
}

_actquery(wchar_t *query, int down, int sens) {
	return find(0, LN, IND, query, down);
}

static
isearchsearchline(int ln, int ind, wchar_t *query) {
	wchar_t	*text = getb(b, ln, 0);
	wchar_t *at = wcsistr(text + ind, query);
	if (!at)
		return 0;
//	gob(b, ln, at - text + wcslen(query));
//	_act(EndSelection);
//	_act(StartSelection);
	gob(b, ln, at - text);
	return 1;
}

actisearch(wchar_t *query) {
	int		i;
	
	if (isearchsearchline(LN, IND, query))
		return 1;
	for (i=LN+1; i<=NLINES; i++)
		if (isearchsearchline(i, 0, query))
			return 1;
	for (i=1; i<LN; i++)
		if (isearchsearchline(i, 0, query))
			return 1;
	return isearchsearchline(LN, 0, query);
}

_actreplace(wchar_t *query, wchar_t *repl, int down, int sens) {
	wchar_t		*txt,*src;
	Loc		lo,unused;
	int		len;
	struct match	m;
	jmp_buf		trap;

	/* If there is a selection, search from the beginning
	 * of the selection instead of wherever the caret is
	 */	
	if (!ordersel(&lo, &unused))
		lo=CAR;
	lo.ind = lo.ind? lo.ind-1: 0;
	
	if (setjmp(*(m.trap=&trap))) {
		alertabort(m.err_msg, m.err_re);
		return 0;
	}
	
	if (!find(&m, lo.ln, lo.ind, query, down))
		return 0;
	
	/* Must build substitute before deleting */
	len=_subst(0, &m, repl);
	src=malloc((len+1) * sizeof(wchar_t));
	_subst(src, &m, repl);

	_act(DeleteSelection);
	record(UndoSwap, LN, LN);
	record(UndoGroup, 0, 2);
		
	for (txt=src; *txt; txt++)
		instabb(*txt);
	
	free(src);
	
	return 1;
}

_actreplaceall(wchar_t *query, wchar_t *repl, int down, int sens) {
	int	n;
	for (n=0; autoreplace(); n++);
	if (!n)
		return n;
	record(UndoGroup, 0, n);
	return n;
}

_actins(int c) {
	if (SLN)
		_act(DeleteSelection);
	if (c=='\n')
		return _act(BreakLine);
	
	record(UndoSwap, LN, LN);
	return instabb(c);
}

static
isbrk(int c) {
	return brktbl[c&0xffff];
}

static
getindent(int ln) {
	wchar_t	*txt;
	int	i;
	
	txt=getb(b, ln, 0);
	for (i=0; iswspace(txt[i]); i++);
	return ind2col(ln, i);
}

static
insprefix(int lo, int hi, wchar_t *txt) {
	Loc	old=CAR;
	int	i, advance = 0;
	wchar_t* p;

	record(UndoSwap, lo, hi);
	for (i=lo; i<=hi; i++) {
		gob(b, i, 0);
		advance = 0;
		for (p=txt; *p; p++)
			advance += instabb(*p);
	}
	
	if (SLN)
		SIND += advance;
	gob(b, old.ln, old.ind+advance);
	return hi-lo+1;
}

static
wrap(int lo, int hi, wchar_t *pre, wchar_t *suf) {
	Loc	old=CAR;
	int	i, advance = 0;
	wchar_t* p;

	record(UndoSwap, lo, hi);
	for (i=lo; i<=hi; i++) {
		gob(b, i, 0);
		advance = 0;
		for (p=pre; *p; p++)
			advance += instabb(*p);
		_act(MoveEnd);
		for (p=suf; *p; p++)
			instabb(*p);
	}
	
	if (SLN)
		SIND += advance;
	gob(b, old.ln, old.ind+advance);
	return hi-lo+1;
}

static
delprefix(int lo, int hi, wchar_t *pre) {
	Loc	old=CAR;
	wchar_t	*txt;
	int	i, j, n, plen;
	
	plen=wcslen(pre);
	record(UndoSwap, lo, hi);
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

static
autoindent(int ln, int lvl) {
	int	x, nt, ns;
	
	x=getindent(ln);
	if (x>=lvl)
		return 0;
	
	gob(b, ln, 0);

	nt=(lvl-1)/file.tabc;
	ns=(lvl-1)%file.tabc;
	
	while (nt--)
		instabb(L'\t');
	while (ns--)
		instabb(L' ');
	return 1;
}

static
pastetext(wchar_t *txt) {
	wchar_t	*s;
	int	len, n, sel;
	int	lf;

	if (!txt || !*txt)
		return 0;
	
	if (sel=!!SLN)
		_act(DeleteSelection);
	
	lf = wcscspn(txt, L"\r\n");
	if (txt[lf]==L'\r')
		lf++;
	
	/* Single line */
	if (!txt[lf]) {
		record(UndoSwap, LN, LN);
		if (sel)
			record(UndoGroup, 0, 2);
		for (s=txt; s < txt+lf; s++)
			instabb(*s);
		return 1;
	}
	
	/* Count the number of NEW lines */
	for (n=0, s=txt+lf+1; *s++; n++) {
		s += wcscspn(s, L"\r\n");
		if (*s=='\r')
			s++;
	}
	record(UndoSwap, LN, LN);
	record(UndoInsert, LN+1, LN+n+1);
	
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
	join(LN, LN+1, 0);
	record(UndoGroup, 0, 3 + sel);
	return 1;
}

wchar_t*
copysel() {
	Loc	lo,hi;
	int	i,len,total;
	wchar_t	*s, *out;
	
	if (!ordersel(&lo, &hi))
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

static
delend(int ln, int ind) {
	if (ind==lenb(b, ln))
		return 0;
	gob(b, ln, ind);
	record(UndoSwap, ln, ln);
	while (delb(b));
	return 1;
}

static
delbefore(int ln, int ind) {
	if (ind==0)
		return 0;
	record(UndoSwap, ln, ln);
	gob(b, ln, 0);
	while (ind--)
		delb(b);
	return 1;
}

static
delexlines(int lo, int hi) {
	int i;
	if (lo+1 >= hi)
		return 0;
	record(UndoDelete, lo+1, hi-1);
	for (i=lo+1; i<hi; i++)
		dellb(b, lo+1);
	return 1;
}

static
delbetween(int ln, int lo, int hi) {
	int	len;
	if (lo==hi)
		return 0;
	len=hi-lo;
	gob(b, ln, lo);
	record(UndoSwap, LN, LN);
	while (len--)
		delb(b);
	return 1;
}

static
join(int lo, int hi, int space) {
	wchar_t	*txt;
	int	oldind, i;
	
	if (hi>NLINES || lo<1)
		return 0;
	
	for (i=0; i<hi-lo; i++) {
		txt = getb(b, lo+1, 0);
		oldind = IND;
		
		gob(b, lo, lenb(b, lo));
		record(UndoSwap, lo, lo);
		
		if (space) {
			while (iswspace(*txt))
				txt++;
			instabb(L' ');
		}
		
		while (*txt)
			instabb(*txt++);
		
		gob(b, lo, oldind);
		record(UndoDelete, lo+1, lo+1);
		record(UndoGroup, 0, 2);
		dellb(b, lo+1);
	}
	
	if (hi-lo > 1) {
		SLN=lo;
		SIND=0;
		_act(MoveEnd);
		record(UndoGroup, 0, hi-lo);
	}
	return 1;
}

static
matchbrace() {
	int	x,c,o,i,nest,len;
	wchar_t	*txt;
	
	txt=getb(b, LN, &len) + IND;
	nest=0;
	i=LN;
	
	/*
	 * Find closing brace (ON opening)
	 */
	if (c=closetbl[o=*txt]) {
		for (;;) {
			for ( ; *txt; txt++)
				if (*txt==o)
					nest++;
				else if (*txt==c && nest>1)
					nest--;
				else if (*txt==c) {
					txt++;
					goto got;
				}
			if (++i>NLINES)
				return 0;
			txt=getb(b, i, 0);
		}
	}
	/*
	 * Find opening brace (ON closing)
	 */
	else if (o=opentbl[c=txt[IND? -1: 0]]) {
		if (IND) {
			txt--;
			IND--;
		}
		len=IND;
		for (;;) {
			for ( ; len>=0; len--, txt--)
				if (*txt==c)
					nest++;
				else if (*txt==o && nest>1)
					nest--;
				else if (*txt==o)
					goto got;
			if (--i<1)
				return 0;
			txt=getb(b, i, &len);
			txt+=len;
		}
	} else
		return 0;
got:
	return gob(b, i, txt-getb(b, i, 0));
}

static
clear_load(wchar_t *fn, wchar_t *encoding) {
	b->changes=0;
	clearb(b);
	return load(fn, encoding);
}

static
reload(wchar_t *encoding) {
	int	oldln, oldind;
	oldln=LN;
	oldind=IND;
	if (clear_load(filename,encoding)) {
		gob(b, oldln, oldind);
		return 1;
	}
	return 0;
}

static
skiptabspaces(wchar_t *txt, int ln, int ind, int dir) {
	int col=ind2col(ln, ind);
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

_act(int action) {
	int		indent, n, len, sel, oldln, oldind;
	int		undos;
	Loc		lo, hi;
	wchar_t		*txt;
	
	sel=SLN;

	switch (action) {
	
	case NewFile:
		free(filename);
		filename=wcsdup(L"//Untitled");
		clearb(b);
		setcodec(L"utf-8");
		defperfile();
		return 1;
	
	case LoadFile:
		return clear_load(filename,0);
	
	case ToggleLinebreak:
		file.usecrlf ^= 1;
		return 1;
	
	case ToggleTabs:
		file.usetabs = !file.usetabs;
		return 1;
	
	case Toggle8Tab:
		file.tabc = (file.tabc == 8)? 4: 8;
		return 1;
	
	case ToggleBOM:
		file.usebom ^= 1;
		return 1;
	
	case ReloadFileUTF8:
		return reload(L"utf-8");
	case ReloadFileUTF16:
		return reload(L"utf-16");
	case ReloadFileCP1252:
		return reload(L"cp1252");
	case ReloadFile:
		return reload(0);
			
	case SetUTF8:
		return !!setcodec(L"utf-8");
	case SetUTF16:
		return !!setcodec(L"utf-16");
	case SetCP1252:
		return !!setcodec(L"cp1252");
	case SaveFile:
		return save(filename);
	
	case MoveUp:
		if (LN==1)
			return 0;
		IND=col2ind(LN-1, ind2col(LN, IND));
		return gob(b, LN-1, IND);
	
	case MovePageUp:
		if (LN-vis+1 == 1)
			return 0;
		IND=col2ind(LN-vis+1, ind2col(LN, IND));
		return gob(b, LN-vis+1, IND);
	
	case MoveDown:
		if (LN==NLINES)
			return 0;
		IND=col2ind(LN+1, ind2col(LN, IND));
		return gob(b, LN+1, IND);
	
	case MovePageDown:
		if (LN+vis-1 >= NLINES)
			return 0;
		IND=col2ind(LN+vis-1, ind2col(LN, IND));
		return gob(b, LN+vis-1, IND);
	
	case MoveLeft:
		txt=getb(b, LN, 0);
		if (n=skiptabspaces(txt,LN,IND,-1))
			return gob(b,LN,IND-n);
		if (gob(b, LN, IND-1))
			return 1;
		if (LN==1)
			return 0;
		_act(MoveUp);
		_act(MoveEnd);
		return 0;
	
	case MoveWordLeft:
		if (IND==0) {
			_act(MoveLeft);
			return 0;
		}
		
		txt=getb(b, LN, 0);
		
		/* Skip spaces to a word */
		while(IND && isbrk(txt[IND-1])==2)
			IND--;
		
		/* Punctuation */
		if (isbrk(txt[IND-1])==1)
			IND--;
		else
			/* Skip to the beginning of word */
			while (IND && !isbrk(txt[IND-1]))
				IND--;
		
		return 1;
	
	case MoveRight:
		txt=getb(b, LN, 0);
		if (n=skiptabspaces(txt,LN,IND,+1))
			return gob(b,LN,IND+n);
			
		if (gob(b, LN, IND+1))
			return 1;
		if (LN==NLINES)
			return 0;
		_act(MoveDown);
		_act(MoveHome);
		return 0;
	
	case MoveWordRight:
		txt=getb(b, LN, &len);
		
		if (IND==len) {
			_act(MoveRight);
			return 0;
		}
		
		/* Skip spaces to a word */
		if (isbrk(txt[IND])==2) {
			while (IND<len && isbrk(txt[IND])==2)
				IND++;
			return 1;
		}
		
		/* Don't skip punctuation */
		if (isbrk(txt[IND])==1) {
			IND++;
			return 1;
		}
		
		/* Skip to the end of current word */
		while (IND<len && !isbrk(txt[IND]))
			IND++;
		/* Skip spaces to the next */
		while (IND<len && isbrk(txt[IND])==2)
			IND++;
		return 1;
	
	case MoveEnd:
		return gob(b, LN, lenb(b, LN));
	
	case MoveHome:
		indent=getindent(LN);
		if (IND && ind2col(LN, IND)==indent)
			return gob(b, LN, 0);
		return gob(b, LN, col2ind(LN, indent));
	
	case MoveSof:
		return gob(b, 1, 0);
	
	case MoveEof:
		return gob(b, NLINES, lenb(b, NLINES));
		
	case MoveBrace:
		return matchbrace();
	
	case DeleteChar:
		if (sel)
			return _act(DeleteSelection);
		
		if (IND==lenb(b,LN)) {
			join(LN, LN+1, 0);
			return 0;
		}
		
		record(UndoSwap, LN, LN);
		txt=getb(b, LN, 0);
		if ((n=skiptabspaces(txt,LN,IND,+1)) == file.tabc)
			while (n-->0)
				delb(b);
		else
			delb(b);
		return 1;
	
	case BackspaceChar:
		if (sel)
			return _act(DeleteSelection);
		
		if (IND==0) {
			if (LN==1)
				return 0;
			_act(MoveUp);
			_act(MoveEnd);
			join(LN, LN+1, 0);
			return 0;
		}
		
		oldind=IND;
		record(UndoSwap, LN, LN);
		_act(MoveLeft);
		txt=getb(b, LN, 0);
		if (IND+(n=skiptabspaces(txt,LN,IND,+1)) <= oldind && n)
			while (n-->0)
				delb(b);
		else
			delb(b);
		return 1;
	
	case SpaceAbove:
		if (sel)
			_act(DeleteSelection);
		oldln=LN;
		oldind=IND;
		record(UndoInsert, oldln, oldln);
		inslb(b, oldln, L"", 0);
		gob(b,oldln,0);
		autoindent(oldln, getindent(oldln+1));
		_act(MoveEnd);
		break;
	
	case SpaceBelow:
		if (sel)
			_act(DeleteSelection);
		_act(MoveEnd);
		_act(BreakLine);
		break;
		
	case SpaceBoth:
		if (sel)
			_act(DeleteSelection);
		oldln=LN;
		oldind=IND;
		record(UndoInsert, oldln, oldln);
		record(UndoInsert, oldln+2, oldln+2);
		record(UndoGroup, 0, 2);
		
		inslb(b, oldln, L"", 0);
		gob(b, oldln, 0);
		autoindent(oldln, getindent(oldln+1));
		
		inslb(b, oldln+2, L"", 0);
		gob(b, oldln+2, 0);
		autoindent(oldln+2, getindent(oldln+1));
		
		gob(b, oldln+1, oldind);
		break;
	
	case DeleteLine:
		if (sel)
			_act(DeleteSelection);
		record(UndoDelete, LN, LN);
		return dellb(b, LN);
	
	case BreakLine:
		if (sel)
			_act(DeleteSelection);
		
		record(UndoSwap, LN, LN);
		record(UndoInsert, LN+1, LN+1);
		record(UndoGroup, 0, 2);
		
		txt = getb(b, LN, &len) + IND;
		inslb(b, LN+1, txt, len-IND);
		while (delb(b));
		gob(b, LN+1, 0);
		
		if (!autoindent(LN, getindent(LN-1)))
			_act(MoveHome);
		return 1;
	
	case JoinLine:
		if (sel) {
			ordersel(&lo, &hi);
			return join(lo.ln, hi.ln, 1);
		}
		return join(LN, LN+1, 1);
	
	case DupLine:
		txt = getb(b, LN, &len);
		record(UndoInsert, LN+1, LN+1);
		inslb(b, LN+1, txt, len);
		return 1;
	
	case AscendLine:
		if (sel)
			_act(EndSelection);
		if (LN==1)
			return 0;
		
		oldln=LN;
		oldind=IND;
		
		record(UndoInsert, oldln-1, oldln-1);
		txt = getb(b, oldln, &len);
		inslb(b, oldln-1, txt, len);
				
		record(UndoDelete, oldln+1, oldln+1);
		record(UndoGroup, 0, 2);
		gob(b, oldln-1, IND);
		dellb(b, oldln+1);
		gob(b, oldln-1, oldind);
		return 1;
	
	case DescendLine:
		if (sel)
			_act(EndSelection);
		if (LN==NLINES)
			return 0;
		oldln=LN;
		oldind=IND;
		
		record(UndoInsert, oldln, oldln);
		txt = getb(b, oldln+1, &len);
		inslb(b, oldln, txt, len);
		
		record(UndoDelete, oldln+2, oldln+2);
		record(UndoGroup, 0, 2);
		gob(b, oldln+1, oldind);
		return dellb(b, oldln+2);
	
	case WrapLine:
		if (ordersel(&lo, &hi))
			return wrap(lo.ln, hi.ln, wrapbefore, wrapafter);
		return wrap(LN, LN, wrapbefore, wrapafter);
	
	case SelectAll:
		SLN=0;
		_act(MoveSof);
		_act(StartSelection);
		_act(MoveEof);
		return 1;
		
	case SelectWord:
		SLN=0;
		_act(MoveWordLeft);
		_act(StartSelection);
		_act(MoveWordRight);
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
		if (!ordersel(&lo, &hi))
			return 0;
		return insprefix(lo.ln, hi.ln, L"\t");
	
	case UnindentSelection:
		{
			int i;
			if (!ordersel(&lo, &hi))
				return 0;
			for (i=0; i<file.tabc; i++)
				if (!delprefix(lo.ln, hi.ln, L" "))
					break;
			if (i) {
				record(UndoGroup,0,i);
				return 1;
			}
			return delprefix(lo.ln, hi.ln, L"\t");
		}
	
	case CommentSelection:
		if (!*lang.comment)
			return 0;
			
		if (!ordersel(&lo, &hi)) {
			_act(StartSelection);
			_act(CommentSelection);
			_act(EndSelection);
			return 0;
		}
		txt=getb(b, lo.ln, 0);
		len=wcslen(lang.comment);
		if (!wcsncmp(txt, lang.comment, len)) {
			delprefix(lo.ln, hi.ln, lang.comment);
			return 0;
		}
		return insprefix(lo.ln, hi.ln, lang.comment);
	
	case DeleteSelection:
		if (!ordersel(&lo, &hi))
			return 0;
		SLN=0;
		
		if (hi.ln==lo.ln)
			return delbetween(lo.ln, lo.ind, hi.ind);
		
		undos=0;
		if (delend(lo.ln, lo.ind))
			undos++;
		if (delexlines(lo.ln, hi.ln))
			undos++;
		if (delbefore(lo.ln+1, hi.ind))
			undos++;
		gob(b, lo.ln, lo.ind);
		if (join(LN, LN+1, 0))
			undos++;
		record(UndoGroup, 0, undos);
		return 1;
	
	case CutSelection:
		if (!SLN)
			return 0;
		_act(CopySelection);
		_act(DeleteSelection);
		return 1;
	
	case CopySelection:
		if (!SLN)
			return 0;
		latch=copysel();
		return 1;
	
	case PasteClipboard:
		return pastetext(latch);
	
	case UndoChange:
		return undo(&b->undo);
	
	case RedoChange:
		return undo(&b->redo);
	
	case PromptOpen:
	case PromptFind:
	case PromptGo:
	case PromptReplace:
		return 0;
	
	case ReloadConfig:		
		return config();
	
	case PrevConfig:
		curconf = curconf? curconf-1: nconfs-1;
		selectconfig(curconf);
		return 1;
	
	case NextConfig:
		curconf = abs(curconf+1) % nconfs;
		selectconfig(curconf);
		return 1;
	}

	return 0;
}


