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
#include <stdlib.h>
#include <wchar.h>
#pragma warning(pop)
#include <pg/pg.h>
#include "conf.h"
#include "wse.h"
#include "action.h"

#define	BOT	(top+vis)
#define TAB	tabs[current_tab]

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
HMENU		menu;
HWND		dlg;
UINT		WM_FIND;
int		width;
int		height;
float		dpi = 96.0f;
HMENU		encodingmenu;
BOOL		use_console = TRUE;
BOOL		transparent = FALSE;
#define		ID_CONSOLE 104
enum mode_t	mode;
struct input_t	isearch_input = { .before_key = isearch_before_key, .after_key = isearch_after_key };
struct input_t	fuzzy_search_input = { .before_key = fuzzy_search_before_key, .after_key = fuzzy_search_after_key };
struct input_t	*current_input;
wchar_t		**fuzzy_search_files;
wchar_t		**all_fuzzy_search_files;
Pg		*gs;
HDC		double_buffer_dc;
HBITMAP		double_buffer_bmp;
void		*double_buffer_data;
PgFont		*ui_font;
PgFont		*font[4];
int		status_bar_height = 24;
int		tab_bar_height = 24;
int		isearch_bar_height = 24;
int		additional_bars;
HBITMAP		background_bitmap;
HBRUSH		background_brush;
HPEN		background_pen;
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
	BOOL	inhibit_auto_close;
	wchar_t	*filename;
	wchar_t	file_directory[512];
	wchar_t	file_basename[512];
	wchar_t	filename_extension[512];
	struct file file_settings;
} *tabs;
int		tab_count;
int		current_tab;



OPENFILENAME	ofn = {
			sizeof ofn,
			0, 0, 0,
			0, 0, 0, 0, MAX_PATH, 0, 0, 0, 0,
			OFN_OVERWRITEPROMPT,
			0, 0, 0, 0, 0, 0, 0, 0 };
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
	return font[0]->getCharWidth(font[0], c);
}

static
ind2px(int ln, int ind) {
	wchar_t	*txt;
	int	i,px,tab;

	txt=getb(b, ln, 0);
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

	txt=getb(b, ln, 0);
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
	if (to_tab < 0) to_tab = 0;
	if (to_tab >= tab_count) to_tab = tab_count - 1;
	current_tab = to_tab;
	settitle(TAB.buf->changes);
	b = TAB.buf;
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
	};
	current_tab = tab_count;
	reinitconfig();
	return tab_count++;
}
void close_tab() {
	struct tab_t tab = tabs[current_tab];
	
//TODO: LEAKING BUFFERS
//	free(tab.filename);
//	freeb(tab.buf);
//	free(tab.buf);
	
	for (int i = current_tab; i < tab_count - 1; i++)
		tabs[i] = tabs[i + 1];
	tab_count--;
	
	if (tab_count == 0) {
		act(ExitEditor);
		return;
	}
	switch_tab(current_tab);
}


void new_file() {
	b->changes=0;
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
	
	b->changes=0;
	clearb(b);
	if (!load(filename, L"utf-8"))
		MessageBox(w, L"Could not load", L"Error", MB_OK);
	setfilename(filename);
	free(filename);
	settitle(0);
	TAB.file_settings = file;
	reinitconfig();
	
	gob(b, line_number, 0);
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
			free(filename);
			return;
		}
	free(filename);
	switch_tab(new_tab(newb()));
	load_file(filename_with_line_number);
}
void save_file() {
	if (!save(TAB.filename))
		MessageBox(w, L"Could not save", L"Error", MB_OK);
	b->changes=0;
	settitle(0);
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

INT_PTR CALLBACK
WrapProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
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
			GetWindowText(GetDlgItem(hwnd,100),
				wrapbefore,
				sizeof wrapbefore/sizeof *wrapbefore);
			GetWindowText(GetDlgItem(hwnd,101),
				wrapafter,
				sizeof wrapafter/sizeof *wrapafter);
			dlg=0;
			DestroyWindow(hwnd);
			act(WrapLine);
			return TRUE;
		case IDCANCEL:
			dlg=0;
			DestroyWindow(hwnd);
			return TRUE;
		}
	}
	return FALSE;
}

