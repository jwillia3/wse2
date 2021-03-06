/* vim: set noexpandtab:tabstop=8 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "conf.h"
#include "wse.h"
#include "re.h"
#include "colour.h"

enum {
	Boolean,
	Int,
	Color,
	Style,
	Float,
	String,
	Keyword,
};

struct field {
	wchar_t	*name;
	int	type;
	void	*ptr;
	void	(*exec)(void *);
};

static wchar_t	font_spec[4096];
static wchar_t	backing_font_spec[256*128];

static unsigned init_colour(double l, double c, double h) { return export_rgb(lchuv_srgb((colour_t){l, c, h})); }
static void pretty_colours_from_bg(void *_);
static void pretty_fg(void *_);
static void pretty_bg(void *_);
static void load_scheme(wchar_t *filename);
static void reset_scheme(wchar_t *ignored);
static void expand_font(wchar_t *spec);
static void expand_backing_fonts(wchar_t *spec);
static struct	field fields[] = {
		{L"bg", Color, &conf.bg, pretty_colours_from_bg},
		{L"bg2", Color, &conf.bg2},
		{L"fg", Color, &conf.fg, pretty_fg},
		{L"cursor-colour", Color, &conf.cursor_colour},
		{L"cursor-color", Color, &conf.cursor_colour},
		{L"brace-bg", Color, &conf.brace_bg},
		{L"bad-brace-bg", Color, &conf.bad_brace_bg},
		{L"gutter-bg", Color, &conf.gutterbg},
		{L"gutter-fg", Color, &conf.gutterbg},
		{L"chrome-bg", Color, &conf.chrome_bg},
		{L"chrome-fg", Color, &conf.chrome_fg},
		{L"select", Color, &conf.selbg},
		{L"isearch", Color, &conf.isearchbg},
		{L"bookmark-bg", Color, &conf.bookmarkbg},
		{L"bookmark-fg", Color, &conf.bookmarkfg},
		{L"current-line", Color, &conf.current_line_bg},
		{L"grid-color", Color, &conf.grid_colour},
		{L"grid-colour", Color, &conf.grid_colour},
		{L"grid", Boolean, &global.grid_enabled},
		
		{L"style1", Style, &conf.style[0]},
		{L"style2", Style, &conf.style[1]},
		{L"style3", Style, &conf.style[2]},
		{L"style4", Style, &conf.style[3]},
		{L"style5", Style, &conf.style[4]},
		{L"style6", Style, &conf.style[5]},
		{L"style7", Style, &conf.style[6]},
		{L"style8", Style, &conf.style[7]},
		
		{L"font", String, font_spec, expand_font},
		{L"font-name", String, &conf.fontname},
		{L"font-size", Float, &conf.fontsz},
		{L"font-aspect", Float, &conf.fontasp},
		{L"font-weight", Int, &conf.fontweight},
		{L"font-italic", Boolean, &conf.fontitalic},
		{L"line-height", Float, &conf.leading},
		{L"backing-fonts", String, backing_font_spec, expand_backing_fonts},
		
		{L"ext", String, &lang.ext},
		{L"comment", String, &lang.comment},
		{L"break", String, &lang.brk},
		{L"brace", String, &lang.brace},
		{L"quote", String, &lang.quotes},
		{L"auto-close", Boolean, &lang.autoClose},
		{L"type-over", Boolean, &lang.typeover},
		{L"kwd", Keyword, 0},
		{L"cmd-wrapper", String, &lang.cmdwrapper},
		
		{L"tab-width", Int, &file.tabc},
		{L"use-tabs", Boolean, &file.usetabs},
		{L"use-bom", Boolean, &file.usebom},
		{L"use-crlf", Boolean, &file.usecrlf},
		
		{L"line-width", Int, &global.line_width},
		{L"ruler", Int, &global.ruler},
		{L"alpha", Float, &global.alpha},
		{L"gamma", Float, &global.gamma},
		{L"gfx-flatness", Float, &global.gfx_flatness},
		{L"gfx-subsamples", Float, &global.gfx_subsamples},
		{L"shell", String, &global.shell},
		{L"fixed-margin", Int, &global.fixed_margin},
		{L"center", Boolean, &global.center},
		{L"minimap", Boolean, &global.minimap},
		{L"line-numbers", Boolean, &global.line_numbers},
		{L"match-braces", Boolean, &global.match_braces},
		{L"highlight-selection", Boolean, &global.highlight_sel},
		{L"ui-font", String, &global.ui_font_name},
		{L"ui-font-small-size", Float, &global.ui_font_small_size},
		{L"ui-font-large-size", Float, &global.ui_font_large_size},
		{L"undo-time", Int, &global.undo_time},
		{L"cursor-fps", Int, &global.cursor_fps},
		{L"cursor-insert-width", Float, &global.cursor_insert_width},
		{L"cursor-overwrite-width", Float, &global.cursor_overwrite_width},
		
		{L"load-scheme", String, &scheme.filename, .exec=load_scheme},
		{L"reset-scheme", String, &scheme.filename, .exec=reset_scheme},
		{L"black", Color, &scheme.color[0]},
		{L"red", Color, &scheme.color[1]},
		{L"dark-red", Color, &scheme.color[1]},
		{L"green", Color, &scheme.color[2]},
		{L"dark-green", Color, &scheme.color[2]},
		{L"brown", Color, &scheme.color[3]},
		{L"gold", Color, &scheme.color[3]},
		{L"amber", Color, &scheme.color[3]},
		{L"dark-amber", Color, &scheme.color[3]},
		{L"dark-yellow", Color, &scheme.color[3]},
		{L"blue", Color, &scheme.color[4]},
		{L"dark-blue", Color, &scheme.color[4]},
		{L"purple", Color, &scheme.color[5]},
		{L"dark-purple", Color, &scheme.color[5]},
		{L"magenta", Color, &scheme.color[5]},
		{L"dark-magenta", Color, &scheme.color[5]},
		{L"aqua", Color, &scheme.color[6]},
		{L"dark-aqua", Color, &scheme.color[6]},
		{L"cyan", Color, &scheme.color[6]},
		{L"dark-cyan", Color, &scheme.color[6]},
		{L"teal", Color, &scheme.color[6]},
		{L"dark-teal", Color, &scheme.color[6]},
		{L"light-grey", Color, &scheme.color[7]},
		{L"grey", Color, &scheme.color[7]},
		{L"silver", Color, &scheme.color[7]},
		{L"dark-grey", Color, &scheme.color[8]},
		{L"light-red", Color, &scheme.color[9]},
		{L"bright-red", Color, &scheme.color[9]},
		{L"light-green", Color, &scheme.color[10]},
		{L"bright-green", Color, &scheme.color[10]},
		{L"yellow", Color, &scheme.color[11]},
		{L"light-yellow", Color, &scheme.color[11]},
		{L"bright-yellow", Color, &scheme.color[11]},
		{L"light-gold", Color, &scheme.color[11]},
		{L"bright-gold", Color, &scheme.color[11]},
		{L"light-amber", Color, &scheme.color[11]},
		{L"bright-amber", Color, &scheme.color[11]},
		{L"bright-blue", Color, &scheme.color[12]},
		{L"light-magenta", Color, &scheme.color[13]},
		{L"bright-magenta", Color, &scheme.color[13]},
		{L"light-purple", Color, &scheme.color[13]},
		{L"bright-purple", Color, &scheme.color[13]},
		{L"bright-aqua", Color, &scheme.color[14]},
		{L"light-aqua", Color, &scheme.color[14]},
		{L"bright-teal", Color, &scheme.color[14]},
		{L"light-teal", Color, &scheme.color[14]},
		{L"bright-cyan", Color, &scheme.color[14]},
		{L"light-cyan", Color, &scheme.color[14]},
		{L"white", Color, &scheme.color[15]},
		{0}
		};
defglobals() {
	global.alpha = .9;
	global.gamma = 2.2;
	global.line_width = 80;
	global.ruler = 80;
	wcscpy(global.alignables, L"-> => = , :");
	global.gfx_flatness = 1.01f;
	global.gfx_subsamples = 3.0f;
	global.grid_enabled = true;
	wcscpy(global.shell, L"cmd");
	global.fixed_margin = 0;
	global.center = 1;
	global.minimap = 1;
	global.line_numbers = 1;
	global.match_braces = 1;
	global.highlight_sel = 1;
	global.undo_time = 600;
	global.cursor_fps = 30;
	global.cursor_overwrite_width = 1.00f;
	global.cursor_insert_width = 0.10f;
	wcscpy(global.ui_font_name, L"Consolas");
	global.ui_font_small_size = 12;
	global.ui_font_large_size = 18;
	return 0;
}
defperfile() {
	file.tabc = 4;
	file.usetabs = 0;
	file.usebom = 0;
	file.usecrlf = 0;
	return 0;
}
static void defscheme() {
	scheme = (struct scheme){
		.filename = L"",
		.color = {
			init_colour(10.0, 0.0, 0.0),
			init_colour(50.0, 75.0, 15.0),
			init_colour(50.0, 75.0, 140.0),
			init_colour(50.0, 75.0, 60.0),
			init_colour(50.0, 75.0, 240.0),
			init_colour(50.0, 75.0, 260.0),
			init_colour(50.0, 75.0, 190.0),
			init_colour(50.0, 0.0, 0.0),
			init_colour(35.0, 0.0, 0.0),
			init_colour(75.0, 75.0, 15.0),
			init_colour(75.0, 75.0, 140.0),
			init_colour(75.0, 75.0, 60.0),
			init_colour(75.0, 75.0, 240.0),
			init_colour(75.0, 75.0, 260.0),
			init_colour(75.0, 75.0, 190.0),
			init_colour(100.0, 0.0, 0.0),
		},
	};
}

static
deflang() {
	*lang.ext=0;
	wcscpy(lang.comment, L"");
	wcscpy(lang.brk, L"~!@#$%^&*()-+={}[]\\|;:'\",.<>/?");
	wcscpy(lang.brace, L"()[]{}");
	wcscpy(lang.quotes, L"\"'");
	memset(lang.kwd_re,0,sizeof lang.kwd_re);
	memset(lang.kwd_comp,0,sizeof lang.kwd_comp);
	memset(lang.kwd_color,0,sizeof lang.kwd_color);
	memset(lang.kwd_opt,0,sizeof lang.kwd_opt);
	wcscpy(lang.cmdwrapper, L"cmd /c %ls & pause >nul");
	lang.autoClose = 1;
	lang.typeover = 1;
	lang.nkwd=0;
}

static void def_font() {
	wcscpy(conf.fontname, L"Consolas");
	conf.fontweight = 0;
	conf.fontitalic = 0;
	conf.fontsz = 12.0;
	conf.fontasp = 0.0;
	conf.leading = 1.25;
	conf.default_style = 0;
	*conf.fontfeatures = 0;
}

static
defconfig() {
	memset(conf.style, 0, sizeof conf.style);
	conf.bg = export_rgb(lchuv_srgb((colour_t){100.0, 0.0, 0.0}));
	pretty_colours_from_bg(&conf.bg);
	def_font();
	*font_spec = 0;
	wcscpy(conf.backing_font[0], L"Consolas");
	wcscpy(conf.backing_font[1], L"Courier New");
	wcscpy(conf.backing_font[2], L"Source Code Pro");
	wcscpy(conf.backing_font[3], L"Dejavu Sans Mono");
	conf.nbacking_fonts = 4;
	return 1;
}

static void expand_backing_fonts(wchar_t *spec) {
	wchar_t *start = spec;
	conf.nbacking_fonts = 0;
	while (1) {
		wchar_t *separator = start + wcscspn(start, L",");
		wchar_t *end = separator;
		while (end > start && iswspace(end[-1])) end--;
		wcsncpy(conf.backing_font[conf.nbacking_fonts], start, end - start);
		conf.backing_font[conf.nbacking_fonts][end - start] = 0;
		conf.nbacking_fonts++;
		if (*separator) separator++;
		while (iswspace(*separator)) separator++;
		if (!*separator) break;
		start = separator;
	};
}
static void expand_font(wchar_t *spec) {
	def_font();
	wchar_t *part = wcstok(spec, L" \t,/");
	wchar_t fontname[128];
	*fontname = 0;
	
	while (part) {
		wchar_t *end;
		float value = wcstod(part, &end);
		
		if (!wcscmp(end, L"pt") && value != 0.0f)
			conf.fontsz = value;
		else if (!wcscmp(end, L"w"))
			conf.fontweight = value;
		else if (!wcscmp(end, L"em"))
			conf.leading = value;
		else if (part[0] == '1' && part[1] == ':')
			conf.fontasp = wcstod(part+2, NULL);
		else if (!wcscmp(end, L":1"))
			conf.fontasp = wcstod(part, NULL),
			conf.fontasp = 1.0f / fabs(conf.fontasp ? conf.fontasp : 1.0f);
		else if (!wcsicmp(part, L"italic")) /* italic style */
			conf.fontitalic = 1;
		else if (!wcsicmp(part, L"bold")) /* bold weight */
			conf.fontweight = 700;
		else if (!wcsicmp(part, L"underline"))
			conf.default_style ^= UNDERLINE_STYLE;
		else if (!wcsicmp(part, L"small-caps"))
			conf.default_style ^= SMALL_CAPS_STYLE;
		else if (!wcsicmp(part, L"all-caps"))
			conf.default_style ^= ALL_CAPS_STYLE;
		else if (*part == '+' && wcslen(part) == 5)
			wcscat(conf.fontfeatures, part+1);
		else { /* part of font name */
			if (*fontname)
				wcscat(fontname, L" ");
			wcscat(fontname, part);
		}
		
		part = wcstok(NULL, L" \t,/");
	}
	if (*fontname) wcscpy(conf.fontname, fontname);
}

