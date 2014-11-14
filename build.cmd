@ECHO	OFF
IF	EXIST wse.exe	DEL wse.exe
cl  -MDd -Zi -GL -Gy -GS -WL -Fewse.exe -nologo *.c asg/*.c wse.res -link /subsystem:console
mt  -nologo -manifest wse.exe.manifest -outputresource:"wse.exe;1"
DEL	*.obj
