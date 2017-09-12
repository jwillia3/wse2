/* vim: set noexpandtab:tabstop=8 */
#define	KWD_MAX		128
enum style_flag {
	BOLD_STYLE       = 1,
	ITALIC_STYLE     = 2,
	UNDERLINE_STYLE  = 4,
	ALL_CAPS_STYLE   = 8,
	SMALL_CAPS_STYLE = 16,
};
struct textstyle {
	enum style_flag	style;
	unsigned	color;
};
struct conf {		
	int		bg;
	int		fg;
	int		bg2;
	int		brace_fg;
	int		selbg;
	int		isearchbg;
	int		bookmarkfg;
	int		current_line_bg;
	int		gutterbg;
	int		gutterfg;
	int		default_style;
	int		chrome_bg;
	int		chrome_fg;
	int		chrome_active_bg;
	int		chrome_active_fg;
	int		chrome_inactive_bg;
	int		chrome_inactive_fg;
	int		chrome_alert_fg;
	struct textstyle style[8];
		
	/* Font specifications */
	wchar_t		fontname[128];
	wchar_t		fontfeatures[128];
	double		fontsz;
	double		fontasp;
	int		fontweight;
	int		fontitalic;
	double		leading;
	
	wchar_t		backing_font[128][8];
	int		nbacking_fonts;
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
	double		gfx_flatness;
	double		gfx_subsamples;
	wchar_t		shell[128];
	int		fixed_margin;
	int		line_numbers;
	int		center;
	int		minimap;
	int		match_braces;
	int		undo_time;
	int		cursor_fps;
	double		cursor_overwrite_width;
	double		cursor_insert_width;
	wchar_t		ui_font_name[128];
	double		ui_font_small_size;
	double		ui_font_large_size;
	wchar_t		altgr[65536];
	wchar_t		shift_altgr[65536];
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
	int		typeover;
} lang;

struct scheme {
	wchar_t		filename[128];
	unsigned	color[16];
	unsigned	bg;
	unsigned	fg;
} scheme;

wchar_t		*configfile;
int		nconfs;
int		curconf;
struct conf	confset[32];

int		nlangs;
int		curlang;
struct lang	langset[32];

