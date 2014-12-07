wse.exe:    *.c *.h ../asg/asg.lib
	cl -Zi -Ox -Fewse.exe -nologo *.c ../asg/*.c wse.res
	mt  -nologo -manifest wse.exe.manifest -outputresource:"wse.exe;1"