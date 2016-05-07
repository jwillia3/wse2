/* vim: set noexpandtab:tabstop=8 */
#include <math.h>
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
	Keyword
};

struct field {
	wchar_t	*name;
	int	type;
	void	*ptr;
	void	(*exec)(void *);
};

static wchar_t	font_spec[4096];

void nice_colours_bg(void *colourp);
void nice_colours_fg(void *colourp);
static struct	field fields[] = {
		{L"bg", Color, &conf.bg, nice_colours_bg},
		{L"bg2", Color, &conf.bg2},
		{L"fg", Color, &conf.fg, nice_colours_fg},
		{L"gutter_bg", Color, &conf.gutterbg},
		{L"select", Color, &conf.selbg},
		{L"isearch", Color, &conf.isearchbg},
		{L"bookmark", Color, &conf.bookmarkbg},
		{L"bg_image", String, &conf.bgimage},
		
		{L"style1", Style, &conf.style[0]},
		{L"style2", Style, &conf.style[1]},
		{L"style3", Style, &conf.style[2]},
		{L"style4", Style, &conf.style[3]},
		{L"style5", Style, &conf.style[4]},
		{L"style6", Style, &conf.style[5]},
		{L"style7", Style, &conf.style[6]},
		{L"style8", Style, &conf.style[7]},
		{L"fixed_margin", Int, &conf.fixed_margin},
		{L"center", Boolean, &conf.center},
		
		{L"font", String, font_spec},
				
		{L"ext", String, &lang.ext},
		{L"comment", String, &lang.comment},
		{L"break", String, &lang.brk},
		{L"brace", String, &lang.brace},
		{L"auto-close", Boolean, &lang.autoClose},
		{L"kwd", Keyword, 0},
		{L"cmd_wrapper", String, &lang.cmdwrapper},
		
		{L"tab_width", Int, &file.tabc},
		{L"use_tabs", Boolean, &file.usetabs},
		{L"use_bom", Boolean, &file.usebom},
		{L"use_crlf", Boolean, &file.usecrlf},
		
		{L"wire1", Int, global.wire},
		{L"wire2", Int, global.wire+1},
		{L"wire3", Int, global.wire+2},
		{L"wire4", Int, global.wire+3},
		{L"alpha", Float, &global.alpha},
		{L"gamma", Float, &global.gamma},
		{L"shell", String, &global.shell},
		{0}
		};
defglobals() {
	global.alpha = .9;
	global.gamma = 2.2;
	global.wire[0] = 64;
	global.wire[1] = 72;
	global.wire[2] = 80;
	global.wire[3] = 132;
	wcscpy(global.shell, L"cmd");
	return 0;
}
defperfile() {
	file.tabc = 4;
	file.usetabs = 0;
	file.usebom = 0;
	file.usecrlf = 0;
	return 0;
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
	conf.bg2 = RGB(245,245,245);
	conf.fg = RGB(64,64,64);
	conf.selbg = RGB(240,240,255);
	conf.isearchbg = RGB(255,255,0);
	conf.bookmarkbg = RGB(255,200,255);
	wcscpy(conf.bgimage, L"");
	
	wcscpy(conf.fontname, L"Courier New");
	conf.fontweight = 0;
	conf.fontitalic = 0;
	conf.fontsz = 12.0;
	conf.fontasp = 0.0;
	conf.leading = 1.125;
	*conf.fontfeatures = 0;
	*font_spec = 0;
	conf.fixed_margin = 1;
	conf.center = 1;
	return 1;
}

static
configfont(wchar_t *spec) {
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
		wcscpy(conf.fontname, L"Courier new");

}

static
directive(wchar_t *s) {
	int	x;
	wchar_t	*e;

	while (iswspace(*s))
		s++;
	
	if (!*s) {
		configfont(font_spec);
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
	return ((unsigned)(b*255)<<16)+
		((unsigned)(g*255)<<8)+
		((unsigned)(r*255));
}

static
rgb_to_hsv(unsigned rgb, double *h, double *s, double *v) {
	double r = (rgb & 255) / 255.0;
	double g = (rgb >> 8 & 255) / 255.0;
	double b = (rgb >> 16 & 255) / 255.0;
	
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
	int i;
	
	conf.fg = *colour;
	for (i = 0; i < 8; i++)
		conf.style[i].color = conf.fg;
}
void nice_colours_bg(void *colourp) {
	unsigned	colour = *(unsigned*)colourp;
	double		h,s,v;
	
	rgb_to_hsv(colour, &h, &s, &v);
	conf.bg2 = colour;
	conf.gutterbg = hsv_to_rgb(h, s, v >= .5? v - .125: v + .125);
	conf.selbg = hsv_to_rgb(h, s, v >= .5? v - .125: v + .125);
	conf.isearchbg = hsv_to_rgb(h, s, v >= .5? v - .25: v + .25);
	conf.bookmarkbg = hsv_to_rgb(fmod(h + 180, 360), s * .25, v);
	conf.fg = hsv_to_rgb(h, s, v >= .5? v - .5: v + .5);
	nice_colours_fg(&conf.fg);
	conf.selbg = hsv_to_rgb(h, s, v >= .5? v - .1: v + .2);
}

static
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
	else if (swscanf(arg, L"rgb %d %d %d", &r,&g,&b)
	  || swscanf(arg, L"%02x%02x%02x", &r, &g, &b)) {
		return (b<<16)+(g<<8)+r;
	}
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
		style->style = 0;
		for (;;) {
			while (iswspace(*arg))
				arg++;
			if (!wcsncmp(arg, L"bold", 4))
				arg+=4, style->style |= 1;
			else if (!wcsncmp(arg, L"italics", 7))
				arg+=7, style->style |= 2;
			else if (!wcsncmp(arg, L"italic", 6))
				arg+=6, style->style |= 2;
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
	
	wcscpy(wcsrchr(path, L'\\')+1, L"wse.conf");
	if (loadconfig(path)) {
		configfile=wcsdup(path);
		return 1;
	}
	configfile=L"";
	defconfig();
	deflang();
	confset[nconfs++]=conf;
	return 0;
}