static
directive(wchar_t *s) {
	int	x;
	wchar_t	*e;

	while (iswspace(*s))
		s++;
	
	if (!*s) {
		confset[nconfs++]=conf;
		defconfig();
	}
	else if (isdigit(*s))
		selectconfig(wcstol(s, 0, 10));
	else if (*s==L'.') {
		x=0;
		s=lang.ext;
		while (iswspace(*s))
			s++;
		
		while (e=wcschr(s, L',')) {
			*e=0;
			s=e+1;
		}
		s[wcslen(s)+1]=0;
		
		langset[nlangs++]=lang;
		deflang();
	}
	return 1;
}

static colour_t accent(colour_t c, double h) {
	c = adjust_h(c, h);
	c.c = clamp(10.0, c.c, 90.0);
	c.l = clamp(20.0, c.l, 90.0);
	return c;
}

static void pretty_fg(void *_) {
	colour_t fg = srgb_lchab(import_rgb(conf.fg));
	colour_t a1 = accent(fg, 120.0);
	colour_t a2 = accent(fg, 240.0);
	for (int i = 0; i < 8; i++)
		conf.style[i].color = conf.fg;
	conf.bookmarkfg      	= export_lchab(a1);
	conf.chrome_fg       	= export_lchab(fg);
	conf.cursor_colour	= export_lchab(fg);
}
static void pretty_bg(void *_) {
	colour_t bg = srgb_lchab(import_rgb(conf.bg));
	colour_t a1 = accent(bg, 120.0);
	colour_t a2 = accent(bg, 240.0);
	conf.bg2 = conf.bg;
	
	conf.gutterbg        = conf.bg;
	conf.selbg           = export_lchab(a1);
	conf.isearchbg       = export_lchab(a1);
	conf.grid_colour     = export_lchab(enhance_l(bg, -0.05));
	conf.current_line_bg = export_lchab(enhance_l(bg, -0.05));
	conf.brace_bg        = export_lchab(enhance_l(a1, -0.125));
	conf.bad_brace_bg    = export_lchab(enhance_l(a1, -0.25));
	conf.bookmarkbg      = export_lchab(enhance_l(a2, -0.00));
	conf.chrome_bg       = export_lchab(bg);
}
void pretty_colours_from_bg(void *_) {
	colour_t bg = srgb_lchab(import_rgb(conf.bg));
	conf.fg = export_lchab(enhance_l(bg, -0.60));
	pretty_fg(_);
	pretty_bg(_);
}
static void reset_scheme(wchar_t *ignored) {
	defscheme();
}
static void load_termite_scheme(FILE *file) {
	char buf[1024];
	int colour;
	unsigned value;
	while (fgets(buf, sizeof buf, file)) {
		if (2 == sscanf(buf, "color%d = \"#%x\"", &colour, &value) ||
		    2 == sscanf(buf, "color%d = #%x", &colour, &value))
			scheme.color[colour % 16] = value;
		else if (1 == sscanf(buf, "background = \"#%x\"", &value) ||
		         1 == sscanf(buf, "background = #%x", &value))
			scheme.bg = value;
		else if (1 == sscanf(buf, "foreground = \"#%x\"", &value) ||
		         1 == sscanf(buf, "foreground = #%x", &value))
			scheme.fg = value;
		else if (1 == sscanf(buf, "cursor = \"#%x\"", &value) ||
		         1 == sscanf(buf, "cursor = \"#%x\"", &value))
			scheme.cursor_colour = value;
	}
}
static void load_konsole_scheme(FILE *file) {
	char buf[1024];
	int current_colour = -10;
	int r, g, b;
	char ignored;
	while (fgets(buf, sizeof buf, file)) {
		if (2 == sscanf(buf, "[Color%dIntense%c", &current_colour, &ignored) && ignored==']') current_colour += 8;
		else if (1 == sscanf(buf, "[Color%d]", &current_colour));
		else if (!memcmp(buf, "[Background]", 12)) current_colour = -2;
		else if (!memcmp(buf, "[Foreground]", 12)) current_colour = -1;
		else if (3 == sscanf(buf, " Color = %d , %d , %d ", &r, &g, &b)) {
			unsigned colour_value = rgb(r, g, b);
			if (current_colour >= 0 && current_colour < 16)
				scheme.color[current_colour] = colour_value;
			else if (current_colour == -1)
				scheme.fg = colour_value;
			else if (current_colour == -2)
				scheme.bg = colour_value;
		} else current_colour = -10;
	}
	scheme.cursor_colour = scheme.fg;
}
static void load_scheme(wchar_t *filename) {
	wchar_t tmp[256];
	wcscpy(tmp, L"/schemes/");
	wcscat(tmp, filename);
	wchar_t *p = tmp + wcslen(tmp);
	wchar_t **dir = platform_data_path();
	FILE *file;
	
	wcscat(p, L".colorscheme");
	if ((file = platform_open_any(dir, tmp, L"r")))
		load_konsole_scheme(file);
	else if (*p=0, (file = platform_open_any(dir, tmp, L"r")))
		load_termite_scheme(file);
	else return;
	
	fclose(file);
	conf.bg = scheme.bg;
	conf.fg = scheme.fg;
	pretty_fg(0);
	pretty_bg(0);
	conf.cursor_colour = scheme.cursor_colour;
}

