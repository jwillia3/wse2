struct conf {
	int		tabc;
	int		rows;
	int		cols;
	int		wire;
			
	int		bg;
	int		fg;
	int		selbg;
	int		color[16];
	wchar_t		bgimage[128];
	
	int		doublebuffer;
	
	/* Font specifications */
	wchar_t		fontname[128];
	double		fontsz;
	double		fontasp;
	double		leading;
	double		weight;
	double		smooth;
	int		italic;
	
	/* Derived Font metrics */
	int		aheight; /* ascender height */
	int		lheight; /* height w/ leading */
	int		em;
	int		tabw;
	int		widths[65536];
} conf;

struct lang {
	wchar_t		ext[128];
	wchar_t		comment[128];
	wchar_t		brk[128];
	wchar_t		brace[128];
	wchar_t		kwd[64][256];
	wchar_t		kwdcol[64];
	wchar_t		kwdbold[64];
	int		nkwd, commentcol;
} lang;

wchar_t		shell[128];

wchar_t		*configfile;
int		nconfs;
int		curconf;
struct conf	confset[32];

int		nlangs;
int		curlang;
struct lang	langset[32];

