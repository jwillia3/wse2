wse.exe:    *.c *.h ../ags/ags.lib
	cl -Zi -Ox -I ..  -Fewse.exe -nologo *.c wse.res ../ags/ags.lib
	mt  -nologo -manifest wse.exe.manifest -outputresource:"wse.exe;1"
../ags/ags.lib:
	cd ../ags && make