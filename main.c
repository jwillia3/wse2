/* vim: set noexpandtab:tabstop=8 */

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0501
#define STRICT
#define UNICODE
#pragma comment(lib,"user32.lib")
#pragma comment(lib,"gdi32.lib")
#pragma comment(lib,"shell32.lib")
#pragma comment(lib,"comdlg32.lib")
#pragma comment(lib,"uxtheme.lib")


#pragma warning(push)
#pragma warning(disable: 4005)
#include <Windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#pragma warning(pop)
#include <pg2/pg.h>
#include "conf.h"
#include "wse.h"
#include "action.h"

enum { MDT_EFFECTIVE_DPI = 0 };
HRESULT (WINAPI *GetDpiForMonitor)(HMONITOR, int, unsigned*, unsigned*);

#define	BOT	(top+vis)
#define TAB	tabs[current_tab]

#undef CAR
#undef SEL
#undef NLINES
#define CAR	(TAB.buf->car)
#define SEL	(TAB.buf->sel)
#define NLINES	(TAB.buf->nlines)

enum mode_t {
	NORMAL_MODE,
	ISEARCH_MODE,
	FUZZY_SEARCH_MODE,
};

struct input_t {
	wchar_t	*text;
	int	length;
	int	cursor;
	bool	(*before_key)(struct input_t *, int c, bool alt, bool ctl, bool shift);
	void	(*after_key)(struct input_t *);
};

bool isearch_before_key(struct input_t *, int, bool, bool, bool);
void isearch_after_key(struct input_t *);
bool fuzzy_search_before_key(struct input_t *, int, bool, bool, bool);
void fuzzy_search_after_key(struct input_t *);

HWND		w;
HWND		dlg;
UINT		WM_FIND;
int		width;
int		height;
float		dpi = 96.0f;
BOOL		use_console = TRUE;
BOOL		transparent = FALSE;
#define		ID_CONSOLE 104
enum mode_t	mode;
struct input_t	isearch_input = { .before_key = isearch_before_key, .after_key = isearch_after_key };
struct input_t	fuzzy_search_input = { .before_key = fuzzy_search_before_key, .after_key = fuzzy_search_after_key };
struct input_t	*current_input;
wchar_t		**fuzzy_search_files;
wchar_t		**all_fuzzy_search_files;
unsigned	fuzzy_index;
unsigned	fuzzy_count;
Pg		*gs;
HDC		double_buffer_dc;
HBITMAP		double_buffer_bmp;
void		*double_buffer_data;
PgFont		*ui_font;
PgFont		*font[4];
PgFont		*backing_fonts[8];
int		status_bar_height = 24;
int		tab_bar_height = 24;
int		isearch_bar_height = 24;
int		additional_bars;
int		minimap_width = 128;
int		tab_width;
float		cursor_phase;
INT_PTR		cursor_timer;
int		last_cursor_line;
struct tab_t {
	Buf	*buf;
	Loc	click;
	int	top;
	float	magnification;
	int	ascender_height;
	int	line_height;
	int	em;
	int	tab_px_width;
	int	total_margin;
	int	max_line_width;
	BOOL	inhibit_auto_close;
	bool	scrolling;
	wchar_t	*filename;
	wchar_t	file_directory[512];
	wchar_t	file_basename[512];
	wchar_t	filename_extension[512];
	struct file file_settings;
	Scanner	brace[4];
} *tabs;
struct symbol_t {
	wchar_t *name;
	wchar_t *file_spec;
};
int		tab_count;
int		current_tab;
struct symbol_t *symbols;
int		symbol_count;

FINDREPLACE	fr = {
			sizeof fr, 0, 0,
			FR_DOWN|FR_DIALOGTERM
			|FR_HIDEMATCHCASE|FR_HIDEWHOLEWORD,
			0, 0, 1024, 1024, 0, 0, 0 };
WNDCLASSEX	wc = {
			sizeof wc,
			CS_VREDRAW|CS_HREDRAW|CS_DBLCLKS,
			0, 0, 0, 0, 0, 0, 0,
			0, L"Window", 0 };

static void recalculate_tab_width();
static void recalculate_text_metrics();
static void reserve_vertical_space(int amount);

static
px2line(int px) {
	return max(0, (px - tab_bar_height) / TAB.line_height) + top;
}

static
line2px(int ln) {
	return (ln-top)*TAB.line_height + tab_bar_height;
}

static
charwidth(unsigned c) {
	return pgGetCharWidth(font[0], c);
}

static
ind2px(int ln, int ind) {
	wchar_t	*txt;
	int	i,px,tab;

	txt=getb(TAB.buf, ln, 0);
	px=0;
	tab=TAB.tab_px_width;
	for (i=0; txt[i] && i<ind; i++)
		px += txt[i]=='\t'
			? tab - (px + tab) % tab
			: charwidth(txt[i]);
	return px + TAB.total_margin;
}

static
px2ind(int ln, int x) {
	wchar_t	*txt;
	int	i,px,tab;

	txt=getb(TAB.buf, ln, 0);
	px=0;
	x-=TAB.total_margin;
	tab=TAB.tab_px_width;
	for (i=0; txt[i] && px<x; i++)
		px += txt[i]=='\t'
			? tab - (px + tab) % tab
			: charwidth(txt[i]);
	return i;
}

static void*
makedlgitem(void *mem, DLGITEMTEMPLATE *it, int class, wchar_t *txt) {
	mem=(void*)((LONG_PTR)mem + 3 & ~3); /* align */
	memcpy(mem,it,sizeof *it);
	mem=(DLGITEMTEMPLATE*)mem + 1;
	*((WORD*)mem)++=0xffff;	/* class */
	*((WORD*)mem)++=class;	/* button */
	mem=wcscpy(mem,txt) + wcslen(txt)+1;
	*((WORD*)mem)++=0;	/* creation data */
	return mem;
}

typedef struct {
	DLGITEMTEMPLATE	dlg;
	DWORD		cls;
	wchar_t		*txt;
} DIALOG_ITEM;

HWND makedlg(HWND hwnd, DIALOG_ITEM *items, int n, wchar_t *title, DLGPROC proc) {
	static char	buf[8192];
	void		*mem;
	DIALOG_ITEM	*i;
	wchar_t		*font = L"Segoe UI";
	DLGTEMPLATE	dlg = { WS_SYSMENU|WS_VISIBLE|DS_SETFONT, 0,
				n, 0,0, 256+28+8,20+4+16+4+24};
	
	memcpy(buf, &dlg, sizeof dlg);
	mem=buf + sizeof dlg;
	*((WORD*)mem)++=0;	/* menu */
	*((WORD*)mem)++=0;	/* class */
	mem=wcscpy(mem,title) + wcslen(title)+1;
	*((WORD*)mem)++=9;	/* font size */
	mem=wcscpy(mem,font) + wcslen(font)+1;
	for (i=items; i < items+dlg.cdit; i++)
		mem=makedlgitem(mem, &i->dlg, i->cls, i->txt);
	
	return CreateDialogIndirect(GetModuleHandle(0),(void*)buf,
		hwnd,proc);
}

void switch_tab(int to_tab) {
	TAB.top = top;

	if (to_tab < 0) to_tab = 0;
	if (to_tab >= tab_count) to_tab = tab_count - 1;
	current_tab = to_tab;
	settitle(TAB.buf->changes);
	top = TAB.top;
	file = TAB.file_settings;
	reinitlang();
	recalculate_text_metrics();
	invdafter(1);
}
void next_tab() {
	switch_tab((current_tab + 1) % tab_count);
}
void previous_tab() {
	switch_tab(((unsigned)current_tab - 1) % tab_count);
}
int new_tab(Buf *b) {
	tabs = realloc(tabs, (tab_count + 1) * sizeof *tabs);
	tabs[tab_count] = (struct tab_t){
		.magnification = 1.00f,
		.buf = b,
		.filename = wcsdup(L""),
		.top = 1,
		.file_settings = file,
		.max_line_width = global.line_width,
	};
	current_tab = tab_count;
	reinitconfig();
	recalculate_tab_width();
	return tab_count++;
}
void close_tab() {
	struct tab_t tab = tabs[current_tab];
	
	free(tab.filename);
	freeb(tab.buf);
	free(tab.buf);
	
	for (int i = current_tab; i < tab_count - 1; i++)
		tabs[i] = tabs[i + 1];
	tab_count--;
	
	if (tab_count == 0) {
		act(ExitEditor);
		return;
	}
	switch_tab(current_tab);
	recalculate_tab_width();
}


void load_file_in_new_tab(wchar_t *filename_with_line_number);

static int compare_symbol_name(const void *a, const void *b) {
	return wcscmp(a, ((struct symbol_t*)b)[0].name);
}
static int go_to_symbol(wchar_t *name) {
	struct symbol_t *symbol = bsearch(name, symbols, symbol_count, sizeof *symbols, compare_symbol_name);
	if (symbol)
		load_file_in_new_tab(symbol->file_spec);
	return symbol != NULL;
}
static int compare_symbol(const void *a, const void *b) {
	return wcscmp(((struct symbol_t*)a)[0].name, ((struct symbol_t*)b)[0].name);
}

static void run_program(wchar_t *commandline) {
	STARTUPINFO startup_info = { sizeof startup_info, };
	PROCESS_INFORMATION process_info;
	bool ok = CreateProcess(NULL, commandline, NULL, NULL, FALSE, CREATE_NO_WINDOW,
		NULL, NULL, &startup_info, &process_info);
	if (ok) {
		WaitForSingleObject(process_info.hProcess, INFINITE);
		CloseHandle(process_info.hProcess);
		CloseHandle(process_info.hThread);
	}
}

