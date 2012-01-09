/* vim: set noexpandtab:tabstop=8 */
#include <stdarg.h>
#include <stdio.h>

dbg(char *fmt, ...) {
	static int	cnt;
	FILE	*f = fopen("wse.log", cnt?"ab":"wb");
	va_list	va;
	
	va_start(va, fmt);
	fprintf(f, "%d: ", cnt++);
	vfprintf(f, fmt, va);
	fputs("\n", f);
	fclose(f);
	va_end(va);
}