static unsigned
getcolor(wchar_t *arg, unsigned colour) {
	wchar_t		name[128];
	int		end;
	colour_t	in = import_rgb(colour), a;
	colour_t	(*colourspace)(colour_t) = lchab_srgb;
	
	while (1 == swscanf(arg, L"%[a-zA-Z-]%n", &name, &end)) {
		bool found = false;
		arg += end;
		struct field *f;
		for (f = fields; f->name; f++)
			if (f->type == Color && !wcscmp(f->name, name)) {
				in = import_rgb(*(unsigned*)f->ptr);
				found = true;
				break;
			}
		if (!found)
			colourspace =
				!wcsicmp(name, L"lchuv") ? lchuv_srgb :
				!wcsicmp(name, L"lchab") ? lchab_srgb :
				!wcsicmp(name, L"luv") ? luv_srgb :
				!wcsicmp(name, L"lab") ? lab_srgb :
				!wcsicmp(name, L"xyz") ? xyz_srgb :
				!wcsicmp(name, L"srgb") ? srgb_srgb :
				!wcsicmp(name, L"rgb") ? rgb8_srgb :
				NULL;
	}
	if (colourspace && 3 == swscanf(arg, L" + %lf %lf %lf", &a.x, &a.y, &a.z))
		return export_rgb(colourspace((colour_t){in.x + a.x, in.y + a.y, in.z + a.z}));
	if (colourspace && 3 == swscanf(arg, L" - %lf %lf %lf", &a.x, &a.y, &a.z))
		return export_rgb(colourspace((colour_t){in.x - a.x, in.y - a.y, in.z - a.z}));
	else if (colourspace && 3 == swscanf(arg, L"%lf %lf %lf", &in.x, &in.y, &in.z))
		return export_rgb(colourspace(in));
	return export_rgb(in);
}

