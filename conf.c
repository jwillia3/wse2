/* vim: set noexpandtab:tabstop=8 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "conf.h"
#include "wse.h"
#include "re.h"

#define RGB(r,g,b) (((b&255)<<16)+((g&255)<<8)+(r&255))

enum {
	Boolean,
	Int,
	Color,
	Style,
	Float,
	String,
	Keyword,
	AltGr,
	ShiftAltGr,
};

struct field {
	wchar_t	*name;
	int	type;
	void	*ptr;
	void	(*exec)(void *);
};

static wchar_t	font_spec[4096];
static wchar_t	backing_font_spec[256*128];

void nice_colours_bg(void *colourp);
void nice_colours_fg(void *colourp);
void load_scheme(wchar_t *filename);
void expand_font(wchar_t *spec);
static void expand_backing_fonts(wchar_t *spec);
static struct	field fields[] = {
		{L"bg", Color, &conf.bg, nice_colours_bg},
		{L"bg2", Color, &conf.bg2},
		{L"fg", Color, &conf.fg, nice_colours_fg},
		{L"gutter", Color, &conf.gutterbg},
		{L"chrome-bg", Color, &conf.chrome_bg},
		{L"chrome-fg", Color, &conf.chrome_fg},
		{L"chrome-active-bg", Color, &conf.chrome_active_bg},
		{L"chrome-active-fg", Color, &conf.chrome_active_fg},
		{L"chrome-inactive-bg", Color, &conf.chrome_inactive_bg},
		{L"chrome-inactive-fg", Color, &conf.chrome_inactive_fg},
		{L"chrome-alert-fg", Color, &conf.chrome_alert_fg},
		{L"select", Color, &conf.selbg},
		{L"isearch", Color, &conf.isearchbg},
		{L"bookmark", Color, &conf.bookmarkbg},
		{L"current-line", Color, &conf.current_line_bg},
		
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
		{L"auto-close", Boolean, &lang.autoClose},
		{L"kwd", Keyword, 0},
		{L"cmd-wrapper", String, &lang.cmdwrapper},
		
		{L"tab-width", Int, &file.tabc},
		{L"use-tabs", Boolean, &file.usetabs},
		{L"use-bom", Boolean, &file.usebom},
		{L"use-crlf", Boolean, &file.usecrlf},
		
		{L"line-width", Int, &global.line_width},
		{L"alpha", Float, &global.alpha},
		{L"gamma", Float, &global.gamma},
		{L"gfx-flatness", Float, &global.gfx_flatness},
		{L"gfx-subsamples", Float, &global.gfx_subsamples},
		{L"shell", String, &global.shell},
		{L"fixed-margin", Int, &global.fixed_margin},
		{L"center", Boolean, &global.center},
		{L"minimap", Boolean, &global.minimap},
		{L"ui-font", String, &global.ui_font_name},
		{L"ui-font-small-size", Float, &global.ui_font_small_size},
		{L"ui-font-large-size", Float, &global.ui_font_large_size},
		
		{L"altgr", AltGr, NULL},
		{L"shift-altgr", ShiftAltGr, NULL},
		
		{L"load-scheme", String, &scheme.filename, .exec=load_scheme},
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
		{L"light-blue", Color, &scheme.color[12]},
		{L"bright-blue", Color, &scheme.color[12]},
		{L"light-magenta", Color, &scheme.color[13]},
		{L"bright-magenta", Color, &scheme.color[13]},
		{L"light-purple", Color, &scheme.color[13]},
		{L"bright-purple", Color, &scheme.color[13]},
		{L"bright-cyan", Color, &scheme.color[14]},
		{L"light-cyan", Color, &scheme.color[14]},
		{L"white", Color, &scheme.color[15]},
		{0}
		};
defglobals() {
	global.alpha = .9;
	global.gamma = 2.2;
	global.line_width = 80;
	global.gfx_flatness = 1.01f;
	global.gfx_subsamples = 3.0f;
	wcscpy(global.shell, L"cmd");
	global.fixed_margin = 0;
	global.center = 1;
	global.minimap = 0;
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
static unsigned rgb(uint8_t r, uint8_t g, uint8_t b) {
	return 0xff000000 + (r << 16) + (g << 8) + b;
}
static void defscheme() {
	scheme = (struct scheme){
		.filename = L"",
		.color[0] = rgb(0, 0, 0),
		.color[1] = rgb(194, 54, 33),
		.color[2] = rgb(37, 188, 36),
		.color[3] = rgb(173, 173, 39),
		.color[4] = rgb(73, 46, 225),
		.color[5] = rgb(211, 56, 211),
		.color[6] = rgb(51, 187, 200),
		.color[7] = rgb(203, 204, 205),
		.color[8] = rgb(129, 131, 131),
		.color[9] = rgb(252,57,31),
		.color[10] = rgb(49, 231, 34),
		.color[11] = rgb(234, 236, 35),
		.color[12] = rgb(88, 51, 255),
		.color[13] = rgb(249, 53, 248),
		.color[14] = rgb(20, 240, 240),
		.color[15] = rgb(233, 235, 235),
	};
}

static
deflang() {
	*lang.ext=0;
	wcscpy(lang.comment, L"");
	wcscpy(lang.brk, L"~!@#$%^&*()-+={}[]\\|;:'\",.<>/?");
	wcscpy(lang.brace, L"()[]{}");
	memset(lang.kwd_re,0,sizeof lang.kwd_re);
	memset(lang.kwd_comp,0,sizeof lang.kwd_comp);
	memset(lang.kwd_color,0,sizeof lang.kwd_color);
	memset(lang.kwd_opt,0,sizeof lang.kwd_opt);
	wcscpy(lang.cmdwrapper, L"cmd /c %ls & pause >nul");
	lang.autoClose = 1;
	lang.nkwd=0;
}

static
defconfig() {
	memset(conf.style, 0, sizeof conf.style);
	conf.bg = RGB(255,255,255);
	nice_colours_bg(&conf.bg);
	
	wcscpy(conf.fontname, L"Consolas");
	conf.fontweight = 0;
	conf.fontitalic = 0;
	conf.fontsz = 12.0;
	conf.fontasp = 0.0;
	conf.leading = 1.25;
	conf.default_style = 0;
	*conf.fontfeatures = 0;
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
	wchar_t *part = wcstok(spec, L" \t,/");
	*conf.fontname = 0;
	*conf.fontfeatures = 0;
	
	while (part) {
		wchar_t *end;
		float value = wcstod(part, &end);
		
		if (!wcscmp(end, L"pt"))
			conf.fontsz = value;
		else if (!wcscmp(end, L"w"))
			conf.fontweight = value;
		else if (!wcscmp(end, L"em"))
			conf.leading = value;
		else if (*part == ':')
			conf.fontasp = wcstod(part+1, NULL);
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
			if (*conf.fontname)
				wcscat(conf.fontname, L" ");
			wcscat(conf.fontname, part);
		}
		
		part = wcstok(NULL, L" \t,/");
	}
	
	if (!*conf.fontname)
		wcscpy(conf.fontname, L"Consolas");

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

static unsigned
hsv_to_rgb(double h, double s, double v) {
	double r,g,b, f, p,q,t;
	while (h < 0)
		h += 360;
	h = fmod(h, 360);
	h /= 60.0;
	f = h - floor(h);
	p = v * (1.0 - s);
	q = v * (1.0 - s * f);
	t = v * (1.0 - s * (1.0 - f));
	if (h < 1.0) r=v, g=t, b=p;
	else if (h < 2.0) r=q, g=v, b=p;
	else if (h < 3.0) r=p, g=v, b=t;
	else if (h < 4.0) r=p, g=q, b=v;
	else if (h < 5.0) r=t, g=p, b=v;
	else r=v, g=p, b=q;
	return 0xff000000 +
		((unsigned)(r*255)<<16)+
		((unsigned)(g*255)<<8)+
		((unsigned)(b*255));
}

static
rgb_to_hsv(unsigned rgb, double *h, double *s, double *v) {
	double b = (rgb & 255) / 255.0;
	double g = (rgb >> 8 & 255) / 255.0;
	double r = (rgb >> 16 & 255) / 255.0;
	
	double maxc = max(r, max(g, b));
	double minc = min(r, min(g, b));
	double c = maxc - minc;
	
	*h =60 * (c == 0? 0.0:
		maxc == r? fmod((g - b) / c, 6.0):
		maxc == g? (b - r) / c + 2.0:
		(r-g)/c + 4.0);
	*v = maxc;
	*s = maxc? c / *v: 0.0;
}

void nice_colours_fg(unsigned *colour) {
	int	i;
	double 	h,s,v;
	
	rgb_to_hsv(*colour, &h, &s, &v);
	
	conf.fg = *colour;
	for (i = 0; i < 8; i++)
		conf.style[i].color = conf.fg;
}
void nice_colours_bg(void *colourp) {
	unsigned	colour = *(unsigned*)colourp;
	double		h,s,v, fg_v;
	
	rgb_to_hsv(colour, &h, &s, &v);
	fg_v = v >= .5? v - .5: v + .5;
	conf.bg = colour;
	conf.bg2 = colour;
	conf.gutterbg = hsv_to_rgb(h, s, v);
	conf.selbg = hsv_to_rgb(h, s, v >= .5? v - .1: v + .2);
	conf.current_line_bg = hsv_to_rgb(h, s, v >= .5? v - .05: v + .1);
	conf.isearchbg = hsv_to_rgb(h, s, v >= .5? v - .25: v + .25);
	conf.bookmarkbg = hsv_to_rgb(fmod(h + 90, 360), s < 0.125f ? 0.125f : s * 0.25f, v);
	conf.fg = hsv_to_rgb(h, s, fg_v);
	nice_colours_fg(&conf.fg);
	
	conf.chrome_fg = hsv_to_rgb(0, 0, fg_v);
	conf.chrome_bg = hsv_to_rgb(0, 0, v);
	conf.chrome_inactive_bg = hsv_to_rgb(h, s, v);
	conf.chrome_inactive_fg = hsv_to_rgb(h, s, v + (v < 0.5f ? 0.1f : -0.1f));
	conf.chrome_active_bg = hsv_to_rgb(h, s, min(v + 0.2f, 1.0f));
	conf.chrome_active_fg = hsv_to_rgb(h, s, v < 0.5f ? 1.0f : 0.0f);
	conf.chrome_alert_fg = hsv_to_rgb(10, 1, 0.75f);
}
void load_scheme(wchar_t *filename) {
	FILE *file = _wfopen(filename, L"r");
	if (!file) return;
	char buf[1024];
	int current_colour = -10;
	int r, g, b;
	char ignored;
	while (fgets(buf, sizeof buf, file)) {
		if (2 == sscanf(buf, "[Color%dIntense%c", &current_colour, &ignored) && ignored==']') current_colour += 8;
		else if (1 == sscanf(buf, "[Color%d]", &current_colour));
		else if (!memcmp(buf, "[Background]", 12)) current_colour = -2;
		else if (!memcmp(buf, "[Foreground]", 12)) current_colour = -1;
		else if (3 == sscanf(buf, "Color=%d,%d,%d", &r, &g, &b)) {
			unsigned colour_value = rgb(r, g, b);
			if (current_colour >= 0 && current_colour < 16)
				scheme.color[current_colour] = colour_value;
			else if (current_colour == -1)
				scheme.fg = colour_value;
			else if (current_colour == -2)
				scheme.bg = colour_value;
		}
	}
	fclose(file);
	nice_colours_bg(&scheme.bg);
	nice_colours_fg(&scheme.fg);
}

static unsigned
getcolor(wchar_t *arg, unsigned colour) {
	wchar_t		name[128];
	double		h, s, v; /* hue,chroma,luma */
	double		y; /* luma (Y'601) */
	int		r,g,b;
	
	if (4 == swscanf(arg, L"%ls + %lf %lf %lf", name, &h,&s,&v)
	 || 4 == swscanf(arg, L"%ls + hsl %lf %lf %lf", name, &h,&s,&v)) {
		double oh, os, ov;
		struct field *f;
		rgb_to_hsv(colour, &oh, &os, &ov);
		for (f = fields; f->name; f++)
			if (f->type == Color && !wcscmp(f->name, name)) {
				rgb_to_hsv(*(unsigned*)f->ptr, &oh, &os, &ov);
				break;
			}
		return hsv_to_rgb(fmod(oh+h,360), max(0,min(os+s,1)), max(0,min(v+ov,1)));
	} else if (3 == swscanf(arg, L"%lf %lf %lf", &h,&s,&v)
	 || 3 == swscanf(arg, L"hsl %lf %lf %lf", &h,&s,&v))
		return hsv_to_rgb(h,s,v);
	else if (3 == swscanf(arg, L"rgb %d %d %d", &r,&g,&b)
	  || 3 == swscanf(arg, L"%02x%02x%02x", &r, &g, &b)) {
		return 0xff000000+(r<<16)+(g<<8)+b;
	} else for (struct field *f = fields; f->name; f++)
		if (f->type == Color && !wcscmp(f->name, arg))
			return *(unsigned*)f->ptr;
	return colour;
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
	
	case ShiftAltGr:
	case AltGr:
		while (*arg && iswspace(*arg)) arg++;
		if (*arg) {
			wchar_t from = *arg++;
			while (*arg && iswspace(*arg)) arg++;
			wchar_t to = *arg;
			(cf->type == AltGr ? global.altgr : global.shift_altgr)[toupper(from)] = to;
		}
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
