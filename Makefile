CL = 
wse.exe:    *.c *.h pg2/pg.lib
	cl -Zi -Ox -I .  -Fewse.exe -nologo -D_CRT_NON_CONFORMING_WCSTOK *.c wse.res pg2/pg.lib
pg2/pg.lib: pg2/*.c pg2/*.h
	cd pg2 && $(MAKE)