static
configline(int ln, wchar_t *s) {
	wchar_t		*arg;
	struct textstyle *style;
	struct field	*cf;
	
	while (iswspace(*s))
		s++;
	if (*s=='#' || !*s)
		return 0;
	if (*s=='.')
		return directive(s+1);
	
	/* Cut field name */
	for (arg=s; *arg && !iswspace(*arg); arg++);
	*arg++=0;
	while (iswspace(*arg)) arg++;
	
	/* Find field */
	for (cf=fields; cf->name && wcscmp(cf->name, s); cf++);
	if (!cf->name)
		return 0;
	
	/* Remove comment if not string or keyword */
	if (cf->type != String && cf->type != Keyword)
		arg[wcscspn(arg, L"#")] = 0;
	
	switch (cf->type) {
	
	case Boolean:
		if (!wcscmp(arg,L"yes")) *(int*)cf->ptr = 1;
		else if (!wcscmp(arg,L"true")) *(int*)cf->ptr = 1;
		else *(int*)cf->ptr = 0;
		while (*++arg);
		return 1;
	
	case Int:
		*(int*)cf->ptr = wcstol(arg, &arg, 0);
		return 1;

	case Color:
		*(unsigned*)cf->ptr = getcolor(arg, *(unsigned*)cf->ptr);
		if (cf->exec)
			cf->exec(cf->ptr);
		return 1;
	
	case Style:
		style = (struct textstyle*)cf->ptr;
		style->style = conf.default_style;
		for (;;) {
			while (iswspace(*arg))
				arg++;
			if (!wcsncmp(arg, L"bold", 4))
				arg+=4, style->style ^= BOLD_STYLE;
			else if (!wcsncmp(arg, L"italics", 7))
				arg+=7, style->style ^= ITALIC_STYLE;
			else if (!wcsncmp(arg, L"italic", 6))
				arg+=6, style->style ^= ITALIC_STYLE;
			else if (!wcsncmp(arg, L"underline", 9))
				arg+=9, style->style ^= UNDERLINE_STYLE;
			else if (!wcsncmp(arg, L"all-caps", 8))
				arg+=8, style->style ^= ALL_CAPS_STYLE;
			else if (!wcsncmp(arg, L"small-caps", 10))
				arg+=10, style->style ^= SMALL_CAPS_STYLE;
			else
				break;
		}
		style->color = getcolor(arg, style->color);
		return 1;
	
	case Float:
		*(double*)cf->ptr = wcstod(arg, &arg);
		return 1;
	
	case String:
		wcscpy(cf->ptr, arg);
		if (cf->exec)
			cf->exec(cf->ptr);
		return 1;
	
	case Keyword:
		lang.kwd_color[lang.nkwd] = wcstoul(arg,&arg,0) - 1;
		while (iswspace(*arg))
			arg++;
		wcscpy(lang.kwd_re[lang.nkwd], arg);
		re_comp(lang.kwd_comp[lang.nkwd], arg, &lang.kwd_opt[lang.nkwd]);
		lang.nkwd++;
		return 1;
	
	}
	return 0;
}

