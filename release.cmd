@ECHO	OFF
IF	EXIST wse.exe	DEL wse.exe
DEL	*.pdb
cl  -Zi -Ox -Fewse.exe -nologo *.c wse.res -link /subsystem:windows
mt  -nologo -manifest wse.exe.manifest -outputresource:"wse.exe;1"
DEL	*.obj
