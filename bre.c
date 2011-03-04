#
/*
		POSIX BASIC REGULAR EXPRESSION
		    JERRY LEE WILLIAMS JR
			25 MAY 2009

 */

#include <wchar.h>
#include <setjmp.h>
#include "bre.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/*
 * GENERAL
 * (1) Each sub-expression form must always scan itself
 *	completely and return the rest of the expression
 *	so that the failure-absorbing operators do not have
 *	to have the logic to skip sub-expressions.
 * (2) `match.ok` is only set on failure
 */

typedef struct match M;
typedef wchar_t *str;
#define OK	m->ok
#define TXT	(m->lim[0])
#define ADV()	(TXT++)

static wchar_t	*esc=L"..[[**++^^$$??t\tn\nr\r\\\\f\fv\v";

static str	seq(M *m, str re);

typedef struct state {
	int	ok;
	int	n;
	wchar_t	*txt;
} State;

static
err(M *m, wchar_t *msg, wchar_t *re) {
	m->err_re=re;
	m->err_msg=msg;
	longjmp(*m->trap, 1);
}

static
save(State *s, M *m) {
	s->ok=m->ok;
	s->n=m->n;
	s->txt=TXT;
}

static
restore(M *m, State *s) {
	m->ok=s->ok;
	m->n=s->n;
	TXT=s->txt;
}

static
isword(int c) {
	return iswalnum(c) || c=='_';
}

static
isbound(M *m, wchar_t *txt) {
	return m->sot==txt || !isword(txt[-1]) || !isword(*txt);
}

static str
atom(M *m, str re) {
	int	ok,not,n,sz;
	wchar_t	*p;
	
	switch (*re) {
	
	case '.':
		re++;
		if (OK && *TXT)
			ADV();
		else
			OK=0;
		break;
	
	case '$':
		re++;
		if (*TXT)
			OK=0;
		break;
	
	case '^':
		re++;
		if (TXT==m->sot)
			OK=0;
		break;
	
	case '[':
		ok=0;
		if (not=(re[1]=='^'))
			re++;
		if (re[1]==']')
			ok=*TXT==*++re;
		while (*++re != ']')
			if (!*re)
				err(m, L"unclosed bracket", re);
			else if (re[1]=='-' && re[2]!=']' && re[2]) {
				re+=2;
				if (re[-2]<=*TXT && *TXT<=*re)
					ok=1;
			} else if (*re=='\\') {
				p=wcschr(esc, *++re);
				if (!p)
					err(m, L"bad escape", re-2);
				if (p[1]==*TXT)
					ok=1;
			} else if (*re==*TXT)
				ok=1;
		if (ok!=not && *TXT)
			ADV();
		else
			OK=0;
		re++;
		break;
	
	case '\\':
		re++;
		switch (*re++) {
		
		case '(':
			n=++m->n;
			m->p[n]=TXT;
			re=seq(m, re);
			m->lim[n]=TXT;
			if (wcsncmp(re, L"\\)", 2))
				err(m, L"unclosed paren", re-2);
			re+=2;
			break;
			
		case ')':
			/* seq() should have intercepted */
			err(m, L"misplaced paren", re-2);
			break;
		
		case '<':
			if (m->sot < TXT && isword(TXT[-1]))
				OK=0;
			break;
			
		case '>':
			if (isword(*TXT))
				OK=0;
			break;
			
		case 'a':
		case 'A':
			if (re[-1]=='a' && iswlower(*TXT))
				ADV();
			else if (re[-1]=='A' && iswupper(*TXT))
				ADV();
			else
				OK=0;
			break;
			break;
		
		case 'b':
		case 'B':
			if (isbound(m, TXT)!=(re[-1]=='b'))
				OK=0;
			break;
		
		case 'w':
		case 'W':
			if (isword(*TXT)==(re[-1]=='w'))
				ADV();
			else
				OK=0;
			break;
		
		case 'd':
		case 'D':
			ok=re[-1]=='d';
			if (*TXT && ok==iswdigit(*TXT))
				ADV();
			else
				OK=0;
			break;
		
		case 's':
		case 'S':
			ok=re[-1]=='s';
			if (*TXT && ok==iswspace(*TXT))
				ADV();
			else
				OK=0;
			break;
		
		case '{':
		case '}':
			/* Handled later */
			break;
			
		default:
		
			if (isdigit(re[-1])) {
			
				n=re[-1]-'0';
				if (n > m->n)
					err(m, L"unseen reference", re-2);
					
				sz=m->lim[n] - m->p[n];
				p = malloc((sz+1)*sizeof(wchar_t));
				if (!p)
					err(m, L"memory error", re-2);
				wcsncpy(p, m->p[n], sz);
				p[sz]=0;
				seq(m, p);
				
				free(p);
				break;
			}
		
			p=wcschr(esc, re[-1]);
			if (!p)
				err(m, L"bad escape", re-2);
			if (p[1]==*TXT)
				ADV();
			else
				OK=0;
			break;
		}
		break;
	
	default:
		if (OK && *re==*TXT)
			ADV();
		else
			OK=0;
		re++;
	}
	return re;
}

