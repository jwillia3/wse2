void platform_scan_directory(const wchar_t *dir, void per_file(const wchar_t *name, void *data));
wchar_t **platform_list_fonts(int *countp);
AsgFont *platform_open_font_file(const wchar_t *filename, int font_index, bool scan_only);