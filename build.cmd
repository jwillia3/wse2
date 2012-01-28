@ECHO	OFF
IF	EXIST wse.exe	DEL wse.exe
cl  -Zi -GL -Gy -GS -WL -Fewse.exe -nologo *.c wse.res -link /subsystem:console user32.lib gdi32.lib shell32.lib comdlg32.lib uxtheme.lib
mt  -nologo -manifest wse.exe.manifest -outputresource:"wse.exe;1"
DEL	*.obj
