/* vim: set noexpandtab:tabstop=8 */
typedef struct Codec	Codec;
typedef struct Line	Line;
typedef struct Buf	Buf;
typedef struct Loc	Loc;
typedef struct Undo	Undo;

struct	Loc {
	int	ln;
	int	ind;
};

struct	Line {
	wchar_t	*dat;
	int	len;
	int	max;
};

struct	Codec {
	wchar_t	*name;
	int	(*sign)(unsigned char*);
	wchar_t *(*dec)(unsigned char*,int);
	int	(*enc)(unsigned char*,wchar_t*,int);
};

enum {
	UndoSwap,
	UndoDelete,
	UndoInsert,
	UndoGroup,
};

struct Undo {
	int	type;
	Line	*dat;
	int	lo;
	int	hi;
	Loc	car;
	Undo	*grp;
	Undo	*next;
};

struct	Buf {
	Line	*dat;
	int	nlines;
	int	max;
	Loc	car;
	Loc	sel;
	Undo	*undo;
	Undo	*redo;
	int	changes;
};

#define CAR	(b->car)
#define SEL	(b->sel)
#define LN	(CAR.ln)
#define IND	(CAR.ind)
#define SLN	(SEL.ln)
#define SIND	(SEL.ind)
#define NLINES	(b->nlines)

Buf		*b;
wchar_t		*filename;
wchar_t		filepath[512];
wchar_t		filebase[512];
wchar_t		fileext[512];
wchar_t		lastcmd[512];
wchar_t		wrapbefore[512];
wchar_t		wrapafter[512];
int		top;
int		vis;
char		brktbl[65536];
wchar_t		opentbl[65536];
wchar_t		closetbl[65536];
wchar_t		*latch;
Codec		*codec;

int		config(),
		selectconfig(int n),
		configfont(),
		defaultperfile();

Codec*		setcodec(wchar_t *name);

int		_act(int action),
		_actins(int c),
		_actquery(wchar_t *query, int down, int sens),
		_actreplace(wchar_t *query, wchar_t *repl, int down, int sens),
		_actreplaceall(wchar_t *query, wchar_t *repl, int down, int sens);

wchar_t		*copysel();

int		record(int type, int lo, int hi),
		undo(Undo **stk),
		clearundo(Undo **stk);
		
int		load(wchar_t *fn, wchar_t *encoding),
		save(wchar_t *fn);

int		samerange(Loc *lo1, Loc *hi1, Loc *lo2, Loc *hi2),
		sameloc(Loc *x, Loc *y),
		cmploc(Loc *x, Loc *y),
		ordersel(Loc *lo, Loc *hi),
		px2line(int px),
		line2px(int ln),
		col2ind(int ln, int col),
		ind2col(int ln, int ind),
		ind2px(int ln, int ind),
		col2px(int ln, int col);

int		inslb(Buf *b, int ln, wchar_t *txt, int len),
		insb(Buf *b, int c),
		delb(Buf *b),
		dellb(Buf *b, int ln),
		gob(Buf *b, int ln, int col),
		lenb(Buf *b, int ln),
		initb(Buf *b),
		clearb(Buf *b),
		sat(int lo, int x, int hi);
		
wchar_t*	getb(Buf *b, int ln, int *len);
