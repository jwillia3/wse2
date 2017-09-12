/* vim: set noexpandtab:tabstop=8 */
#include <stdbool.h>
#include <stdint.h>
typedef struct Codec	Codec;
typedef struct Line	Line;
typedef struct Buf	Buf;
typedef struct Loc	Loc;
typedef struct Undo	Undo;
typedef struct Bookmark	Bookmark;
typedef struct Scanner	Scanner;

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

struct Scanner {
	Buf	*b;
	union {
		Loc;
		Loc loc;
	};
	wchar_t	c;
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
	uint64_t timestamp;
};

struct	Buf {
	Line	*dat;
	int	nlines;
	int	max;
	Loc	car;
	Loc	sel;
	Bookmark *bookmarks;
	Undo	*undo;
	Undo	*redo;
	int	changes;
};

struct	Bookmark {
	int		line;
	Bookmark	*prev;
	Bookmark	*next;
};

#define CAR	(b->car)
#define SEL	(b->sel)
#define LN	(CAR.ln)
#define IND	(CAR.ind)
#define SLN	(SEL.ln)
#define SIND	(SEL.ind)
#define NLINES	(b->nlines)

wchar_t		lastcmd[512];
int		top;
int		vis;
char		brktbl[65536];
wchar_t		opentbl[65536];
wchar_t		closetbl[65536];
wchar_t		*latch;
Codec		*codec;
int		overwrite;

int		config(),
		selectconfig(int n),
		configfont(),
		defperfile(),
		defglobals();
int		addbookmark(Buf *b, int line),
		deletebookmark(Buf *b, int line),
		isbookmarked(Buf *b, int line);

Codec*		setcodec(wchar_t *name);
uint64_t	platform_time_ms();
wchar_t		*platform_normalize_path(wchar_t *filename);
wchar_t 	**platform_list_directory(wchar_t *directory, int *countp);
void		*platform_openfile(wchar_t *name, int write, int *sz);
void		platform_closefile(void *file);
void		platform_writefile(void *f, void *buf, int sz);
void		platform_readfile(void *f, void *buf, int sz);
wchar_t		*platform_program_directory(wchar_t *path);
void		platform_cd(wchar_t *path);
unsigned char*	encodeutf8(wchar_t *in, wchar_t *end);
int		encodeutf8to(unsigned char *out, wchar_t *in, wchar_t *end);
wchar_t*	decodeutf8(unsigned char *in, unsigned char *end);
wchar_t*	wcsistr(wchar_t *big, wchar_t *substring);

int		_act(Buf *b, int action),
		_actins(Buf *b, int c),
		_actquery(Buf *b, wchar_t *query, int down, int sens),
		actisearch(Buf *b, wchar_t *query, int down, int skip),
		_actreplace(Buf *b, wchar_t *query, wchar_t *repl, int down, int sens),
		_actreplaceall(Buf *b, wchar_t *query, wchar_t *repl, int down, int sens);

wchar_t		*copysel(Buf *b);

int		record(Buf *b, int type, int lo, int hi),
		undo(Buf *b, Undo **stk),
		clearundo(Buf *b, Undo **stk),
		undosuntil(Buf *b, Undo *mark);
Undo		*topundo(Buf *b);
		
		
int		load(Buf *b, wchar_t *fn, wchar_t *encoding),
		save(Buf *b, wchar_t *fn);

int		samerange(Loc *lo1, Loc *hi1, Loc *lo2, Loc *hi2),
		sameloc(Loc *x, Loc *y),
		cmploc(Loc *x, Loc *y),
		ordersel(Buf *b, Loc *lo, Loc *hi),
		col2ind(Buf *b, int ln, int col),
		ind2col(Buf *b, int ln, int ind);

Buf 		*newb();
void		freeb(Buf *b);
int		inslb(Buf *b, int ln, wchar_t *txt, int len),
		insb(Buf *b, int c),
		delb(Buf *b),
		delatb(Buf *b, int index),
		dellb(Buf *b, int ln),
		gob(Buf *b, int ln, int col),
		lenb(Buf *b, int ln),
		initb(Buf *b),
		clearb(Buf *b),
		sat(int lo, int x, int hi),
		forward(Scanner *scan),
		backward(Scanner *scan);
Scanner 	getscanner(Buf *b, int line, int ind),
		endscanner(Buf *b),
		startscanner(Buf *b),
		matchbrace(Scanner scan, bool allow_back, bool allow_forward);
		
wchar_t*	getb(Buf *b, int ln, int *len);
