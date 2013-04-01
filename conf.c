/* vim: set noexpandtab:tabstop=8 */
#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <Windows.h>
#include <math.h>
#include <stdlib.h>
#include <wchar.h>
#include "conf.h"
#include "wse.h"
#include "re.h"

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
};

static struct	field fields[] = {
		{L"bg_color", Color, &conf.bg},
		{L"bg_color2", Color, &conf.bg2},
		{L"fg_color", Color, &conf.fg},
		{L"select_color", Color, &conf.selbg},
		{L"bg_image", String, &conf.bgimage},
		
		{L"style0", Style, &conf.style[0]},
		{L"style1", Style, &conf.style[1]},
		{L"style2", Style, &conf.style[2]},
		{L"style3", Style, &conf.style[3]},
		{L"style4", Style, &conf.style[4]},
		{L"style5", Style, &conf.style[5]},
		{L"style6", Style, &conf.style[6]},
		{L"style7", Style, &conf.style[7]},
		
		{L"font_name", String, &conf.fontname},
		{L"font_size", Float, &conf.fontsz},
		{L"font_aspect", Float, &conf.fontasp},
		{L"font_weight", Float, &conf.weight},
		{L"font_smoothing", Float, &conf.smooth},
		{L"font_italic", Boolean, &conf.italic},
		{L"line_height", Float, &conf.leading},
		{L"font_blur", Float, &conf.blur},
		{L"font_blur_x", Int, &conf.fbx},
		{L"font_blur_y", Int, &conf.fby},
		
		{L"ext", String, &lang.ext},
		{L"comment", String, &lang.comment},
		{L"comment_color", Int, &lang.commentcol},
		{L"break", String, &lang.brk},
		{L"brace", String, &lang.brace},
		{L"kwd", Keyword, 0},
		{L"cmd_wrapper", String, &lang.cmdwrapper},
		
		{L"wire", Int, file.wire},
		{L"wire2", Int, file.wire+1},
		{L"wire3", Int, file.wire+2},
		{L"wire4", Int, file.wire+3},
		{L"tab_width", Int, &file.tabc},
		{L"use_tabs", Boolean, &file.usetabs},
		{L"use_bom", Boolean, &file.usebom},
		{L"use_crlf", Boolean, &file.usecrlf},
		{L"cols", Int, &file.cols},
		{L"rows", Int, &file.rows},		
		{L"alpha", Float, &file.alpha},
		{L"shell", String, &file.shell},
		{0}
		};

defaultperfile() {
	file.alpha = .9;
	file.rows = 80;
	file.cols = 25;
	file.wire[0] = 64;
	file.wire[1] = 72;
	file.wire[2] = 80;
	file.wire[3] = 132;
	file.tabc = 4;
	file.usetabs = 0;
	file.usebom = 0;
	file.usecrlf = 0;
	wcscpy(file.shell, L"cmd");
	return 0;
}

static
deflang() {
	*lang.ext=0;
	wcscpy(lang.comment, L"");
	wcscpy(lang.brk, L"~!@#$%^&*()-+={}[]\\|;:'\",.<>/?");
	wcscpy(lang.brace, L"()[]{}''\"\"<>``");
	memset(lang.kwd,0,sizeof lang.kwd);
	wcscpy(lang.cmdwrapper, L"cmd /c %ls & pause >nul");
	lang.nkwd=0;
	lang.commentcol=0;
}

static
defconfig() {
	memset(conf.style, 0, sizeof conf.style);
	conf.bg = RGB(255,255,255);
	conf.bg2 = RGB(245,245,245);
	conf.fg = RGB(64,64,64);
	conf.selbg = RGB(160,160,192);
	conf.style[0].color = RGB(160,160,192);
	wcscpy(conf.bgimage, L"");
	
	file.tabc = 4;
	file.usetabs = 0;
	file.usebom = 0;
	file.usecrlf = 0;
	file.wire[0] = 64;
	file.wire[1] = 72;
	file.wire[2] = 80;
	file.wire[3] = 128;
	file.cols = 80;
	file.rows = 24;
	file.alpha = .9;
	
	wcscpy(conf.fontname, L"Courier New");
	conf.fontsz = 12.0;
	conf.fontasp = 0.0;
	conf.leading = 1.5;
	conf.smooth = 1.0;
	conf.italic = 0;
	conf.weight = .4;
	conf.blur = .2;
	conf.fbx = 0;
	conf.fby = -1;
	return 1;
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
		
		langset[nlangs++]=lang;
		deflang();
	}
	return 1;
}