static void reload_symbols(wchar_t *directory) {
	if (!directory)
		return;
	
	// Call ctags to generate symbols
//	wchar_t commandline[0x8000];
//	swprintf(commandline, 0x8000, L"cmd /c ctags -xR %ls >%ls/tags", directory, directory);
//	run_program(commandline);
//	
//	// Clear symbols
//	for (int i = 0; i < symbol_count; i++) {
//		free(symbols[i].name);
//		free(symbols[i].file_spec);
//	}
//	symbol_count = 0;
//	
//	// Load the tags file
//	wchar_t tags_filename[MAX_PATH];
//	wsprintf(tags_filename, L"%ls/tags", directory);
//	FILE *file = _wfopen(tags_filename, L"r");
//	if (file) {
//		char line[256];
//		wchar_t name[256];
//		char type[256];
//		int line_number;
//		wchar_t filename[256];
//		wchar_t file_spec[MAX_PATH * 2];
//		while (fgets(line, sizeof line, file)) {
//			if (4 != sscanf(line, "%ls\t%s\t%d\t%ls\t", name, type, &line_number, filename))
//				continue;
//			GetFullPathName(filename, MAX_PATH * 2, file_spec, NULL);
//			swprintf(file_spec, MAX_PATH * 2, L"%ls:%d", file_spec, line_number);
//			symbols = realloc(symbols, (symbol_count + 2) * sizeof *symbols);
//			symbols[symbol_count++] = (struct symbol_t){
//				.name = wcsdup(name),
//				.file_spec = platform_normalize_path(wcsdup(file_spec)),
//			};
//		}
//		symbols[symbol_count] = (struct symbol_t){0,};
//		if (symbol_count)
//			qsort(symbols, symbol_count, sizeof *symbols, compare_symbol);
//		fclose(file);
//		unlink("tags");
//	}
}


void new_file() {
	TAB.buf->changes=0;
	clearb(TAB.buf);
	defperfile();
	top=1;
	setfilename(L"");
	settitle(0);
}
void load_file(wchar_t *filename_with_line_number) {
	wchar_t *filename = wcsdup(filename_with_line_number);
	int line_number = separate_line_number(filename);
	platform_normalize_path(filename);
	
	TAB.buf->changes=0;
	clearb(TAB.buf);
	if (!load(TAB.buf, filename, L"utf-8"))
		MessageBox(w, L"Could not load", L"Error", MB_OK);
	setfilename(filename);
	free(filename);
	settitle(0);
	TAB.file_settings = file;
	reload_symbols(TAB.file_directory);
	reinitconfig();
	
	
	gob(TAB.buf, line_number, 0);
	act(MoveHome);
	invdafter(1);
}
void load_file_in_new_tab(wchar_t *filename_with_line_number) {
	wchar_t *filename = wcsdup(filename_with_line_number);
	int line_number = separate_line_number(filename);
	platform_normalize_path(filename);
	for (int i = 0; i < tab_count; i++)
		if (!wcsicmp(tabs[i].filename, filename)) {
			switch_tab(i);
			_act(TAB.buf, EndSelection);
			gob(TAB.buf, line_number, 0);
			_act(TAB.buf, MoveHome);
			invdafter(top);
			free(filename);
			return;
		}
	free(filename);
	switch_tab(new_tab(newb()));
	load_file(filename_with_line_number);
}
void save_file() {
	if (!save(TAB.buf, TAB.filename))
		MessageBox(w, L"Could not save", L"Error", MB_OK);
	TAB.buf->changes=0;
	settitle(0);
	reload_symbols(TAB.file_directory);
}


INT_PTR CALLBACK
SpawnProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	int	len;
	
	switch (msg) {
	case WM_INITDIALOG:
		dlg=hwnd;
		return TRUE;
	case WM_CLOSE:
		dlg=0;
		DestroyWindow(hwnd);
		return TRUE;
	case WM_COMMAND:
		switch (wparam) {
		case IDOK:
			len=GetWindowText(GetDlgItem(hwnd,100),
				lastcmd,
				sizeof lastcmd/sizeof *lastcmd);
			dlg=0;
			DestroyWindow(hwnd);
			act(SpawnCmd);
			return TRUE;
		case IDCANCEL:
			dlg=0;
			DestroyWindow(hwnd);
			return TRUE;
		case ID_CONSOLE:
			use_console = IsDlgButtonChecked(hwnd, ID_CONSOLE);
			return TRUE;
		}
	}
	return FALSE;
}

static
openspawn(HWND hwnd, wchar_t *initcmd) {
	DIALOG_ITEM	items[] = {
		{{WS_BORDER|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,0,
			48,4, 256-48-4,14, 100},
			0x81, initcmd},
		{{WS_VISIBLE,0,
			4,8, 44,16, 103},
			0x82, L"Command: "},
		{{WS_VISIBLE|BS_AUTOCHECKBOX|WS_TABSTOP,0,
			4,20, 96,16, ID_CONSOLE},
			0x80, L"Run with &console"},
		{{WS_VISIBLE|BS_DEFPUSHBUTTON|WS_TABSTOP,0,
			256,4, 28,16, IDOK},
			0x80, L"OK"},
		{{WS_VISIBLE|WS_TABSTOP,0,
			256,20+4, 28,16, IDCANCEL},
			0x80, L"Cancel"}
	};
	HWND dlgwnd = makedlg(hwnd, items, sizeof items/sizeof *items,
		L"Run", SpawnProc);
	CheckDlgButton(dlgwnd, ID_CONSOLE, use_console);
}

static
settitle(int mod) {
	wchar_t	all[MAX_PATH];
	swprintf(all, MAX_PATH, L"%ls%ls%ls%ls",
		mod? L"*": L"",
		TAB.file_basename,
		*TAB.filename_extension? L".": L"",
		TAB.filename_extension);
	SetWindowText(w, all);
	invdafter(1);
}

static
setfilename(wchar_t *fn) {
	wchar_t	*s=fn, *e;
	
	int length = GetFullPathName(fn, 0, NULL, NULL);
	TAB.filename = calloc(length + 1, sizeof *TAB.filename);
	GetFullPathName(fn, length, TAB.filename, NULL);
	
	platform_normalize_path(TAB.filename);
	
	if (e=wcsrchr(fn, L'/')) {
		wcsncpy(TAB.file_directory, fn, e-s);
		TAB.file_directory[e-s]=0;
		e++;
	} else {
		GetCurrentDirectory(512, TAB.file_directory);
		e=fn;
	}
	
	s=e;
	if ((e=wcsrchr(s, L'.')) && s!=e) {
		wcsncpy(TAB.file_basename, s, e-s);
		TAB.file_basename[e-s]=0;
		wcscpy(TAB.filename_extension, e+1);
	} else {
		wcscpy(TAB.file_basename, s);
		TAB.filename_extension[0]=0;
	}
	
	reinitlang();
}

alertchange(int mod) {
	settitle(mod);
	invdafter(1);
}

alertabort(wchar_t *msg, wchar_t *re) {
	wchar_t	buf[1024];
	swprintf(buf, 1024, L"error: %ls at /...%ls/",
		msg, re);
	MessageBox(w, buf, L"Expression Error", MB_OK);
}

static void stop_cursor_blink() {
        cursor_timer = 0;
        KillTimer(w, cursor_timer);
}
static void start_cursor_blink() {
        stop_cursor_blink();
        cursor_timer = SetTimer(w, 0, 1000 / global.cursor_fps, NULL);
}


static
snap() {
	if (LN < top)
		top=sat(1, LN-1, NLINES);
	else if (BOT <= LN)
		top=sat(1, LN-vis+1, NLINES-vis+1);
	else return 0;
	invdafter(top);
	return 0;
}

/*
 * Invalidate the given lines and update the display.
 * This comes packaged with snapping to the caret, so if
 * you don't want that, you should call InvalidateRect().
 */
static
invd(int lo, int hi) {
	RECT	rt;
	
	snap();
	if (mode != NORMAL_MODE) { // line numbers don't mean anything in other modes
		InvalidateRect(w, NULL, 0);
	} else {
		rt.top=line2px(lo);
		rt.bottom=line2px(hi+1);
		rt.left=0;
		rt.right=width;
		InvalidateRect(w, &rt, 0);
	}
}

static
invdafter(int lo) {
	invd(lo, BOT);
}

static
commitlatch() {
	wchar_t	*txt;
	HANDLE handle;
	if (!latch)
		return 0;

	handle=GlobalAlloc(GMEM_MOVEABLE, (wcslen(latch)+1)*sizeof (wchar_t));
	txt=GlobalLock(handle);
	wcscpy(txt, latch);
	free(latch);
	latch=0;
	GlobalUnlock(handle);
	
	OpenClipboard(w);
	EmptyClipboard();
	SetClipboardData(CF_UNICODETEXT, txt);
	CloseClipboard();
	return 1;
}

