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
	int		active_tab;
	int		inactive_tab;
	int		saved_file;
	int		unsaved_file;
	int		selbg;
	int		isearchbg;
	int		bookmarkbg;
	int		gutterbg;
	struct textstyle style[8];
	int		fixed_margin;
	int		center;
	
	/* Font specifications */
	wchar_t		fontname[128];
	wchar_t		fontfeatures[128];
	double		fontsz;
	double		fontasp;
	int		fontweight;
	int		fontitalic;
	double		leading;
	
	wchar_t		ui_font_name[128];
	double		ui_font_small_size;
	double		ui_font_large_size;
} conf;
struct file {
	int		tabc;
	int		usetabs;
	int		usebom;
	int		usecrlf;
} file;
struct {
	int		line_width;
	double		alpha;
	double		gamma;
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
	int		autoClose;
} lang;

struct scheme {
	wchar_t		filename[128];
	unsigned	color[16];
} scheme;

wchar_t		*configfile;
int		nconfs;
int		curconf;
struct conf	confset[32];

int		nlangs;
int		curlang;
struct lang	langset[32];

