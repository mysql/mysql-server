


include ..\..\makefile.i

doall: tsos tsosaux


tsos: ..\os.lib tsos.c
	$(CCOM) $(CFL) -I.. -I..\.. ..\..\ut.lib ..\..\sync.lib ..\..\mem.lib ..\os.lib tsos.c $(LFL)

tsosaux: tsosaux.c
	$(CCOM) $(CFL) -I.. -I..\.. ..\..\ut.lib ..\os.lib tsosaux.c $(LFL)