generalinvd(int onlines, int wassel, Loc *olo, Loc *ohi) {
	int	selnow;
	Loc	lo,hi;
	
	selnow=ordersel(TAB.buf, &lo, &hi);
	if (onlines != NLINES)
		invdafter(top);
	else if (wassel != selnow && (wassel||selnow))
		invdafter(top);
	else if (selnow && !samerange(olo, ohi, &lo, &hi))
		invd(min(olo->ln, lo.ln), max(ohi->ln, hi.ln));
}
spawn_cmd() {
	STARTUPINFO		si;
	PROCESS_INFORMATION	pi;
	wchar_t wrap[MAX_PATH*3];
	
	ZeroMemory(&si, sizeof si);
	ZeroMemory(&pi, sizeof pi);
	si.cb=sizeof si;
	
	if (use_console)
		swprintf(wrap,
			sizeof wrap/sizeof *wrap,
			lang.cmdwrapper,
			lastcmd,
			TAB.filename);
	else
		wcscpy(wrap, lastcmd);
		
	if (CreateProcess(0,wrap, 0,0,0,0,0,0,&si, &pi)) {
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
}
void highlight_brace() {
	TAB.brace[2] = TAB.brace[0];
	TAB.brace[3] = TAB.brace[1];
	TAB.brace[0] = (Scanner){0};
	TAB.brace[1] = (Scanner){0};
	if (!global.match_braces) return;
	Scanner start = getscanner(TAB.buf, LN, IND);
	Scanner other = matchbrace(start, true, true);
	if (other.c) {
		TAB.brace[0] = start;
		TAB.brace[1] = other;
	}
	for (int i = 0; i < 4; i++)
		invd(TAB.brace[i].ln, TAB.brace[i].ln);
}
void adjust_line_width(struct tab_t *tab) {
	int new = 0;
	for (int i = 1; i <= NLINES; i++)
		new = max(new, lenb(tab->buf, i));
	if (new > tab->max_line_width) {
		tab->max_line_width = new + 10;
		recalculate_text_metrics();
		invdafter(top);
	} else if (new < tab->max_line_width - 10) {
		tab->max_line_width = max(new, global.line_width);
		recalculate_text_metrics();
		invdafter(top);
	} 
}

act(int action) {
	
	STARTUPINFO		si;
	PROCESS_INFORMATION	pi;
	int	ok, wassel, onlines,oldconf;
	int	ln, ind;
	Loc	lo,hi;
	DWORD	sz;
	wchar_t	*txt;
	
	onlines=NLINES;
	wassel=ordersel(TAB.buf, &lo, &hi);
	
	switch (action) {
	
	case PasteClipboard:
		if (!OpenClipboard(w))
			return 0;
		latch = GetClipboardData(CF_UNICODETEXT);
		break;
		
	case ReloadConfig:
	case PrevConfig:
	case NextConfig:
		oldconf=curconf;
		break;
	}
	
	ok = _act(TAB.buf, action);
	
	switch (action) {
	
	case ExitEditor:
		SendMessage(w, WM_CLOSE,0,0);
		break;
	
	case SpawnEditor:
		ZeroMemory(&si, sizeof si);
		ZeroMemory(&pi, sizeof pi);
		si.cb=sizeof si;
		txt=malloc(MAX_PATH*sizeof(wchar_t));
		GetModuleFileName(0, txt, MAX_PATH);
		if (CreateProcess(txt,0, 0,0,0,0,0,0,&si, &pi)) {
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
		free(txt);
		break;
	
	case SpawnShell:
		ZeroMemory(&si, sizeof si);
		ZeroMemory(&pi, sizeof pi);
		si.cb=sizeof si;
		if (CreateProcess(0,global.shell, 0,0,0,0,0,0,&si, &pi)) {
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
		break;
		
	case SpawnCmd:
		spawn_cmd();
		break;
	
	case PromptSpawn:
		ok=openspawn(w, lastcmd);
		break;
	
	case DeleteLine:
	case JoinLine:
	case Duplicate:
	case BreakLine:
		/* Line monitor will handle */
		break;
	
	case SelectAll:
	case StartSelection:
	case IndentSelection:
	case UnindentSelection:
	case EndSelection:
		/* The selection monitor handles updates */
		break;
	case CommentSelection:
		if (!wassel)
			invd(LN, LN);
		/* The selection monitor handles updates */
		break;
	
	case MoveLeft:
	case MoveRight:
	case MoveWordLeft:
	case MoveWordRight:
	case MoveHome:
	case MoveEnd:
		invdafter(BOT);
		snap();
		break;
	case MoveUp:
	case MoveDown:
	case MovePageUp:
	case MovePageDown:
	case MoveSof:
	case MoveEof:
		invdafter(top);
		snap();
		break;
	
	case DeleteChar:
	case BackspaceChar:
	case DeleteSelection:
		/* If the update was simple, invalidate the
		 * line; if it was not, the line or selection
		 * monitor will handle it
		 */
		invd(LN, LN);
		break;
	
	case AscendLine:
		if (ok)
			invd(LN, LN+1);
		break;
		
	case DescendLine:
		if (ok)
			invd(LN-1, LN);
		break;
	
	case CutSelection:
		commitlatch();
		break;

	case CopySelection:
		commitlatch();
		break;
		
	case PasteClipboard:
		latch=0;
		CloseClipboard();
		invd(LN, LN);
		break;
	
	case UndoChange:
	case RedoChange:
		if (ok)
			invdafter(top);
		break;
	
	case PromptFind:
	case PromptReplace:
		if (dlg)
			break;
		fr.Flags &= ~FR_DIALOGTERM;
		if (SLN) {
			wchar_t *tmp = copysel(TAB.buf);
			wcsncpy(fr.lpstrFindWhat,tmp,MAX_PATH);
			fr.lpstrFindWhat[MAX_PATH] = 0;
			free(tmp);
		}
		dlg = action==PromptFind
			? FindText(&fr)
			: ReplaceText(&fr);
		break;
	
	case ReloadConfig:
	case PrevConfig:
	case NextConfig:
		if (ok) {
			if (action==ReloadConfig)
				selectconfig(oldconf);
			reinitconfig();
			invdafter(top);
		}
		break;
	case EditConfig:
		load_file_in_new_tab(configfile);
		break;
	case ToggleTransparency:
		transparent = !transparent;
		SetLayeredWindowAttributes(w, 0, 255*(transparent?global.alpha:1), LWA_ALPHA);
		break;
	case ToggleMinimap:
		global.minimap = !global.minimap;
		invdafter(top);
		break;
	case RaiseFontMagnification:
		TAB.magnification += 0.05f;
		configfont();
		invdafter(top);
		break;
	case LowerFontMagnification:
		if (TAB.magnification > 0.05f * 2.0f) TAB.magnification -= 0.05f;
		configfont();
		invdafter(top);
		break;
	case ResetFontMagnification:
		TAB.magnification = 1.00f;
		configfont();
		invdafter(top);
		break;
	case NewTab:
		switch_tab(new_tab(newb()));
		break;
	case NextTab:
		next_tab();
		break;
	case PreviousTab:
		previous_tab();
		break;
	case CloseTab:
		close_tab();
		break;
	default:
		invdafter(top);
	}
	
	highlight_brace(TAB);
	adjust_line_width(&TAB);
	generalinvd(onlines, wassel, &lo, &hi);
	return ok;
}

actins(int c) {
	int	ok, wassel, onlines;
	Loc	lo,hi;
	wchar_t	*txt, *brace;
	
	onlines=NLINES;
	wassel=ordersel(TAB.buf, &lo, &hi);
	
	txt = getb(TAB.buf, LN, 0);
	brace = wcschr(lang.brace, c);
	if (brace && lang.autoClose && !TAB.inhibit_auto_close) {
		BOOL closing = brace - lang.brace & 1;
		
		if (!closing && ordersel(TAB.buf, &lo, &hi)) {
			act(EndSelection);
			gob(TAB.buf, lo.ln, lo.ind);
			_actins(TAB.buf, c);
			gob(TAB.buf, hi.ln, hi.ind+1);
			_actins(TAB.buf, brace[1]);
		} else if (lang.typeover && txt[IND] == c)
			act(MoveRight);
		else if (closing)
			_actins(TAB.buf, c);
		else {
			_actins(TAB.buf, c);
			_actins(TAB.buf, brace[1]);
			act(MoveLeft);
		}
	} else if (lang.typeover && brktbl[c & 0xffff] && txt[IND] == c && !iswspace(c))
		act(MoveRight);
	else _actins(TAB.buf, c);

	highlight_brace(TAB);
	adjust_line_width(&TAB);
	invd(LN, LN);
	generalinvd(onlines, wassel, &lo, &hi);
	return 1;
}

actquery(wchar_t *query, int down, int sens) {
	wchar_t	title[2];
	if (!_actquery(TAB.buf, query, down, sens))
		return 0;
	GetWindowText(dlg, title, 2);
	if (dlg && title[0]=='F')
		SendMessage(dlg, WM_CLOSE, 0, 0);
	invdafter(top);
	return 1;
}

actreplace(wchar_t *query, wchar_t *repl, int down, int sens) {
	if (!_actreplace(TAB.buf, query, repl, down, sens))
		return 0;
	invdafter(top);
	return 1;
}

actreplaceall(wchar_t *query, wchar_t *repl, int down, int sens) {
	int	n;
	if (dlg)
		SendMessage(dlg, WM_CLOSE, 0, 0);
	n=_actreplaceall(TAB.buf, query, repl, down, sens);
	if (!n)
		return 0;
	invdafter(top);
	return n;
}

autoquery() {
	return actquery(fr.lpstrFindWhat,
		fr.Flags & FR_DOWN,
		fr.Flags & FR_MATCHCASE);
}

autoreplace() {
	return actreplace(fr.lpstrFindWhat,
		fr.lpstrReplaceWith,
		fr.Flags & FR_DOWN,
		fr.Flags & FR_MATCHCASE);
}

autoreplaceall() {
	return actreplaceall(fr.lpstrFindWhat,
		fr.lpstrReplaceWith,
		fr.Flags & FR_DOWN,
		fr.Flags & FR_MATCHCASE);
}

void start_isearch() {
	mode = ISEARCH_MODE;
	current_input = &isearch_input;
	if (current_input->text)
		free(current_input->text);
	current_input->text = SLN ? copysel(TAB.buf) : wcsdup(L"");
	current_input->length = wcslen(current_input->text);
	current_input->cursor = current_input->length;
	reserve_vertical_space(isearch_bar_height);
	act(EndSelection);
	invdafter(top);
}
void isearch_next_result() {
	actisearch(TAB.buf, isearch_input.text, true, true);
	invdafter(top);
}
bool isearch_before_key(struct input_t *input, int c, bool alt, bool ctl, bool shift) {
	if (c == '\r') {
		isearch_next_result();
		return false;
	} else if (c == 27) {
		reserve_vertical_space(-isearch_bar_height);
		invdafter(top);
		mode = NORMAL_MODE;
		return false;
	}
	return true;
}
void isearch_after_key(struct input_t *input) {
	actisearch(TAB.buf, input->text, true, false);
	invdafter(top);
}

int separate_line_number(wchar_t *filename) {
	wchar_t *line_part = wcsrchr(filename, L':');
	wchar_t *end;
	bool had_line_number = line_part && (line_part[1] == 0 || iswdigit(line_part[1]));
	int line_number = had_line_number ? wcstoul(line_part + 1, &end, 10) : 0;
	if (!had_line_number) return 0;
	*line_part = 0;
	return line_number;
}

void filter_fuzzy_search_list(wchar_t **out, wchar_t **in, wchar_t *request) {
	if (!in || !out) return;
	
	request = wcsdup(request);
	separate_line_number(request);
	*out = NULL;
	int count = 0;
	for ( ; *in; in++)
		if (!*request || wcsistr(*in, request)) {
			count++;
			*out++ = *in +
				(wcsstr(*in, TAB.file_directory) == 0 ?
					wcslen(TAB.file_directory) + 1 :
					0);
		}
	*out = NULL;
	free(request);
	fuzzy_count = count;
	fuzzy_index = fuzzy_count ? fuzzy_index % fuzzy_count : 0;
}
void start_fuzzy_search(wchar_t *initial_text) {
	stop_cursor_blink();
	mode = FUZZY_SEARCH_MODE;
	current_input = &fuzzy_search_input;
	if (current_input->text)
		free(current_input->text);
	current_input->text = wcsdup(initial_text);
	current_input->length = wcslen(current_input->text);
	current_input->cursor = current_input->length;
	
	if (all_fuzzy_search_files) {
		for (wchar_t **p = all_fuzzy_search_files; *p; p++) free(*p);
		free(all_fuzzy_search_files);
		all_fuzzy_search_files = NULL;
	}
	if (fuzzy_search_files) {
		free(fuzzy_search_files);
		fuzzy_search_files = NULL;
	}
	int count;
	all_fuzzy_search_files = platform_list_directory(TAB.file_directory, &count);
	fuzzy_search_files = calloc(count + 1, sizeof *fuzzy_search_files);
	filter_fuzzy_search_list(fuzzy_search_files, all_fuzzy_search_files, fuzzy_search_input.text);
	
	act(EndSelection);
	invdafter(top);
}
bool fuzzy_search_before_key(struct input_t *input, int c, bool alt, bool ctl, bool shift) {
	if (c == '\r') {
		mode = NORMAL_MODE;
		if (*input->text == ':') {
			int line_number = separate_line_number(input->text);
			if (line_number) {
				gob(TAB.buf, line_number, 0);
				act(MoveHome);
			}
		} else if (fuzzy_search_files[0]) {
			wchar_t spec[MAX_PATH + 1];
			swprintf(spec, MAX_PATH + 1, L"%ls:%d",
				fuzzy_search_files[fuzzy_index],
				separate_line_number(input->text));
			load_file_in_new_tab(spec);
		} else
			load_file_in_new_tab(input->text);
		start_cursor_blink();
		invdafter(top);
		return false;
	} else if (c == 27) { // Escape
		start_cursor_blink();
		invdafter(top);
		mode = NORMAL_MODE;
		return false;
	}
	return true;
}
void fuzzy_search_after_key(struct input_t *input) {
	filter_fuzzy_search_list(fuzzy_search_files, all_fuzzy_search_files, input->text);
	invdafter(top);
}

void input_insert(int c) {
	current_input->text = realloc(current_input->text,
		(current_input->length + 2) * sizeof *current_input->text);
	wmemmove(current_input->text + current_input->cursor + 1,
		current_input->text + current_input->cursor,
		current_input->length - current_input->cursor);
	current_input->text[current_input->cursor++] = c;
	current_input->text[++current_input->length] = 0;
	current_input->after_key(current_input);
}
void input_delete() {
	if (current_input->cursor == current_input->length)
		return;
	wmemmove(current_input->text + current_input->cursor,
		current_input->text + current_input->cursor + 1,
		current_input->length - current_input->cursor);
	current_input->text[--current_input->length] = 0;
	current_input->after_key(current_input);
}
void input_home() {
	current_input->cursor = 0;
	invdafter(top);
}
void input_end() {
	current_input->cursor = current_input->length;
	invdafter(top);
}
void input_move(int dir) {
	if (dir > 0 && dir + current_input->cursor <= current_input->length)
		current_input->cursor += dir;
	if (dir < 0 && dir + current_input->cursor >= 0)
		current_input->cursor += dir;
	invdafter(top);
}

int wmchar_input(int c, bool alt, bool ctl, bool shift) {
	if (!current_input->before_key(current_input, c, alt, ctl, shift))
		return 1;
	
	if (c >= 0x20 && current_input->length < MAX_PATH)
		input_insert(c);
	else if (c == 8 && current_input->cursor > 0) { // backspace
		input_move(-1);
		input_delete();
	} else if (c == 22) { // ^V
		wchar_t *text;
		if (!OpenClipboard(w))
			return 0;
		if (text = GetClipboardData(CF_UNICODETEXT))
			while (*text)
				input_insert(*text++);
		CloseClipboard();
	}
	return 1;
}

int wmkey_input(int c, bool alt, bool ctl, bool shift) {
	switch (c) {
	case VK_DELETE:
		input_delete();
		break;
	case VK_LEFT:
		if (alt) input_home(); else input_move(-1);
		break;
	case VK_RIGHT:
		if (alt) input_end(); else input_move(1);
		break;
	case VK_UP:
		if (mode == FUZZY_SEARCH_MODE) {
			fuzzy_index = fuzzy_index ? fuzzy_index - 1 : max(fuzzy_count - 1, 0);
			invdafter(top);
		}
		break;
	case VK_DOWN:
		if (mode == FUZZY_SEARCH_MODE) {
			fuzzy_index = fuzzy_index < fuzzy_count ? fuzzy_index + 1 : 0;
			invdafter(top);
		}
		break;
	}
	return 1;
}

int wmsyskeydown(int c) {
		
	int	ctl = GetAsyncKeyState(VK_CONTROL) & 0x8000;
	int	shift = GetAsyncKeyState(VK_SHIFT) & 0x8000;
		
	if (mode == ISEARCH_MODE || mode == FUZZY_SEARCH_MODE)
		return !wmkey_input(c, true, ctl, shift);
	
	switch (c) {
	case 13: // Alt + Return
		act(shift ? SpaceAbove : SpaceBelow);
		return true;
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case '0':
		switch_tab(c == '0' ? 9 : c - '1');
		return true;
	case 'A':
		act(SelectAll);
		break;
	case 'C':
		return act(CopySelection);
	case 'D':
		if (shift)
			act(Duplicate);
		else
			return 0;
		break;
	case 'F':
		if (ctl && shift)
			return act(PromptFind);
		start_isearch();
		break;
	case 'J':
		act(JoinLine);
		return true;
	case 'M':
		act(ToggleMinimap);
		return true;
	case 'N':
		act(NewTab);
		break;
	case 'P':
		start_fuzzy_search(L"");
		break;
	case 'R':
		if (!SLN)
			_act(TAB.buf, SelectWord);
		if (SLN) {
			wchar_t *name = copysel(TAB.buf);
			go_to_symbol(name);
			free(name);
		}
		break;
	case 'S':
		save_file();
		break;
	case 'T':
		act(NewTab);
		break;
	case 'V':
		act(PasteClipboard);
		break;
	case 'W':
		act(CloseTab);
		break;
	case 'X':
		act(CutSelection);
		break;
	case 'Y':
		act(RedoChange);
		break;
	case 'Z':
		act(UndoChange);
		break;
	case 187: // Alt + + and Alt + =
		if (!ctl) act(shift ? ResetFontMagnification : RaiseFontMagnification);
		break;
	case 189: // Alt + -
		act(LowerFontMagnification);
		break;
	case 191: // Alt + /
		act(CommentSelection);
		break;
	case VK_UP:
		setsel(shift);
		act(MoveSof);
		break;
	case VK_DOWN:
		setsel(shift);
		act(MoveEof);
		break;
	case VK_LEFT:
		setsel(shift);
		act(MoveHome);
		break;
	case VK_RIGHT:
		setsel(shift);
		act(MoveEnd);
		break;
	case VK_F12:
		act(EditConfig);
		break;
	default: return 0;
	}
	return 1;
}
wmchar(int c) {
	
	int	ctl=GetAsyncKeyState(VK_CONTROL) & 0x8000;
	int	shift=GetAsyncKeyState(VK_SHIFT) & 0x8000;
	
	if (mode == ISEARCH_MODE || mode == FUZZY_SEARCH_MODE)
		return wmchar_input(c, false, ctl, shift);
	switch (c) {
	
	case 1: /* ^A */
		return act(MoveHome);
	
	case 2: /* ^B */
		setsel(shift);
		return act(MoveBrace);
	
	case 3: /* ^C */
		return act(CopySelection);
	
	case 4: /* ^D */
		if (shift)
			return act(Duplicate);
		else
			return act(DeleteChar);
		
	case 5: /* ^E */
		return act(MoveEnd);
	
	case 6: /* ^F */
		if (ctl && shift)
			return act(PromptFind);
		start_isearch();
		return 1;
	
	case 7: /* ^G */
		start_fuzzy_search(L":");
		return 0;
	
	case 8: /* ^H Bksp */
		return act(BackspaceChar);
	
	case 9: /* ^I Tab */
		if (!SLN)
			goto normal;
		if (shift)
			return act(UnindentSelection);
		return act(IndentSelection);
	
	case 10: /* ^J */
		return act(JoinLine);

	case 11: /* ^K */
		return shift ? act(DeleteLine) : act(ClearRight);
	
	case 12: /* ^L */
		return 0;
	
	case 13: /* ^M Enter */
		if (shift)
			return act(SpaceBelow);
		return act(BreakLine);
	
	case 14: /* ^N */
		return act(NewTab);
	
	case 15: // ^O
		return false;
	
	case 16: /* ^P */
		return false;
	
	case 17: /* ^Q */
		return act(CommentSelection);
	
	case 18: /* ^R */
		if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
			return autoreplace();
		return act(PromptReplace);
		
	case 19: /* ^S */
		save_file();
		return true;
	
	case 20: /* ^T */
		return act(NewTab);
	
	case 21: /* ^U */
		return act(ClearLeft);
	
	case 22: /* ^V */
		return act(PasteClipboard);
	
	case 23: /* ^W */
		return act(CloseTab);
	
	case 24: /* ^X */
		return act(CutSelection);
	
	case 25: /* ^Y */
		return act(RedoChange);
		
	case 26: /* ^Z */
		return act(UndoChange);
	
	case 27: /* ^[ Esc */
		return 0;
	
	case 127: /* ^Bksp */
		act(StartSelection);
		act(MoveWordLeft);
		act(DeleteSelection);
		return 0;
	
	case ' ':
		if (ctl)
			return act(SelectWord);
		else
			goto normal;
	default:
normal:
		return actins(c);
	}
}

setsel(int yes) {
	if (yes)
		return act(StartSelection);
	if (SLN) return act(EndSelection);
	return 0;
}

int wmkey(int c) {

	int	ctl=GetAsyncKeyState(VK_CONTROL) & 0x8000;
	int	shift=GetAsyncKeyState(VK_SHIFT) & 0x8000;
	int	ok;
	
	if (mode == ISEARCH_MODE || mode == FUZZY_SEARCH_MODE)
		return wmkey_input(c, false, ctl, shift);

	switch (c) {
	
	case '\t':
		if (ctl)
			return act(shift ? PreviousTab : NextTab);
		break;
	
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case '0':
		if (ctl) { switch_tab(c == '0' ? 9 :c - '1'); return true; }
		break;
		
	
	case VK_UP:
		setsel(shift);
		return act(ctl? AscendLine: MoveUp);
	
	case VK_DOWN:
		setsel(shift);
		return act(ctl? DescendLine: MoveDown);
	
	case VK_LEFT:
		setsel(shift);
		return act(ctl? MoveWordLeft: MoveLeft);

	case VK_RIGHT:
		setsel(shift);
		return act(ctl? MoveWordRight: MoveRight);
	
	case VK_HOME:
		setsel(shift);
		return act(ctl? MoveSof: MoveHome);
	
	case VK_END:
		setsel(shift);
		return act(ctl? MoveEof: MoveEnd);
	
	case VK_PRIOR:
		setsel(shift);
		return act(MovePageUp);
	
	case VK_NEXT:
		setsel(shift);
		return act(MovePageDown);
	
	case VK_INSERT:
		if (ctl)
			return act(CopySelection);
		else if (shift)
			return act(PasteClipboard);
		return act(ToggleOverwrite);
		
	case VK_DELETE:
		if (ctl) {
			act(StartSelection);
			act(MoveWordRight);
			act(DeleteSelection);
			return 0;
		}
		return act(shift? CutSelection: DeleteChar);
	
	case VK_F2:
		return act(ctl? SpawnShell: SpawnEditor);
		
	case VK_F3:
		act(EndSelection);
		isearch_next_result(&isearch_input);
		return 1;
	
	case VK_F5:
		load_file(TAB.filename);
		return 1;
		
	case VK_F7:
		if (ctl)
			return act(PromptSpawn);
		else
			return act(SpawnCmd);
	
	case VK_F9:
		if (shift)
			return act(PrevBookmark);
		else if (ctl)
			return act(ToggleBookmark);
		else
			return act(NextBookmark);
	
	case VK_F11:
		return act(ToggleTransparency);
	case VK_F12:
		if (ctl)
			return act(ReloadConfig);
		return act(shift? PrevConfig: NextConfig);
	}
	return 0;
}

paintsel() {
	if (GetFocus() == w) {
		float p0 = 0.10f, p1 = 1.0f, p2 = 1.0f, p3 = 1.0f, p4 = 1.0f, p5 = 0.10f;
	
		float q = TAB.line_height * 0.5f *
			(pow(1.0f - cursor_phase, 5) * p0 +
			5 * cursor_phase * pow(1.0f - cursor_phase, 4) * p1 +
			10 * pow(cursor_phase, 2.0f) * pow(1.0f - cursor_phase, 3) * p2 +
			10 * pow(cursor_phase, 3.0f) * pow(1.0f - cursor_phase, 2) * p3 +
			5 * pow(cursor_phase, 4.0f) * pow(1.0f - cursor_phase, 2) * p4 +
			1 * pow(cursor_phase, 5.0f) * pow(1.0f - cursor_phase, 1) * p5);
		
		PgPt pt = pgPt(ind2px(LN, IND), line2px(LN) + TAB.line_height * 0.5f);
		pgClearSection(gs,
			pgAddPt(pt, pgPt(0.0f, -q)),
			pgAddPt(pt, pgPt(TAB.em * (overwrite ? global.cursor_overwrite_width : global.cursor_insert_width), q)),
			conf.fg);
	}

	if (!SLN)
		return false;
	
	Loc lo, hi;
	ordersel(TAB.buf, &lo, &hi);
	if (lo.ln != hi.ln) {
		int first_line_left = ind2px(lo.ln, lo.ind);
		int first_line_top = line2px(lo.ln);
		int first_line_bottom = first_line_top + TAB.line_height;
		int last_line_top = line2px(hi.ln);
		int last_line_bottom = last_line_top + TAB.line_height;
		int last_line_right = ind2px(hi.ln, hi.ind);
		
		PgRect r0 = pgRect(pgPt(first_line_left, first_line_top), pgPt(width, first_line_bottom + 0.5f));
		PgRect r1 = pgRect(pgPt(TAB.total_margin, first_line_bottom), pgPt(width, last_line_top + 0.5f));
		PgRect r2 = pgRect(pgPt(TAB.total_margin, last_line_top), pgPt(last_line_right, last_line_bottom));
		
		pgClearSection(gs, r0.a, r0.b, conf.selbg);
		pgClearSection(gs, r1.a, r1.b, conf.selbg);
		pgClearSection(gs, r2.a, r2.b, conf.selbg);
	} else
		pgClearSection(gs,
			pgPt(ind2px(lo.ln, lo.ind), line2px(lo.ln)),
			pgPt(ind2px(hi.ln, hi.ind), line2px(hi.ln) + TAB.line_height),
			conf.selbg);

	return true;
}

void blurtext(Pg *gs, int style, int x, int y, wchar_t *txt, int n, uint32_t fg) {
	int fontno      = style % 4;
	bool underline  = style & UNDERLINE_STYLE;
	bool caps       = style & (ALL_CAPS_STYLE | SMALL_CAPS_STYLE);
	bool small_caps = style & SMALL_CAPS_STYLE;
	wchar_t *p;
	wchar_t *end = txt + n;
	int margin = TAB.total_margin;
	int faux_bold = (fontno & 1) && pgGetFontWeight(font[fontno]) < 600;
	
	
	PgPt at = pgPt(x, y);
	float underline_y = at.y + pgGetFontAscender(font[fontno]) +
		1.5f * pgGetFontHeight(font[fontno]) / 10.0f;
	
	for (p = txt; p < end; p++) {
		if (*p == '\t')
			at.x += TAB.tab_px_width - fmod(at.x - margin + TAB.tab_px_width, TAB.tab_px_width);
		else {
			PgFont *cur_font = font[fontno];
			int c = caps ? towupper(*p) : *p;
			float push = 0;
			float start_x = at.x;
			
			if (!pgGetGlyph(cur_font, c))
				for (int i = 0; i < conf.nbacking_fonts; i++)
					if (backing_fonts[i] && pgGetGlyph(backing_fonts[i], c)) {
						cur_font = backing_fonts[i];
						break;
					}
			float ctm_d = cur_font->ctm.d;
			
			if (small_caps && c != *p) {
				float x_height = pgGetFontCapHeight(cur_font);
				push = pgGetFontAscender(cur_font) - x_height;
				cur_font->ctm.d *= x_height / pgGetFontAscender(cur_font);
			}
			
			if (faux_bold)
				pgFillChar(gs, cur_font, at.x + 1, at.y + push, c, fg);
			at.x = (int)pgFillChar(gs, cur_font, at.x, at.y + push, c, fg);
			if (underline)
				pgStrokeLine(gs,
					pgPt(start_x, underline_y),
					pgPt(at.x, underline_y),
					pgGetFontHeight(cur_font) / 10.0f, fg);
			cur_font->ctm.d = ctm_d;
		}
	}
}

paintstatus() {
	wchar_t	buf[1024];
	wchar_t *selmsg=L"%ls %d:%d of %d Sel %d:%d (%d %ls)";
	wchar_t *noselmsg=L"%ls %d:%d of %d";
	int	len;
	
	float top = height - status_bar_height;

	len=swprintf(buf, 1024, SLN? selmsg: noselmsg,
		TAB.filename,
		LN, ind2col(TAB.buf, LN, IND),
		NLINES,
		SLN,
		ind2col(TAB.buf, SLN, SIND),
		SLN==LN? abs(SIND-IND): abs(SLN-LN)+1,
		SLN==LN? L"chars": L"lines");
	
	pgScaleFont(ui_font, global.ui_font_small_size * dpi / 72.0f, 0.0f);
	float x = 4;
	float y = top + status_bar_height / 2.0f - pgGetFontEm(ui_font) / 2.0f;
	pgClearSection(gs, pgPt(0, top), pgPt(width, height), conf.chrome_bg);
	pgFillString(gs, ui_font, x, y, buf, len, conf.chrome_fg);
}

#include "re.h"

paintline(Pg *gs, int x, int y, int line) {
	int	k,len,sect;
	void	*txt = getb(TAB.buf,line,&len);
	unsigned short *i = txt, *j = txt, *end = i + len;
	SIZE	size;
	
	if (mode == ISEARCH_MODE) {
		wchar_t *i = txt;
		for (i = txt; *i && (i = wcsistr(i, isearch_input.text)); i += current_input->length)
			pgClearSection(gs,
				pgPt(ind2px(line, i - txt), y - (TAB.line_height - TAB.ascender_height)/2),
				pgPt(ind2px(line, i - txt + current_input->length), y - (TAB.line_height - TAB.ascender_height) / 2.0f + TAB.line_height),
				conf.isearchbg);
	}
	
	while (j<end) {
		/* Match a keyword  */
		for (k=0,sect=0; k<lang.nkwd; k++) {
			wchar_t *comp=lang.kwd_comp[k];
			unsigned options=lang.kwd_opt[k];
			sect=re_run(j, comp, options);
			
			if (sect > 0) /* matched */
				break;
		}
	
		if (sect>0) {
			int style=conf.style[lang.kwd_color[k]].style;
			
			/* Draw the preceding section */
			blurtext(gs, conf.default_style, x, y, i, j-i, conf.fg);
			x=ind2px(line, j-txt);
			
			/* Then draw the keyword */
			blurtext(gs, style, x,y, j, sect, conf.style[lang.kwd_color[k]].color);
			i=j+=sect;
			x=ind2px(line,j-txt);
		} else  if (*j && brktbl[*j] != 1) { /* Skip spaces or word */
			int kind = brktbl[*j];
			while (*j && brktbl[*j] == kind) j++;
		} else /* Skip one breaker */
			j++;
	}
	if (j>i)
		blurtext(gs, conf.default_style, x,y, i, j-i, conf.fg);

	for (int i = 0; i < 2; i++)
		if (line == TAB.brace[i].ln && TAB.brace[i].c)
			blurtext(gs, BOLD_STYLE, ind2px(TAB.brace[i].ln, TAB.brace[i].ind),
				line2px(TAB.brace[i].ln) + (TAB.line_height-TAB.ascender_height)/2,
				&TAB.brace[i].c, 1, conf.brace_fg);
}

paintlines(Pg *gs, int first, int last) {
	int	line, _y=line2px(first);
	
	for (line=first; line<=last && line <= BOT; line++, _y += TAB.line_height)
		paintline(gs, TAB.total_margin,
			_y + (TAB.line_height-TAB.ascender_height)/2, line);
}

void paint_minimap(Pg *full_canvas) {
	PgPt a = pgPt(width - minimap_width, tab_bar_height + 3.0f);
	PgPt b = pgPt(width - 3.0f, height - additional_bars);
	Pg *gs = pgSubsectionCanvas(full_canvas, pgRect(a, b));
	float each_line = min(1.0f, (float)gs->height / NLINES);
	float y = 0.0f;
	float longest_line = 1;
	for (int i = 1; i <= NLINES; i++)
		longest_line = max(longest_line, lenb(TAB.buf, i));
	pgClearSection(gs,
		pgPt(0.0f, each_line * top),
		pgPt(gs->width, each_line * BOT),
		conf.selbg);
	pgStrokeRect(gs,
		pgPt(0.0f, each_line * top),
		pgPt(gs->width, each_line * BOT),
		3.0f,
		conf.fg);
	for (int i = 1; i <= NLINES; i++, y += each_line)
		pgStrokeLine(gs,
			pgPt(0, y),
			pgPt(lenb(TAB.buf, i) / longest_line * gs->width, y),
			each_line * 0.5f,
			conf.fg);
	pgFreeCanvas(gs);
}

void paint_normal_mode(PAINTSTRUCT *ps) {
	int	i,n,y,x,len,first,last;
	
	first = px2line(ps->rcPaint.top);
	last = px2line(ps->rcPaint.bottom);
	
	/* Clear the background */
	pgClearSection(gs,
		pgPt(ps->rcPaint.left-1, ps->rcPaint.top-1),
		pgPt(ps->rcPaint.right+1, ps->rcPaint.bottom+1),
		conf.bg);
	
	/* Draw odd line's background */
	if (conf.bg2 != conf.bg) {
		y=line2px(first);
		for (i=first; i<=last; i++) {
			if (i % 2)
				pgClearSection(gs,
					pgPt(0, y),
					pgPt(width, y + TAB.line_height),
					conf.bg2);
			y += TAB.line_height;
		}
	}
	
	/* Clear the gutters */
	pgClearSection(gs,
		pgPt(0, 0),
		pgPt(TAB.total_margin - 3, height),
		conf.gutterbg);
	pgClearSection(gs,
		pgPt(width - TAB.total_margin + 3, 0),
		pgPt(width, height),
		conf.gutterbg);
	pgClearSection(gs,
		pgPt(0, line2px(LN)),
		pgPt(width, line2px(LN) + TAB.line_height),
		conf.current_line_bg);
	
	// Draw the tabs
	pgClearSection(gs, pgPt(0, 0), pgPt(width, tab_bar_height), conf.gutterbg);
	
	pgScaleFont(ui_font, global.ui_font_small_size * dpi / 72.0f, 0.0f);
	for (int i = 0; i < tab_count; i++) {
		float left = tab_width * i;
		float right = tab_width * (i + 1);
		float radius = 16;
		
		uint32_t colour = i == current_tab ? conf.chrome_active_bg : conf.chrome_inactive_bg;
		
		PgPath *path = pgNewPath();
		pgMove(path, pgPt(left, tab_bar_height));
		pgLine(path, pgPt(left, radius));
		pgQuad(path,
			pgPt(left, 0),
			pgPt(left + radius, 0));
		pgLine(path, pgPt(right - radius, 0));
		pgQuad(path,
			pgPt(right, 0),
			pgPt(right, radius));
		pgLine(path, pgPt(right, tab_bar_height));
		pgFillPath(gs, path, colour);
		pgStrokePath(gs, path, 2.5f, 0xff000000);
		pgFreePath(path);
		
		wchar_t *name = wcsrchr(tabs[i].filename, '/') ? wcsrchr(tabs[i].filename, '/') + 1 : tabs[i].filename;
		float measured = pgGetStringWidth(ui_font, name, -1);
		float x_offset = 3.5f + left + (right - left) / 2.0f - measured / 2.0f;
		float y_offset = tab_bar_height / 2.0f - pgGetFontEm(ui_font) / 2.0f;
		pgFillString(gs, ui_font,
			x_offset, y_offset,
			name, -1,
			tabs[i].buf->changes ? conf.chrome_alert_fg : i == current_tab ? conf.chrome_active_fg : conf.chrome_inactive_fg);
	}
	
	paintsel();
	
	if (global.line_numbers)
		for (int line = first; line < last; line++) {
			bool		is_bookmarked = isbookmarked(TAB.buf, line);
			unsigned	color = is_bookmarked ? conf.bookmarkfg : conf.fg;
			wchar_t		buf[16];
			wsprintf(buf, L"%6d  ", line);
			float		width = pgGetStringWidth(font[0], buf, -1);
			float		x = TAB.total_margin - width;
			if (is_bookmarked) {
				char		svg[1024];
				sprintf(svg, "M%g,%g h%g l%g,%g l%g,%g h%g Z",
					x, (float)line2px(line), width * 3.0f / 4.0f, 
					width * 1.0f / 4.0f, TAB.line_height / 2.0f,
					-width * 1.0f / 4.0f, TAB.line_height / 2.0f,
					-width * 3.0f / 4.0f);
				PgPath		*arrow = pgInterpretSvgPath(NULL, svg);
				pgFillPath(gs, arrow, conf.bookmarkbg);
				pgFreePath(arrow);
			}
			pgFillString(gs, font[0], x, line2px(line) + (TAB.line_height - TAB.ascender_height) / 2.0f, buf, -1, color);
		}
	
	if (global.minimap) {
		Pg *code_canvas = pgSubsectionCanvas(gs, pgRect(pgPt(0.0f, 0.0f), pgPt(width - minimap_width, height)));
		paintlines(code_canvas, first,last);
		pgFreeCanvas(code_canvas);
		paint_minimap(gs);
	} else
		paintlines(gs, first,last);
	
	paintstatus();
	/* Get another DC to this window to draw
	 * the status bar, which is probably outside
	 * of the update region of the PAINTSTRUCT
	 */
	
	HDC dc = GetDC(w);
	BitBlt(dc,
		0, height-TAB.line_height,
		width, height,
		double_buffer_dc,
		0, height-TAB.line_height,
		SRCCOPY);
	BitBlt(dc,
		0, 0,
		width, tab_bar_height,
		double_buffer_dc,
		0, 0,
		SRCCOPY);
	ReleaseDC(w, dc);
}

void paint_isearch_mode(PAINTSTRUCT *ps) {
	float top = height - status_bar_height - isearch_bar_height;
	
	paint_normal_mode(ps);
	
	pgClearSection(gs, pgPt(0, top), pgPt(width, top + isearch_bar_height), conf.fg);
	
	pgScaleFont(ui_font, global.ui_font_small_size* dpi / 72.0f, 0.0f);
	float x_offset = 32.0f;
	float y_offset = top + isearch_bar_height / 2.0f - pgGetFontEm(ui_font) / 2.0f;
	pgFillString(gs, ui_font,
		x_offset, y_offset,
		isearch_input.text,
		isearch_input.length,
		conf.bg);
}

void paint_fuzzy_search_mode(PAINTSTRUCT *ps) {
	pgClearSection(gs, pgPt(0, 0), pgPt(width, height), conf.chrome_bg);
	
	pgScaleFont(ui_font, global.ui_font_large_size * dpi / 72.0f, 0.0f);
	float x_offset = width * 1.0f / 4.0f;
	float y = tab_bar_height;
	float em = pgGetFontEm(ui_font);
	
	wchar_t *item_text = wcsstr(fuzzy_search_input.text, TAB.file_directory) ?
		fuzzy_search_input.text + wcslen(TAB.file_directory) :
		fuzzy_search_input.text;
	
	pgClearSection(gs, pgPt(x_offset, y), pgPt(width, y + em), conf.chrome_active_bg);
	pgFillString(gs, ui_font,
		x_offset, y,
		item_text, wcslen(item_text),
		conf.chrome_active_fg);
	y += pgGetFontEm(ui_font);

	pgScaleFont(ui_font, global.ui_font_small_size* dpi / 72.0f, 0.0f);
	em = pgGetFontEm(ui_font);
	float leading = em * 0.0125f;
	pgFillString(gs, ui_font, x_offset - em, y, L">", -1, conf.chrome_active_fg);
	for (int i = 0; i < fuzzy_count; i++) {
		if (y >= height) break;
		wchar_t *txt = fuzzy_search_files[(i + fuzzy_index) % fuzzy_count];
		pgFillString(gs, ui_font, x_offset, y + leading, txt, -1, i ? conf.chrome_fg : conf.chrome_active_fg);
		y += em + leading * 2;
	}
}

void paint(PAINTSTRUCT *ps) {
	pgIdentity(gs);
	
	if (mode == NORMAL_MODE) paint_normal_mode(ps);
	else if (mode == ISEARCH_MODE) paint_isearch_mode(ps);
	else if (mode == FUZZY_SEARCH_MODE) paint_fuzzy_search_mode(ps);
	
	BitBlt(ps->hdc,
		ps->rcPaint.left,
		ps->rcPaint.top,
		ps->rcPaint.right,
		ps->rcPaint.bottom,
		double_buffer_dc,
		ps->rcPaint.left,
		ps->rcPaint.top,
		SRCCOPY);
}

wmwheel(int clicks) {
	int	d, dy;

	d=3;
	SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &d, 0);
	
	dy=clicks * d / -WHEEL_DELTA;
	if (vis<NLINES)
		top = sat(1, top+dy, NLINES-vis+1);
	
	InvalidateRect(w, 0, 0);
}

wm_find() {
	if (fr.Flags&FR_DIALOGTERM) {
		dlg=0;
		return 0;
	}
	
	if (fr.Flags & FR_FINDNEXT)
		return autoquery();
	
	if (fr.Flags & FR_REPLACE)
		return autoreplace();
	
	if (fr.Flags & FR_REPLACEALL)
		return autoreplaceall();
	return 0;
}

void scroll_by_minimap(int x, int y) {
	float total_height = height - tab_bar_height - additional_bars;
	float each_line = min(1.0f, total_height / NLINES);
	int selected_line = y / each_line + 1;
	int half_screen = (BOT - top) / 2;
	top = sat(1, selected_line - half_screen, NLINES - vis + 1);
	InvalidateRect(w, 0, 0);
}

void wm_click(int x, int y, bool left, bool middle, bool right) {
	if (global.minimap && x >= width - minimap_width) {
		scroll_by_minimap(x, y - tab_bar_height);
		SetCapture(w);
		TAB.scrolling = true;
		return;
	}
	TAB.scrolling = false;
	if (y < tab_bar_height) {
		int selected = x / tab_width;
		if (middle) {
			int old_tab = current_tab;
			switch_tab(selected);
			act(CloseTab);
			if (old_tab != selected)
				switch_tab(old_tab);
		} else if (left)
			switch_tab(selected);
		return;
	}
	TAB.click.ln=px2line(y);
	TAB.click.ind=px2ind(TAB.click.ln, x);
	gob(TAB.buf, TAB.click.ln, TAB.click.ind);
	act(EndSelection);
	act(StartSelection);
	SetCapture(w);
}

wm_drag(int x, int y) {
	int	ln,ind;
	Loc	olo, ohi, lo, hi;
	
	if (TAB.scrolling) {
		scroll_by_minimap(x, y - tab_bar_height);
		return 0;
	}
	
	if (!TAB.click.ln)
		return 0;
	
	ordersel(TAB.buf, &olo, &ohi);
	
	ln=px2line(y);
	ind=px2ind(ln, x);
	gob(TAB.buf, ln, ind);
	
	ordersel(TAB.buf, &lo, &hi);
	invd(min(lo.ln, olo.ln), max(hi.ln, ohi.ln));
	return 1;
}

LRESULT CALLBACK
WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	PAINTSTRUCT ps;
	HDC	dc;

	switch (msg) {

	case WM_PAINT:
		BeginPaint(hwnd, &ps);
		#ifdef TIME_PAINTING
			LARGE_INTEGER start, end, frequency;
			QueryPerformanceCounter(&start);
		#endif
		paint(&ps);
		#ifdef TIME_PAINTING
			QueryPerformanceCounter(&end);
			QueryPerformanceFrequency(&frequency);
			char startPosition[64];
			char endPosition[64];
			char positionDifference[64];
			sprintf(startPosition, "(%d, %d)", ps.rcPaint.left, ps.rcPaint.top);
			sprintf(endPosition, "(%d, %d)", ps.rcPaint.right, ps.rcPaint.bottom);
			sprintf(positionDifference, "(%d, %d)",
				ps.rcPaint.right - ps.rcPaint.left,
				ps.rcPaint.bottom - ps.rcPaint.top);
			printf("%-12s - %-12s %-12s %6.4fs\n",
				startPosition, endPosition, positionDifference,
				(double)(end.QuadPart - start.QuadPart) / frequency.QuadPart);
		#endif
		EndPaint(hwnd, &ps);
		return 0;
	
	case WM_SYSKEYUP:
		if (wparam == VK_MENU)
			TAB.inhibit_auto_close ^= TRUE;
		return 0;
		
	case WM_CHAR:
		wmchar(wparam);
		TAB.inhibit_auto_close = FALSE;
		return 0;
	
	case WM_KEYDOWN:
		wmkey(wparam);
		return 0;

	
	case WM_SIZE:
		width = (short) LOWORD(lparam);
		height = (short) HIWORD(lparam);
		recalculate_text_metrics();
		
		/* Resize double-buffer */
		DeleteObject(double_buffer_bmp);
		dc=GetDC(hwnd);
		{
			BITMAPINFO info = {
			{ sizeof info.bmiHeader,
				width + 3 & ~3,
				-height,
				1,
				32,
				0,
				(width + 3 & ~3) * height * 4,
				96,
				96,
				-1,
				-1
			} };
			double_buffer_bmp=CreateDIBSection(
				NULL,
				&info,
				DIB_RGB_COLORS,
				&double_buffer_data,
				NULL,
				0);
		}
		gs->bmp = NULL;
		pgResizeCanvas(gs, width + 3 & ~3, height);
		gs->bmp = double_buffer_data;
		recalculate_text_metrics();
		SelectObject(double_buffer_dc, double_buffer_bmp);
		ReleaseDC(hwnd, dc);
		return 0;
		
	case WM_MOUSEWHEEL:
		wmwheel((short) HIWORD(wparam));
		return 0;
	
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		wm_click((short)LOWORD(lparam),
			(short)HIWORD(lparam),
			wparam & MK_LBUTTON,
			wparam & MK_MBUTTON,
			wparam & MK_RBUTTON);
		return 0;
	
	case WM_MOUSEMOVE:
		wm_drag((short)LOWORD(lparam),
			(short)HIWORD(lparam));
		return 0;
		
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		TAB.scrolling = false;
		TAB.click.ln=0;
		if (sameloc(&CAR, &SEL))
			SLN=0;
		ReleaseCapture();
		return 0;

	case WM_LBUTTONDBLCLK:
		act(SelectWord);
		return 0;
	
	case WM_COMMAND:
		act(LOWORD(wparam));
		return 0;
	
	case WM_SETFOCUS:
		start_cursor_blink();
		return 0;
		
	case WM_KILLFOCUS:
		stop_cursor_blink();
		return 0;
	
	case WM_TIMER:
		InvalidateRect(w, &(RECT){.top=line2px(LN), .bottom=line2px(LN+1), .left=0, .right=width}, FALSE);
		InvalidateRect(w, &(RECT){.top=line2px(last_cursor_line), .bottom=line2px(last_cursor_line+1), .left=0, .right=width}, FALSE);
		last_cursor_line = LN;
		cursor_phase += 0.1f;
		if (cursor_phase > 1.0f) cursor_phase = 0.0f;
		return 0;
	
	case WM_CREATE:
		/* Get device resolution */
		{
			HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
			unsigned tmpDpi;
			if (GetDpiForMonitor && GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &tmpDpi, &tmpDpi) == S_OK) {
				dpi = tmpDpi;
			} else {
				HDC hdc = GetDC(hwnd);
				dpi = GetDeviceCaps(hdc, LOGPIXELSY);
				ReleaseDC(hwnd, hdc);
			}
			CloseHandle(monitor);
		}
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	
	case WM_DROPFILES: {
		TCHAR name[MAX_PATH];
		HDROP drop = (HDROP)wparam;
		
		DragQueryFile(drop,
			DragQueryFile(drop,-1,0,0)-1,
			name, MAX_PATH);
		
		platform_normalize_path(name);
		load_file_in_new_tab(name);
		DragFinish(drop);
		return true;
		}
	
	case 0x02E0: // case WM_DPICHANGED: // Windows 8.1
		{
			RECT r = *(RECT*)lparam;
			dpi = LOWORD(wparam);
			reinitconfig();
			SetWindowPos(hwnd, NULL,
				r.left,
				r.top,
				r.right - r.left,
				r.bottom - r.top,
				SWP_NOZORDER|SWP_NOACTIVATE);
		}
		return 0;
	}
	
	/* Find dialog notification */
	if (msg==WM_FIND)
		return wm_find();

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

static
mark(int *tbl, wchar_t *list, int x) {
	while (*list)
		tbl[*list++ & 0xffff]=x;
}

static
instrlist(wchar_t *list, wchar_t *s) {
	
	return 0;
}

static
autoselectlang() {
	int	i;
	wchar_t	*list;
	
	for (i=0; i<nlangs; i++)
	for (list=langset[i].ext; *list; list+=wcslen(list)+1)
		if (!wcscmp(list, TAB.filename_extension) || !wcscmp(list, L"*")) {
			lang=langset[i];
			return 1;
		}
	return 0;
}

static
reinitlang() {
	wchar_t	*s;
	
	autoselectlang();
	
	/*
	 * Construct tables
	 * 1=punctuation
	 * 2=space
	 */
	ZeroMemory(brktbl, sizeof brktbl);
	s=lang.brk;
	while (*s)
		brktbl[*s++ & 0xffff] = 1;
	
	s=L" \t\n";
	while (*s)
		brktbl[*s++ & 0xffff] = 2;
	brktbl[0] = 1;
	
	s=lang.brace;
	ZeroMemory(opentbl, sizeof opentbl);
	ZeroMemory(closetbl, sizeof opentbl);
	ZeroMemory(quote_table, sizeof quote_table);
	for (wchar_t *s = lang.quotes; *s; s++)
		quote_table[*s & 0xffff] = true;
	for ( ; *s; s+=2) {
		closetbl[s[0] & 0xffff] = s[1];
		opentbl[s[1] & 0xffff] = s[0];
	}
}

static void reserve_vertical_space(int amount) {
	additional_bars += amount;
	vis = (height - tab_bar_height - additional_bars) / TAB.line_height;
}

static void recalculate_tab_width() {
	pgScaleFont(ui_font, global.ui_font_small_size * dpi / 72.0f, 0.0f);
	tab_width = min(width / (tab_count ? tab_count : 1), 32 * pgGetCharWidth(ui_font, 'M'));
}
static void recalculate_text_metrics() {
	float sy = conf.fontsz * TAB.magnification * dpi / 72.f;
	float sx = sy * conf.fontasp;
	pgScaleFont(font[0], sx, sy);
	pgScaleFont(font[1], sx, sy);
	pgScaleFont(font[2], sx, sy);
	pgScaleFont(font[3], sx, sy);
	
	for (int i = 0; i < conf.nbacking_fonts; i++)
		if (backing_fonts[i]) pgScaleFont(backing_fonts[i], sx, sy);
	
	TAB.ascender_height = pgGetFontAscender(font[0]) -
		pgGetFontDescender(font[0]) +
		pgGetFontLineGap(font[0]);
	TAB.line_height = TAB.ascender_height * conf.leading;
	TAB.em = pgGetCharWidth(font[0], 'M');
	TAB.tab_px_width = TAB.em * file.tabc;
	
	TAB.total_margin = global.fixed_margin +
			(global.center && TAB.max_line_width * TAB.em < width?
				(width - TAB.max_line_width * TAB.em) / 2:
				0);
	isearch_bar_height = TAB.line_height;
	status_bar_height = TAB.line_height;
	tab_bar_height = TAB.line_height;
	reserve_vertical_space(0);
	recalculate_tab_width();
}

static
configfont() {
	wchar_t tmp[MAX_PATH];
	wchar_t *p;
	int	i;
	char	features[128];
	
	if (ui_font) ui_font->free(ui_font);
	ui_font = pgOpenFont(global.ui_font_name, 400, false);
	if (!ui_font) ui_font = pgOpenFont(conf.fontname, conf.fontweight, conf.fontitalic);
	if (!ui_font) ui_font = pgOpenFont(L"Consolas", 400, false);
	if (!ui_font) ui_font = pgOpenFont(L"Courier New", 400, false);
		
	if (font[0]) font[0]->free(font[0]);
	if (font[1]) font[1]->free(font[1]);
	if (font[2]) font[2]->free(font[2]);
	if (font[3]) font[3]->free(font[3]);
	
	for (int i = 0; i < conf.nbacking_fonts; i++)
		if (backing_fonts[i]) pgFreeFont(backing_fonts[i]);
	
	font[0] = pgOpenFont(conf.fontname, conf.fontweight, conf.fontitalic);
	if (!font[0]) font[0] = pgOpenFont(L"Consolas", 400, false);
	if (!font[0]) font[0] = pgOpenFont(L"Courier New", 400, false);
	
	int weight = pgGetFontWeight(font[0]);
	const wchar_t *family = pgGetFontFamilyName(font[0]);
	bool italic = pgIsFontItalic(font[0]);
	
	font[1] = pgOpenFont(family, min(weight + 300, 900), italic);
	font[2] = pgOpenFont(family, weight, !italic);
	font[3] = pgOpenFont(family, min(weight + 300, 900), !italic);
	
	for (int i = 0; i < conf.nbacking_fonts; i++)
		backing_fonts[i] = pgOpenFont(conf.backing_font[i], 0, 0);
	
	if (!font[1]) font[1] = pgOpenFont(family, weight, italic);
	if (!font[2]) font[2] = pgOpenFont(family, weight, italic);
	if (!font[3]) font[3] = pgOpenFont(family, weight, italic);
	
	
	for (i = 0; conf.fontfeatures[i]; i++)
		features[i] = conf.fontfeatures[i];
	features[i] = 0;
	
	for (int i = 0; i < 4; i++)
		pgSetFontFeatures(font[i], (uint32_t*)(void*)features);
	recalculate_text_metrics();
	return 1;
}

static
reinitconfig() {
	RECT	rt;
	wchar_t	*s;
	
	if (gs) {
		pgSetGamma(gs, global.gamma);
		gs->flatness = max(1.000f, min(global.gfx_flatness, 2.000f));
		gs->subsamples = max(1.0f, min(global.gfx_subsamples, 16.000f));
	}
	configfont();
	reinitlang();
	start_cursor_blink();
}

static
init() {
	HDC	dc;
	RECT	rt;
	HMODULE	shcore;
	
	if (shcore = LoadLibrary(L"shcore.dll")) {
		HRESULT (*SetProcessDpiAwareness)(INT);
		SetProcessDpiAwareness = (void*) GetProcAddress(shcore, "SetProcessDpiAwareness");
		GetDpiForMonitor = (void*) GetProcAddress(shcore, "GetDpiForMonitor");
		if (SetProcessDpiAwareness)
			SetProcessDpiAwareness(2);
	}
	
	initb(TAB.buf);
	top=1;

	wc.lpfnWndProc = WndProc;
	wc.hIcon = LoadIcon(GetModuleHandle(0), (void*) 100);
	wc.hCursor = LoadCursor(NULL, IDC_IBEAM);
	RegisterClassEx(&wc);
	
	WM_FIND = RegisterWindowMessage(FINDMSGSTRING);
	fr.lpstrFindWhat = malloc((MAX_PATH + 1) * sizeof (wchar_t));
	fr.lpstrFindWhat[0] = 0;
	fr.lpstrReplaceWith = malloc((MAX_PATH + 1) * sizeof (wchar_t));
	fr.lpstrReplaceWith[0] = 0;
	
	dc=GetDC(0);
	double_buffer_dc=CreateCompatibleDC(dc);
	double_buffer_bmp=CreateCompatibleBitmap(dc, 1,1);
	SelectObject(double_buffer_dc, double_buffer_bmp);
	ReleaseDC(w, dc);
	
	gs = pgNewBitmapCanvas(0, 0);
	
	SystemParametersInfo(SPI_GETWORKAREA, 0, &rt, 0);
	w = CreateWindowEx(
		WS_EX_ACCEPTFILES|WS_EX_LAYERED,
		L"Window", L"",
		WS_OVERLAPPEDWINDOW|WS_VISIBLE,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		NULL, NULL, GetModuleHandle(0), NULL);
	SetLayeredWindowAttributes(w, 0, 255, LWA_ALPHA);
	reinitconfig();
	fr.hwndOwner = w;
}

int CALLBACK
WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {


	wchar_t	**argv;
	MSG	msg;
	int	argc;

	defglobals();
	config();
	new_tab(newb());
	reserve_vertical_space(status_bar_height);
	defperfile();
	init();

	/*
	 * Load initial file
	 */
	argv = CommandLineToArgvW(GetCommandLine(), &argc);
	if (argc>1) {
		for (int i = 1; i < argc; i++) {
			if (i > 1) new_tab(newb());
			platform_normalize_path(argv[i]);
			load_file(argv[i]);
		}
		LocalFree(argv);
	} else
		new_file();
	
	while (GetMessage(&msg, NULL, 0, 0)) {
		if (dlg && IsDialogMessage(dlg, &msg))
			continue;
		if (msg.message != WM_SYSKEYDOWN || !wmsyskeydown(msg.wParam))
			TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}

/*
 * Creates a console when compiled for conosle.
 * Ignored otherwise.
 */
// main() { return WinMain(0,0,0,0); }