static
loadconfig(wchar_t *fn) {
	int		c,ln,eol;
	char		*utf;
	wchar_t		*buf, *s;
	void		*f;
	int		sz;
	
	f=platform_openfile(fn,0,&sz);
	if (!f)
		return 0;
	
	utf=malloc(sz+1);
	platform_readfile(f,utf,sz);
	platform_closefile(f);
	buf=decodeutf8(utf,utf+sz);
	free(utf);
	
	defglobals();
	defscheme();
	defconfig();
	deflang();
	confset[nconfs]=conf;
	
	s=buf;
	ln=0;
	while (*s) {
		ln++;
		eol=wcscspn(s, L"\r\n");
		c=s[eol];
		if (c=='\r')
			s[eol++]=0;
		else if (c=='\n')
			s[eol]=0;
		
		configline(ln, s);
		
		s += eol + (c!=0);
	}
	free(buf);
	return 1;
}

selectconfig(int x) {
	if (nconfs<=x || x<0)
		return 0;
	curconf=x;
	conf=confset[x];
	return 1;
}

config() {
	wchar_t	path[512];
	
	nconfs=0;
	nlangs=0;
	platform_bindir(path);
	platform_normalize_path(path);
	
	wcscat(path, L"/wse.conf");
	if (loadconfig(path)) {
		configfile=wcsdup(path);
		return 1;
	}
	configfile=L"";
	defscheme();
	defconfig();
	deflang();
	confset[nconfs++]=conf;
	return 0;
}