/*
 *	Altruistic Kleene Star
 *	Return 0 if sub-expr or expr after * fails.
 *	A return of 0 indicates the receiver should back up
 *	and try to match the tail expr. If it cannot, it backs
 *	up and returns 0. Returning 1 means that the maximum
 *	match has been found already.
 */
static
star(M *m, str re, str rest, int rem) {
	State	s;
	M	m1;
	
	if (!rem--)
		return 0;

	save(&s, m);
	atom(m, re);
	if (!OK) {
		restore(m, &s);
		return 0;
	}
	
	if (star(m, re, rest, rem))
		return 1;
		
	OK=1;
	m1=*m;
	seq(&m1, rest);
	if (m1.ok)
		return 1;
	
	restore(m, &s);
        return 0;
}

static
op2(wchar_t *re, wchar_t *op) {
	return *re==*op && re[1]==op[1];
}

static str
suffix(M *m, str re) {
	wchar_t	*oldre;
	State	s;
	int	n,i;
	
	save(&s, m);
	oldre=re;
	re=atom(m, oldre);
	
	for (;;)
		if (*re=='?') {
			re++;
			if (OK)
				continue;
			restore(m, &s);
		} else if (*re=='*') {
			re++;
			if (OK) {
				/* Dump first match to avoid
				 * re-implementing backtracking
				 */
				restore(m, &s);
				star(m, oldre, re, INT_MAX);
			} else
				restore(m, &s);
		} else if (*re=='+') {
			re++;
			if (OK)
				star(m, oldre, re, INT_MAX);
		} else if (op2(re, L"\\{")) {
			n=wcstoul(re+2, &re, 10);
			
			if (n)
				for (i=n-1; i>0 && OK; i--)
					atom(m, oldre);
			else
				restore(m, &s);
			
			if (*re==',' && op2(re+1, L"\\}")) {
				re++;
				star(m, oldre, re+2, INT_MAX);
			} else if (*re==',') {
				n = wcstoul(re+1, &re, 10) - n;
				if (n>0 && OK)
					star(m, oldre, re+2, n);
			}
			
			if (!op2(re, L"\\}"))
				err(m, L"unclosed bracket", re);
			re+=2;
		} else
			return re;
}

static str
seq(M *m, str re) {
	while (*re && wcsncmp(re, L"\\)", 2))
		re=suffix(m, re);
	return re;
}

static
initm(M *m, wchar_t *txt) {
	m->ok=1;
	m->n=0;
	m->sot=txt;
	m->p[0]=txt;
	m->lim[0]=txt;
}

match(struct match *m, wchar_t *re, wchar_t *txt) {
	jmp_buf	trap;
	
	if (!m->trap)
		if (setjmp(*(m->trap=&trap)))
			return 0;
	
	initm(m, txt);
	seq(m, re);
	return OK;
}

search(struct match *m, wchar_t *re, wchar_t *txt) {
	wchar_t	*sot=txt;
	jmp_buf	trap;
	
	if (!m->trap)
		if (setjmp(*(m->trap=&trap)))
			return 0;
	
	initm(m, txt);
	if (*re=='^') {
		seq(m, re+1);
		return OK;
	}
	for(;;) {
		seq(m, re);
		if (OK)
			return 1;
		if (!*txt)
			return 0;
		initm(m, sot);
		m->p[0]=TXT=++txt;
	}
}

_subst(wchar_t *out, struct match *m, wchar_t *repl) {
	int	i,n;
	wchar_t	*s,*e,*o,*p;
	
	jmp_buf	trap;
	if (!m->trap)
		if (setjmp(*(m->trap=&trap)))
			return 0;
	
	i=0;
	o=out;
	while (*repl)
		if (*repl=='\\' && iswdigit(repl[1])) {
			n=repl[1]-'0';
			if (n > 9)
				err(m, L"unset reference", repl);
			repl+=2;
			if (m->n < n)
				continue;
			
			s=m->p[n];
			e=m->lim[n];
			i+=e-s;
			
			if (o)
				while (s<e)
					*o++=*s++;
		} else if (*repl=='\\') {
			repl++;
			if (!(p=wcschr(esc, *repl++)))
				err(m, L"bad escape", repl);
			i++;
			if (o)
				*o++=p[1];
		} else {
			i++;
			if (o)
				*o++=*repl++;
			else
				repl++;
		}
	if (o)
		*o++=0;
	return i;
}

wchar_t*
subst(struct match *m, wchar_t *re, wchar_t *txt, wchar_t *repl) {
	wchar_t	*s;
	int	n;
	
	jmp_buf	trap;
	if (!m->trap)
		if (setjmp(*(m->trap=&trap)))
			return 0;
	
	if (!search(m, re, txt))
		return 0;
	
	n=_subst(0, m, repl);
	s=malloc((n+1)*sizeof (wchar_t));
	_subst(s, m, repl);
	return s;
}


