wse.exe:    *.c *.h pg/pg.lib
	cl -Zi -Ox -I .  -Fewse.exe -nologo -D_CRT_NON_CONFORMING_WCSTOK *.c wse.res pg/pg.lib
	mt  -nologo -manifest wse.exe.manifest -outputresource:"wse.exe;1"
pg/pg.lib: pg/*.c pg/*.h
	cd pg && make