static
openwrap(HWND hwnd) {
	DIALOG_ITEM	items[] = {
		{{WS_BORDER|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,0,
			48,4, 256-48-4,14, 100},
			0x81, wrapbefore},
		{{WS_BORDER|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,0,
			48,20, 256-48-4,14, 101},
			0x81, wrapafter},
		{{WS_VISIBLE,0,
			4,8, 44,16, 102},
			0x82, L"Before: "},
		{{WS_VISIBLE,0,
			4,24, 44,16, 103},
			0x82, L"After: "},
		{{WS_VISIBLE|BS_DEFPUSHBUTTON|WS_TABSTOP,0,
			256,4, 28,16, IDOK},
			0x80, L"OK"},
		{{WS_VISIBLE|WS_TABSTOP,0,
			256,20+4, 28,16, IDCANCEL},
			0x80, L"Cancel"}
	};
	makedlg(hwnd, items, sizeof items/sizeof *items,
		L"Wrap Lines", WrapProc);
	
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

/* This does NOT snap the caret into view because the
 * scrolling routines need to use it
 */
static
movecaret() {
	int		x,y;
	SCROLLINFO	si;
	
	x = ind2px(LN, IND);
	y = line2px(LN);
	if (y > height-TAB.line_height)
		y += TAB.line_height;
	if (GetFocus()==w)
		SetCaretPos(x, y);
	
	/* Set up the scroll bars */
	si.cbSize = sizeof si;
	si.fMask = SIF_ALL ^ SIF_TRACKPOS;
	si.nMin = 1;
	si.nMax = NLINES;
	si.nPage = vis;
	si.nPos = top;
	SetScrollInfo(w, SB_VERT, &si, 1);
	return 1;
}

static
snap() {
	if (LN < top)
		top=sat(1, LN-1, NLINES);
	else if (BOT <= LN)
		top=sat(1, LN-vis+1, NLINES-vis+1);
	else
		return movecaret();
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
	
	selnow=ordersel(&lo, &hi);
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

act(int action) {
	
	STARTUPINFO		si;
	PROCESS_INFORMATION	pi;
	int	ok, wassel, onlines,oldconf;
	int	ln, ind;
	Loc	lo,hi;
	DWORD	sz;
	wchar_t	*txt;
	
	onlines=NLINES;
	wassel=ordersel(&lo, &hi);
	
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
	
	ok = _act(action);
	
	switch (action) {
	
	case ExitEditor:
		SendMessage(w, WM_CLOSE,0,0);
		break;
	
	case ToggleOverwrite:
		DestroyCaret();
		CreateCaret(w, 0, overwrite? TAB.em: 0, TAB.line_height);
		movecaret();
		ShowCaret(w);
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
	
	case PromptWrap:
		ok=openwrap(w);
		break;
	
	case DeleteLine:
	case JoinLine:
	case DupLine:
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
	
	case MoveUp:
	case MoveDown:
	case MoveLeft:
	case MoveRight:
	case MoveWordLeft:
	case MoveWordRight:
	case MoveHome:
	case MoveEnd:
	case MovePageUp:
	case MovePageDown:
	case MoveSof:
	case MoveEof:
		snap();
		invd(BOT,BOT+1);
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
	
	case PromptSaveAs:
		if (dlg)
			break;
		ofn.lpstrInitialDir=TAB.file_directory;
		ok=GetSaveFileName(&ofn);
		if (!ok)
			break;
		setfilename(ofn.lpstrFile);
		save_file();
		break;
	
	case PromptFind:
	case PromptReplace:
		if (dlg)
			break;
		fr.Flags &= ~FR_DIALOGTERM;
		if (SLN) {
			wchar_t *tmp = copysel();
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
	generalinvd(onlines, wassel, &lo, &hi);
	return ok;
}

actins(int c) {
	int	ok, wassel, onlines;
	Loc	lo,hi;
	wchar_t	*txt, *brace;
	
	onlines=NLINES;
	wassel=ordersel(&lo, &hi);
	
	brace = wcschr(lang.brace, c);
	if (brace && lang.autoClose && !TAB.inhibit_auto_close) {
		BOOL closing = brace - lang.brace & 1;
		
		txt = getb(b, LN, 0);
		if (!closing && ordersel(&lo, &hi)) {
			act(EndSelection);
			gob(b, lo.ln, lo.ind);
			_actins(c);
			gob(b, hi.ln, hi.ind+1);
			_actins(brace[1]);
			record(UndoGroup, 0, 2);
		} else if (txt[IND] == c)
			act(MoveRight);
		else if (c == '{') {
			_actins(c);
			_act(BreakLine);
			_act(BreakLine);
			_actins(brace[1]);
			_act(MoveUp);
			_actins('\t');
			record(UndoGroup, 0, 5);
		} else if (closing)
			_actins(c);
		else {
			_actins(c);
			_actins(brace[1]);
			act(MoveLeft);
			record(UndoGroup, 0, 2);
		}
	} else if (!_actins(c))
		return 0;
	
	invd(LN, LN);
	generalinvd(onlines, wassel, &lo, &hi);
	return 1;
}

actquery(wchar_t *query, int down, int sens) {
	wchar_t	title[2];
	if (!_actquery(query, down, sens))
		return 0;
	GetWindowText(dlg, title, 2);
	if (dlg && title[0]=='F')
		SendMessage(dlg, WM_CLOSE, 0, 0);
	invdafter(top);
	return 1;
}

actreplace(wchar_t *query, wchar_t *repl, int down, int sens) {
	if (!_actreplace(query, repl, down, sens))
		return 0;
	invdafter(top);
	return 1;
}

actreplaceall(wchar_t *query, wchar_t *repl, int down, int sens) {
	int	n;
	if (dlg)
		SendMessage(dlg, WM_CLOSE, 0, 0);
	n=_actreplaceall(query, repl, down, sens);
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
	current_input->text = SLN ? copysel() : wcsdup(L"");
	current_input->length = wcslen(current_input->text);
	current_input->cursor = current_input->length;
	reserve_vertical_space(isearch_bar_height);
	act(EndSelection);
	invdafter(top);
}
void isearch_next_result() {
	actisearch(isearch_input.text, true, true);
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
	actisearch(input->text, true, false);
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
	for ( ; *in; in++)
		if (!*request || wcsistr(*in, request))
			*out++ = *in, *out = NULL;
	free(request);
}
void start_fuzzy_search() {
	mode = FUZZY_SEARCH_MODE;
	current_input = &fuzzy_search_input;
	if (current_input->text)
		free(current_input->text);
	current_input->text = wcsdup(L"");
	current_input->length = 0;
	current_input->cursor = 0;
	
	if (all_fuzzy_search_files) {
		for (wchar_t **p = all_fuzzy_search_files; *p; p++) free(*p);
		free(all_fuzzy_search_files);
	}
	if (fuzzy_search_files)
		free(fuzzy_search_files);
	int count;
	all_fuzzy_search_files = platform_list_directory(TAB.file_directory, &count);
	fuzzy_search_files = calloc(count, sizeof *fuzzy_search_files);
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
				gob(b, line_number, 0);
				act(MoveHome);
			}
		} else if (fuzzy_search_files[0]) {
			wchar_t spec[MAX_PATH + 1];
			swprintf(spec, MAX_PATH + 1, L"%ls:%d",
				fuzzy_search_files[0],
				separate_line_number(input->text));
			load_file_in_new_tab(spec);
		}
		invdafter(top);
		return false;
	} else if (c == 27) { // Escape
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
	if (!current_input->before_key(current_input, c, alt, ctl, shift))
		return 1;
		
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
	}
	return 1;
}

int wmsyskeydown(int c) {
		
	int	ctl = GetAsyncKeyState(VK_CONTROL) & 0x8000;
	int	shift = GetAsyncKeyState(VK_SHIFT) & 0x8000;
	
	if (mode == ISEARCH_MODE || mode == FUZZY_SEARCH_MODE)
		return !wmkey_input(c, true, ctl, shift);
	
	switch (c) {
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
			act(DupLine);
		else
			return 0;
		break;
	case 'F':
		if (ctl && shift)
			return act(PromptFind);
		start_isearch();
		break;
	case 'N':
		new_file();
		invdafter(top);
		break;
	case 'P':
		start_fuzzy_search();
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
		if (shift)
			return act(DeleteBraces);
		else
			return act(SelectBraces);
		return 0;
	
	case 3: /* ^C */
		return act(CopySelection);
	
	case 4: /* ^D */
		return act(DupLine);
		
	case 5: /* ^E */
		return act(MoveEnd);
	
	case 6: /* ^F */
		if (ctl && shift)
			return act(PromptFind);
		start_isearch();
		return 1;
	
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
		return 0;
	
	case 12: /* ^L */
		return act(DeleteLine);
		
	case 13: /* ^M Enter */
		if (shift)
			return act(SpaceBelow);
		return act(BreakLine);
	
	case 14: /* ^N */
		new_file();
		invdafter(top);
		return true;
	
	case 15: /* ^O */
		return act(PromptOpen);
	
	case 16: /* ^P */
		if (shift)
			return act(SpaceBoth);
		return act(SpaceAbove);
	
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
		return 0;
	
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
	return act(EndSelection);
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

static uint32_t to_rgba(int win32_colour) {
	return 0xff000000 +
		(win32_colour >> 16 & 0xff) +
		(win32_colour & 0xff00) +
		(win32_colour << 16 & 0xff0000);
}

paintsel(HDC dc) {
	Loc	lo, hi;
	int	diff,x1,y1,x2,y2;
	
	if (!SLN)
		return 0;
	
	diff = abs(SLN-LN);
	ordersel(&lo, &hi);
	
	SetDCBrushColor(dc, conf.selbg);
	SetDCPenColor(dc, conf.selbg);
	x1=ind2px(lo.ln, lo.ind);
	y1=line2px(lo.ln);
	if (diff)
		Rectangle(dc, x1, y1, width, y1 + TAB.line_height);
	
	x1=diff? 0: x1;
	x2=ind2px(hi.ln, hi.ind);
	y2=line2px(hi.ln);
	Rectangle(dc, x1, y2, x2, y2 + TAB.line_height);
	
	if (diff > 1)
		Rectangle(dc, 0, y1 + TAB.line_height, 
			width, y2);
	return 1;
}

blurtext(int fontno, int x, int y, wchar_t *txt, int n, COLORREF fg) {
	wchar_t *p;
	wchar_t *end = txt + n;
	int margin = TAB.total_margin;
	int faux_bold = (fontno & 1) && font[fontno]->getWeight(font[fontno]) < 600;
	PgMatrix ctm;
	
	/* Swap R & G because windows RGB macro builds them backwards */
	fg = 0xff000000 |
		(fg >> 16 & 255) |
		fg & 0x00ff00 |
		(fg & 255) << 16;
	
	PgPt at = pgPt(x, y);
	for (p = txt; p < end; p++) {
		if (*p == '\t')
			at.x += TAB.tab_px_width - fmod(at.x - margin + TAB.tab_px_width, TAB.tab_px_width);
		else {
			if (faux_bold)
				gs->fillChar(gs, font[fontno], pgPt(at.x + 1, at.y), *p, fg);
			at.x += (int)gs->fillChar(gs, font[fontno], at, *p, fg);
		}
	}
}

paintstatus(HDC dc) {
	wchar_t	buf[1024];
	wchar_t *selmsg=L"%ls %d:%d of %d Sel %d:%d (%d %ls)";
	wchar_t *noselmsg=L"%ls %d:%d of %d";
	int	len;
	
	float top = height - status_bar_height;
	
	SetDCPenColor(dc, conf.fg);
	SetDCBrushColor(dc, conf.fg);
	Rectangle(dc, 0, top, width, height);

	len=swprintf(buf, 1024, SLN? selmsg: noselmsg,
		TAB.filename,
		LN, ind2col(LN, IND),
		NLINES,
		SLN,
		ind2col(SLN, SIND),
		SLN==LN? abs(SIND-IND): abs(SLN-LN)+1,
		SLN==LN? L"chars": L"lines");
	
	ui_font->scale(ui_font, conf.ui_font_small_size, 0.0f);
	float x = 4;
	float y = top + status_bar_height / 2.0f - ui_font->getEm(ui_font) / 2.0f;
	gs->fillString(gs, ui_font, pgPt(x, y), buf, len, to_rgba(conf.bg));
}

#include "re.h"

paintline(HDC dc, int x, int y, int line) {
	int	k,len,sect;
	void	*txt = getb(b,line,&len);
	unsigned short *i = txt, *j = txt, *end = i + len;
	SIZE	size;
	
	if (mode == ISEARCH_MODE) {
		wchar_t *i = txt;
		SetDCBrushColor(dc, conf.isearchbg);
		SetDCPenColor(dc, conf.isearchbg);
		for (i = txt; *i && (i = wcsistr(i, isearch_input.text)); i += current_input->length)
			Rectangle(dc,
				ind2px(line, i - txt),
				y - (TAB.line_height-TAB.ascender_height)/2,
				ind2px(line, i - txt + current_input->length),
				y - (TAB.line_height-TAB.ascender_height)/2 + TAB.line_height);
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
			blurtext(0, x, y, i, j-i, conf.fg);
			x=ind2px(line, j-txt);
			
			/* Then draw the keyword */
			blurtext(style, x,y, j, sect,
				conf.style[lang.kwd_color[k]].color);
			SetTextColor(dc, conf.fg);
			i=j+=sect;
			x=ind2px(line,j-txt);
		} else  if (*j && brktbl[*j] != 1) { /* Skip spaces or word */
			int kind = brktbl[*j];
			while (*j && brktbl[*j] == kind) j++;
		} else /* Skip one breaker */
			j++;
	}
	if (j>i)
		blurtext(0, x,y, i, j-i, conf.fg);
}

paintlines(HDC dc, int first, int last) {
	int	line, _y=line2px(first);
	
	SetTextColor(dc, conf.fg);
	for (line=first; line<=last && line <= BOT; line++, _y += TAB.line_height)
		paintline(dc,
			TAB.total_margin,
			_y + (TAB.line_height-TAB.ascender_height)/2, line);
}

void paint_normal_mode(PAINTSTRUCT *ps) {
	int	i,n,y,x,len,first,last;
	
	first = px2line(ps->rcPaint.top);
	last = px2line(ps->rcPaint.bottom);
	
	/* Clear the background */
	SelectObject(double_buffer_dc, background_brush);
	SelectObject(double_buffer_dc, background_pen);
	Rectangle(double_buffer_dc, ps->rcPaint.left-1, ps->rcPaint.top-1,
		ps->rcPaint.right+1, ps->rcPaint.bottom+1);
	
	SelectObject(double_buffer_dc, GetStockObject(DC_BRUSH));
	SelectObject(double_buffer_dc, GetStockObject(DC_PEN));
	
	/* Draw odd line's background */
	if (!*conf.bgimage && conf.bg2 != conf.bg) {
		SetDCPenColor(double_buffer_dc, conf.bg2);
		SetDCBrushColor(double_buffer_dc, conf.bg2);
		y=line2px(first);
		for (i=first; i<=last; i++) {
			if (i % 2)
				Rectangle(double_buffer_dc, 0, y, width, y+TAB.line_height);
			y += TAB.line_height;
		}
	}
	
	/* Clear the gutters */
	SetDCPenColor(double_buffer_dc, conf.gutterbg);
	SetDCBrushColor(double_buffer_dc, conf.gutterbg);
	Rectangle(double_buffer_dc, 0, 0, TAB.total_margin - 3, height);
	Rectangle(double_buffer_dc, width - TAB.total_margin + 3, 0, width, height);
	
	/* Draw bookmark line's background */
	y=line2px(first);
	for (i=first; i<=last; i++) {
		if (isbookmarked(i)) {
			SetDCPenColor(double_buffer_dc, conf.bookmarkbg);
			SetDCBrushColor(double_buffer_dc, conf.bookmarkbg);
			Rectangle(double_buffer_dc, 0, y, width, y+TAB.line_height);
		}
		y += TAB.line_height;
	}
	
	// Draw the tabs
	PgPath *path = pgNewPath();
	path->move(path, &gs->ctm, pgPt(0.0f + 0.5f, 0.5f));
	path->line(path, &gs->ctm, pgPt(width - 0.5f, 0.5f));
	path->line(path, &gs->ctm, pgPt(width - 0.5f, tab_bar_height - 0.5f));
	path->line(path, &gs->ctm, pgPt(0.0f + 0.5f, tab_bar_height - 0.5f));
	path->close(path);
	gs->fill(gs, path, to_rgba(conf.gutterbg));
	path->free(path);
	ui_font->scale(ui_font, conf.ui_font_small_size * dpi / 72.0f, 0.0f);
	for (int i = 0; i < tab_count; i++) {
		float left = (width / tab_count) * i;
		float right = (width / tab_count) * (i + 1);
		
		PgPath *path = pgNewPath();
		path->move(path, &gs->ctm, pgPt(left + 3.5f + 5.0f, 0.5f));
		path->line(path, &gs->ctm, pgPt(right - 3.5f - 5.0f, 0.5f));
		path->line(path, &gs->ctm, pgPt(right - 3.5f, tab_bar_height - 0.5f));
		path->line(path, &gs->ctm, pgPt(left + 3.5f, tab_bar_height - 0.5f));
		path->close(path);
		gs->fill(gs, path, to_rgba(i == current_tab ? conf.active_tab : conf.inactive_tab));
		path->free(path);
		
		float measured = 0.0;
		int length = 0;
		wchar_t *name = tabs[i].filename;
		if (wcsrchr(name, '/'))
			name = wcsrchr(name, '/') + 1;
		if (wcsrchr(name, '\\'))
			name = wcsrchr(name, '\\') + 1;
		for (wchar_t *p = name; *p; p++) {
			float px = ui_font->getCharWidth(ui_font, *p);
			if (measured + px >= (right - left) - 6.0f) break;
			measured += px;
			length++;
		}
		float x_offset = 3.5f + left + (right - left) / 2.0f - measured / 2.0f;
		float y_offset = tab_bar_height / 2.0f - ui_font->getEm(ui_font) / 2.0f;
		gs->fillString(gs, ui_font,
			pgPt(x_offset, y_offset),
			name, length,
			to_rgba(tabs[i].buf->changes ? conf.unsaved_file : conf.saved_file));
	}
	
	paintsel(double_buffer_dc);
	
	/* Draw the wire */
	if (global.wire) {
		HPEN pen;
		int i, n=sizeof global.wire/sizeof *global.wire;
		pen = CreatePen(PS_DOT, 1, conf.fg);
		SetBkMode(double_buffer_dc, TRANSPARENT);
		SelectObject(double_buffer_dc, pen);
		for (i=0; i<n; i++) {
			x=TAB.total_margin + global.wire[i] * TAB.em;
			MoveToEx(double_buffer_dc, x, tab_bar_height, 0);
			LineTo(double_buffer_dc, x, ps->rcPaint.bottom);
		}
		SelectObject(double_buffer_dc, GetStockObject(DC_PEN));
		DeleteObject(pen);
	}
	
	paintlines(double_buffer_dc,first,last);
	paintstatus(double_buffer_dc);
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
	
	PgPath *path = pgNewPath();
	path->move(path, &gs->ctm, pgPt(0.0f, top + 0.5f));
	path->line(path, &gs->ctm, pgPt(width - 0.5f, top + 0.5f));
	path->line(path, &gs->ctm, pgPt(width - 0.5f, top + isearch_bar_height - 0.5f));
	path->line(path, &gs->ctm, pgPt(0.0f + 0.5f, top + isearch_bar_height - 0.5f));
	path->close(path);
	gs->fill(gs, path, to_rgba(conf.fg));
	path->free(path);
	
	ui_font->scale(ui_font, conf.ui_font_small_size, 0.0f);
	float x_offset = 32.0f;
	float y_offset = top + 4.0f;
	gs->fillString(gs, ui_font, pgPt(x_offset, y_offset),
		isearch_input.text,
		isearch_input.length,
		to_rgba(conf.bg));
}

void paint_fuzzy_search_mode(PAINTSTRUCT *ps) {
	PgPath *path = pgNewPath();
	path->move(path, &gs->ctm, pgPt(0.0f, 0.5f));
	path->line(path, &gs->ctm, pgPt(width - 0.5f, 0.5f));
	path->line(path, &gs->ctm, pgPt(width - 0.5f, height - 0.5f));
	path->line(path, &gs->ctm, pgPt(0.0f + 0.5f, height - 0.5f));
	path->close(path);
	gs->fill(gs, path, to_rgba(conf.fg));
	path->free(path);
	
	float x_offset = width * 1.0f / 4.0f;
	float y = tab_bar_height;
	ui_font->scale(ui_font, conf.ui_font_large_size * dpi / 72.0f, 0.0f);
	gs->fillString(gs, ui_font, pgPt(x_offset, y),
		fuzzy_search_input.text, fuzzy_search_input.length,
		to_rgba(conf.bg));
	y += ui_font->getEm(ui_font);

	ui_font->scale(ui_font, conf.ui_font_small_size * dpi / 72.0f, 0.0f);
	float em = ui_font->getEm(ui_font);
	float leading = em * 0.0125f;
	for (wchar_t **p = fuzzy_search_files; p && *p; p++, y += em + leading * 2)
		gs->fillString(gs, ui_font, pgPt(x_offset, y + leading),
			*p, -1, to_rgba(conf.bg));
}

void paint(PAINTSTRUCT *ps) {
	gs->identity(gs);
	
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

wmscroll(int action) {
	SCROLLINFO si;

	switch (action) {

	case SB_PAGEUP:
		top = sat(1, top-vis+1, NLINES);
		break;

	case SB_LINEUP:
		top = sat(1, top-1, NLINES);
		break;

	case SB_PAGEDOWN:
		top = sat(1, BOT-1, NLINES);
		break;

	case SB_LINEDOWN:
		top = sat(1, top+1, NLINES);
		break;

	case SB_THUMBTRACK:
		si.cbSize = sizeof si;
		si.fMask = SIF_TRACKPOS;
		GetScrollInfo(w, SB_VERT, &si);
		top = si.nTrackPos;
		break;
	}
	
	movecaret();
	InvalidateRect(w, 0, 0);
}

wmwheel(int clicks) {
	int	d, dy;

	d=3;
	SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &d, 0);
	
	dy=clicks * d / -WHEEL_DELTA;
	if (vis<NLINES)
		top = sat(1, top+dy, NLINES-vis+1);
	
	movecaret();
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

void wm_click(int x, int y, bool left, bool middle, bool right) {
	if (y < tab_bar_height) {
		int selected = x / (width / tab_count);
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
	gob(b, TAB.click.ln, TAB.click.ind);
	act(EndSelection);
	act(StartSelection);
	SetCapture(w);
}

wm_drag(int x, int y) {
	int	ln,ind;
	Loc	olo, ohi, lo, hi;
	
	if (!TAB.click.ln)
		return 0;
	
	ordersel(&olo, &ohi);
	
	ln=px2line(y);
	ind=px2ind(ln, x);
	gob(b, ln, ind);
	
	ordersel(&lo, &hi);
	invd(min(lo.ln, olo.ln), max(hi.ln, ohi.ln));
	return 1;
}

void fix_caret() {
	if (GetFocus() != w) return;
	CreateCaret(w, 0, overwrite? TAB.em: 0, TAB.line_height);
	movecaret();
	ShowCaret(w);
}

LRESULT CALLBACK
WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	PAINTSTRUCT ps;
	HDC	dc;

	switch (msg) {

	case WM_PAINT:
		BeginPaint(hwnd, &ps);
		paint(&ps);
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
	
	
	case WM_SETFOCUS:
		fix_caret();
		return 0;
	
	case WM_KILLFOCUS:
		DestroyCaret();
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
		gs->width = width + 3 & ~3;
		gs->height = height;
		((PgBitmapCanvas*)gs)->data = double_buffer_data;
		TAB.total_margin = conf.fixed_margin +
			(conf.center && global.wire[2] * TAB.em < width?
				(width - global.wire[2] * TAB.em) / 2:
				0);
		movecaret();
		SelectObject(double_buffer_dc, double_buffer_bmp);
		ReleaseDC(hwnd, dc);
		return 0;
		
	case WM_MOUSEWHEEL:
		wmwheel((short) HIWORD(wparam));
		return 0;
	
	case WM_VSCROLL:
		wmscroll((short) LOWORD(wparam));
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
	
	case WM_CREATE:
		/* Get device resolution */
		{
			HDC hdc = GetDC(hwnd);
			dpi = GetDeviceCaps(hdc, LOGPIXELSY);
			ReleaseDC(hwnd, hdc);
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
	for ( ; *s; s+=2) {
		closetbl[s[0] & 0xffff] = s[1];
		opentbl[s[1] & 0xffff] = s[0];
	}
}

static void reserve_vertical_space(int amount) {
	additional_bars += amount;
	vis = (height - tab_bar_height - additional_bars) / TAB.line_height;
}

static void recalculate_text_metrics() {
	float sy = conf.fontsz * TAB.magnification * dpi / 72.f;
	float sx = sy * conf.fontasp;
	font[0]->scale(font[0], sy, sx);
	font[1]->scale(font[1], sy, sx);
	font[2]->scale(font[2], sy, sx);
	font[3]->scale(font[3], sy, sx);
	
	TAB.ascender_height = font[0]->getAscender(font[0])
		- font[0]->getDescender(font[0])
		+ font[0]->getLeading(font[0]);
	TAB.line_height = TAB.ascender_height * conf.leading;
	TAB.em = font[0]->getCharWidth(font[0], 'M');
	TAB.tab_px_width = TAB.em * file.tabc;
	
	TAB.total_margin = conf.fixed_margin +
			(conf.center && global.wire[2] * TAB.em < width?
				(width - global.wire[2] * TAB.em) / 2:
				0);
	reserve_vertical_space(0);
	fix_caret();
}

static
configfont() {
	wchar_t tmp[MAX_PATH];
	wchar_t *p;
	int	i;
	char	features[128];
	
	if (font[0]) font[0]->free(font[0]);
	if (font[1]) font[1]->free(font[1]);
	if (font[2]) font[2]->free(font[2]);
	if (font[3]) font[3]->free(font[3]);
	
	font[0] = pgOpenFont(conf.fontname, conf.fontweight, conf.fontitalic, 0);
	if (!font[0])
		font[0] = pgOpenFont(L"Courier New", 400, false, 0);
	
	PgFontWeight weight = font[0]->getWeight(font[0]);
	PgFontStretch stretch = 0;
	const wchar_t *family = font[0]->getFamily(font[0]);
	bool italic = font[0]->isItalic(font[0]);
	
	font[1] = pgOpenFont(family, min(weight + 300, 900), italic, stretch);
	font[2] = pgOpenFont(family, weight, !italic, stretch);
	font[3] = pgOpenFont(family, min(weight + 300, 900), !italic, stretch);
	
	if (!font[1]) font[1] = pgOpenFont(family, weight, italic, stretch);
	if (!font[2]) font[2] = pgOpenFont(family, weight, italic, stretch);
	if (!font[3]) font[3] = pgOpenFont(family, weight, italic, stretch);
	
	
	for (i = 0; conf.fontfeatures[i]; i++)
		features[i] = conf.fontfeatures[i];
	features[i] = 0;
	
	for (i = 0; i < 4; i++)
		font[i]->useFeatures(font[i], features);
	
	recalculate_text_metrics();
	return 1;
}

static
reinitconfig() {
	RECT	rt;
	wchar_t	*s;
	
	pgSetGamma(global.gamma);
	configfont();
	reinitlang();
	
	if (background_bitmap)
		DeleteObject(background_bitmap);

	background_bitmap=LoadImage(GetModuleHandle(0),
		conf.bgimage? conf.bgimage: L"",
		IMAGE_BITMAP, 0, 0,
		LR_LOADFROMFILE);
	if (background_bitmap) {
		background_brush=CreatePatternBrush(background_bitmap);
		background_pen=GetStockObject(NULL_PEN);
	} else {
		background_brush=CreateSolidBrush(conf.bg);
		background_pen=CreatePen(PS_SOLID, 1, conf.bg);
	}
}

static
init() {
	HDC	dc;
	RECT	rt;
	HMODULE	shcore;
	
	if (shcore = LoadLibrary(L"shcore.dll")) {
		HRESULT (*SetProcessDpiAwareness)(INT);
		SetProcessDpiAwareness = (void*) GetProcAddress(shcore,
			"SetProcessDpiAwareness");
		if (SetProcessDpiAwareness)
			SetProcessDpiAwareness(2);
	}
	
	initb(b);
	top=1;

	wc.lpfnWndProc = WndProc;
	wc.hIcon = LoadIcon(GetModuleHandle(0), (void*) 100);
	wc.hCursor = LoadCursor(NULL, IDC_IBEAM);
	RegisterClassEx(&wc);
	
	/* Configure Open Dialog */
	ofn.lpstrFile = malloc(MAX_PATH * sizeof (wchar_t));
	ofn.lpstrFile[0] = 0;
	ofn.hwndOwner = w;
	
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
	
	if (ui_font) ui_font->free(ui_font);
	ui_font = pgOpenFont(conf.ui_font_name, 400, false, 0);
	
	
	SystemParametersInfo(SPI_GETWORKAREA, 0, &rt, 0);
	w = CreateWindowEx(
		WS_EX_ACCEPTFILES|WS_EX_LAYERED,
		L"Window", L"",
		WS_OVERLAPPEDWINDOW|WS_VISIBLE,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		1024,
		800,
		NULL, menu, GetModuleHandle(0), NULL);
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
	new_tab(b = newb());
	reserve_vertical_space(status_bar_height);
	defperfile();
	init();

	/*
	 * Load initial file
	 */
	argv = CommandLineToArgvW(GetCommandLine(), &argc);
	if (argc>1) {
		for (int i = 1; i < argc; i++) {
			if (i > 1) new_tab(b = newb());
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
