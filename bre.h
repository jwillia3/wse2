/*
		POSIX BASIC REGULAR EXPRESSION
		    JERRY LEE WILLIAMS JR
			25 MAY 2009
  UNSUPPORTED
  	Equivalence classes
	Coallating classes
	Character classes
			
  ESCAPES - BRE do not define these
	\t	tab
	\n	linefeed
	\r	carriage return
	\\	backslash
	\f	form feed
	\v	vertical tab

  BRACKETED EXPRESSIONS
	\ escapes can be used in brackets contrary to
	POSIX BREs.

  ANCHORS
	^ and $ always act as anchors and never as
	ordary characters except in bracketed
	expressions. (BRE allows this).
  
  EXTENDED OPERATORS
  	+	one or more repetitions
	?	zero or one repetitions

  PERL-STYLE CLASSES
	\a	lowercase letter
	\A	uppercase letter
	\b	word boundary anchor [^a-zA-Z0-9_]
	\B	word boundary anchor [a-zA-Z0-9_]
	\d	digit
	\D	non-digit
	\s	space
	\S	non-space
	\w	word character [a-zA-Z0-9_]
	\W	non-word character [a-zA-Z0-9_]
	\<	beginning of word anchor
	\>	end of word anchor
	

 */
 
struct match {
	int	ok;
	int	n;
	wchar_t	*sot;
	wchar_t	*p[10];
	wchar_t	*lim[10];
	wchar_t	*err_re;
	wchar_t	*err_msg;
	jmp_buf	*trap;
};
 
int	match(struct match *m, wchar_t *re, wchar_t *txt),
	search(struct match *m, wchar_t *re, wchar_t *txt),
	_subst(wchar_t *out, struct match *m, wchar_t *repl);
wchar_t*
	subst(struct match *m, wchar_t *re, wchar_t *txt, wchar_t *repl);
