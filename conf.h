/* vim: set noexpandtab:tabstop=8 */
#define	KWD_MAX		128
struct textstyle {
	int		style;
	unsigned	color;
};
struct conf {		
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
	double		margin_left;
} conf;
struct file {
	int		tabc;
	int		usetabs;
	int		usebom;
	int		usecrlf;
} file;
struct {
	int		rows;
	int		cols;
	int		wire[4];
	double		alpha;
	wchar_t		shell[128];
} global;

struct lang {
	wchar_t		ext[128];
	wchar_t		comment[128];
	wchar_t		brk[128];
	wchar_t		brace[128];
	wchar_t		kwd_re[KWD_MAX][256];
	wchar_t		kwd_comp[KWD_MAX][256];
	wchar_t		kwd_color[KWD_MAX];
	unsigned	kwd_opt[KWD_MAX];
	wchar_t		cmdwrapper[128];
	int		nkwd, commentcol;
} lang;

wchar_t		*configfile;
int		nconfs;
int		curconf;
struct conf	confset[32];

int		nlangs;
int		curlang;
struct lang	langset[32];

