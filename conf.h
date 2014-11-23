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
	int		isearchbg;
	int		bookmarkbg;
	struct textstyle style[8];
	wchar_t		bgimage[128];
	int		fixed_margin;
	double		margin_percent;
		
	/* Font specifications */
	wchar_t		fontname[128];
	wchar_t		fontfeatures[128];
	double		fontsz;
	double		fontasp;
	int		fontweight;
	int		fontitalic;
	double		leading;
} conf;
struct file {
	int		tabc;
	int		usetabs;
	int		usebom;
	int		usecrlf;
} file;
struct {
	int		initwidth;
	int		initheight;
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
	int		nkwd;
} lang;

wchar_t		*configfile;
int		nconfs;
int		curconf;
struct conf	confset[32];

int		nlangs;
int		curlang;
struct lang	langset[32];