static
getcolor(wchar_t *arg) {
	double		h, s, v; /* hue,chroma,luma */
	double		y; /* luma (Y'601) */
	int		r,g,b;
	if (3 == swscanf(arg, L"hsy %lf %lf %lf", &h,&s,&y)) {
		float x, r1,g1,b1, m;
		h /= 60.0;
		x = s * (1 - fabs(fmod(h, 2) - 1));
		if (0 <= h && h <= 1)	r1 = s, g1 = x, b1 = 0;
		else if (h <= 2)	r1 = x, g1 = s, b1 = 0;
		else if (h <= 3) 	r1 = 0, g1 = s, b1 = x;
		else if (h <= 4) 	r1 = 0, g1 = x, b1 = s;
		else if (h <= 5) 	r1 = x, g1 = 0, b1 = s;
		else if (h <= 6) 	r1 = s, g1 = 0, b1 = x;
		else	 		r1 = 0, g1 = 0, b1 = 0;
		m = y - (.3*r1 + .59*g1 + .11*b1);
		r = 255 * (r1 + m);
		g = 255 * (g1 + m);
		b = 255 * (b1 + m);
		if (r < 0) r = 0; else if (r > 255) r = 255;
		if (g < 0) g = 0; else if (g > 255) g = 255;
		if (b < 0) b = 0; else if (b > 255) b = 255;
		return ((unsigned)b<<16)+
			((unsigned)g<<8)+
			((unsigned)r);
	} else if (3 == swscanf(arg, L"hsl %lf %lf %lf", &h,&s,&v)) {
		double r,g,b, f, p,q,t;
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
	} else if (3 == swscanf(arg, L"%d %d %d", &r,&g,&b)
	  || 3 == swscanf(arg, L"rgb %d %d %d", &r,&g,&b)) {
		return (b<<16)+(g<<8)+r;
	}
	return 0;
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
		*(int*)cf->ptr = getcolor(arg);
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
		style->color = getcolor(arg);
		return 1;
	
	case Float:
		*(double*)cf->ptr = wcstod(arg, &arg);
		return 1;
	
	case String:
		wcscpy(cf->ptr, arg);
		return 1;
	
	case Keyword:
		lang.kwdcol[lang.nkwd] = wcstoul(arg,&arg,0);
		while (iswspace(*arg))
			arg++;
		re_comp(lang.kwd[lang.nkwd], arg);
		lang.nkwd++;
		return 1;
	}
	return 0;
}

static
loadconfig(wchar_t *fn) {
	HANDLE		file;
	DWORD		sz, ign;
	int		c,ln,eol;
	char		*utf;
	wchar_t		*buf, *s;
	
	file=CreateFile(fn, GENERIC_READ, FILE_SHARE_READ, 0,
		OPEN_EXISTING, 0, 0);
	if (file==INVALID_HANDLE_VALUE)
		return 0;
	
	sz=GetFileSize(file, 0);
	utf=malloc(sz+1);
	ReadFile(file, utf, sz, &ign, 0);
	CloseHandle(file);
	utf[sz]=0;
	
	sz=MultiByteToWideChar(CP_UTF8, 0, utf, sz+1, 0, 0);
	buf=malloc((sz+1) * sizeof (wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, utf, sz+1, buf, sz+1);
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
	wchar_t	path[MAX_PATH];
	
	nconfs=0;
	nlangs=0;
	
	GetModuleFileName(0, path, MAX_PATH);
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
