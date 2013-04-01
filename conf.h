/* vim: set noexpandtab:tabstop=8 */
struct textstyle {
	int		style;
	unsigned	color;
};
struct conf {
	int		tabc;
	int		usetabs;
	int		usebom;
	int		usecrlf;
	int		rows;
	int		cols;
	int		wire[4];
			
	int		bg;
	int		fg;
	int		bg2;
	int		selbg;
	struct textstyle style[8];
	wchar_t		bgimage[128];
	
	/* Font specifications */
	wchar_t		fontname[128];
	double		fontsz;
	double		fontasp;
	double		leading;
	double		weight;
	double		smooth;
	double		blur;
	int		fbx; /* font blur x direction */
	int		fby; /* font blur y direction */
	int		italic;
} conf;

struct lang {
	wchar_t		ext[128];
	wchar_t		comment[128];
	wchar_t		brk[128];
	wchar_t		brace[128];
	wchar_t		kwd[64][256];
	wchar_t		kwdcol[64];
	wchar_t		cmdwrapper[128];
	int		nkwd, commentcol;
} lang;

wchar_t		shell[128];
double		alpha;

int		file_usecrlf;
int		file_usetabs;
int		file_tabc;
int		file_usebom;

wchar_t		*configfile;
int		nconfs;
int		curconf;
struct conf	confset[32];

int		nlangs;
int		curlang;
struct lang	langset[32];

