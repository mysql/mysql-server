


include ..\..\makefile.i

doall: tscom tscli

tscom: ..\com.lib tscom.c makefile
	$(CCOM) $(CFL) -I.. -I..\.. ..\com.lib ..\..\ut.lib ..\..\mem.lib ..\..\sync.lib ..\..\os.lib tscom.c $(LFL)

tscli: ..\com.lib tscli.c makefile
	$(CCOM) $(CFL) -I.. -I..\.. ..\com.lib ..\..\ut.lib ..\..\mem.lib ..\..\sync.lib ..\..\os.lib tscli.c $(LFL)







