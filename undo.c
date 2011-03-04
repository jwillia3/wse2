#include <stdlib.h>
#include <wchar.h>
#include "wse.h"

static Undo*
push(Undo **stk, Undo *u) {
	u->next=*stk;
	return *stk=u;
}

static Undo*
pop(Undo **stk) {
	Undo	*u=*stk;
	if (!u)
		return 0;
	*stk=u->next;
	u->next=0;
	return u;
}

static Undo*
move(Undo **to, Undo **from, int n) {
	
	Undo	*array, *last;
	
	array=last= *from;
	while (--n)
		last=last->next;
	*from=last->next;
	last->next=*to;
	return *to=array;
}

static Line*
getlines(int lo, int hi) {

	Line	*dat, *first;
	wchar_t	*txt;
	int	i;
	
	dat=first= malloc(sizeof (Line) * (hi-lo+1));
	for (i=lo; i<=hi; i++, dat++) {
		txt = getb(b, i, &dat->len);
		dat->dat = wcsdup(txt);
		dat->max = dat->len;
	}
	return first;
}

static
putlines(Line *dat, int lo, int hi) {
	
	Line	*array;
	int	i;
	
	array=dat;
	for (i=lo; i<=hi; i++, dat++) {
		inslb(b, i, dat->dat, dat->len);
		free(dat->dat);
	}
	free(array);
	
}

static
droplines(int lo, int hi) {
	int	i;
	for (i=hi; i>=lo; i--)
		dellb(b, i);
}

static
modify(int undoing) {
	int	incr=undoing? -1: 1;

	b->changes+=incr;
	if (b->changes==0)
		alertchange(0);
	else if (b->changes-incr==0)
		alertchange(1);
}

_record(Undo **stk, int type, int lo, int hi) {
	
	Undo	*u;
	
	if (hi<lo) {
		int tmp=hi;
		hi=lo;
		lo=tmp;
	}
	
	u=push(stk, malloc(sizeof *u));
	u->type=type;
	u->dat=0;
	u->lo=lo;
	u->hi=hi;
	u->car=CAR;
	u->grp=0;
	
	
	switch (type) {
	
	case UndoSwap:
		u->dat=getlines(lo, hi);
		break;
	
	case UndoDelete:
		u->dat=getlines(lo, hi);
		break;
	
	case UndoInsert:
		break;
	
	case UndoGroup:
		pop(stk);
		move(&u->grp, stk, hi);
		push(stk, u);
		break;
	
	}
	return 1;
}

record(int type, int lo, int hi) {
	clearundo(&b->redo);
	modify(0);
	return _record(&b->undo, type, lo, hi);
}

undo(Undo **stk) {

	Undo	*u, **ostk;
	int	i, invmap[]={ UndoSwap, UndoInsert,
			UndoDelete, UndoGroup };
	
	/* Selections across histories don't make sense */
	SLN=0;
	
	u=pop(stk);
	if (!u)
		return 0;
	
	modify(stk==&b->undo);
	
	/* Add inverse to opposite stack */
	ostk = (stk==&b->undo)? &b->redo: &b->undo;
	if (u->type != UndoGroup)
		_record(ostk, invmap[u->type], u->lo, u->hi);
	
	switch (u->type) {
	
	case UndoSwap:
		droplines(u->lo, u->hi);
		putlines(u->dat, u->lo, u->hi);
		break;
	
	case UndoDelete:
		putlines(u->dat, u->lo, u->hi);
		break;
	
	case UndoInsert:
		droplines(u->lo, u->hi);
		break;
	
	case UndoGroup:
		move(stk, &u->grp, u->hi);
		for (i=0; i<u->hi; i++)
			undo(stk);
		_record(ostk, UndoGroup, 0, u->hi);
		break;
		
	}
	
	CAR=u->car;
	free(u);
	return 1;
}

static
freelines(Line *array, int lo, int hi) {
	Line	*c;
	int	i;
	
	if (! (c=array))
		return 0;
	
	for (i=lo; i<=hi; i++, c++)
		free(c->dat);
	free(array);
	return 1;
}

clearundo(Undo **stk) {
	
	Undo	*u;
	

	while (u=pop(stk)) {
		freelines(u->dat, u->lo, u->hi);
		clearundo(&u->grp);
	}
	return 1;
}

#include <stdio.h>
dbgundo(char *fn, Undo *u) {
	FILE	*f;
	Undo	*ou=u;
	int	n;
	
	f = fopen(fn, "w");
	
	for (n=0; u; n++, u=u->next);
	fprintf(f, "\n\n		%d ITEMS\n", n);
	u=ou;
	
	while(u) {
		_dbgundo(f, u);
		u=u->next;
	}
	
	fclose(f);
	return 0;
}
_dbgundo(FILE *f, Undo *u) {
	char	*s[] = {"SWP", "DEL", "INS", "GRP" };
	Line	*dat;
	int	n;
	
		
	fprintf(f, "%s %d,%d\n",
		s[u->type], u->lo, u->hi);
	
	n = u->hi - u->lo + 1;
	dat = u->dat;
	
	switch(u->type) {
	
	case UndoSwap:
	case UndoDelete:
		while (n--)
			fprintf(f, ">'%ls'\n",
				dat[n].dat);
		break;
		
	case UndoInsert:
		break;
	
	case UndoGroup:
		u=u->grp;
		n--;
		fprintf(f, "(%d){\n", n);
		fflush(f);
		while (n--) {
			_dbgundo(f, u);
			u=u->next;
		}
		fprintf(f, "}\n");
		break;
	default:
		printf("*bad %d\n", u->type);
		break;
	
	}
	fflush(f);
	return 0;
}

