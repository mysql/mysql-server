


include ..\..\makefile.i

doall: tsbuf

tsbuf: ..\buf.lib tsbuf.c
	$(CCOM) $(CFL) -I.. -I..\.. ..\buf.lib ..\..\btr.lib ..\..\trx.lib ..\..\pars.lib ..\..\que.lib ..\..\lock.lib ..\..\row.lib ..\..\read.lib ..\..\srv.lib ..\..\com.lib ..\..\usr.lib ..\..\thr.lib ..\..\fut.lib ..\..\fsp.lib ..\..\page.lib ..\..\dyn.lib ..\..\mtr.lib ..\..\log.lib ..\..\rem.lib ..\..\fil.lib ..\..\dict.lib ..\..\data.lib ..\..\mach.lib ..\..\ha.lib ..\..\ut.lib ..\..\sync.lib ..\..\mem.lib ..\..\os.lib tsbuf.c $(LFL)

tsos: ..\buf.lib tsos.c
	$(CCOM) $(CFL) -I.. -I..\.. ..\buf.lib ..\..\mach.lib ..\..\fil.lib ..\..\ha.lib ..\..\ut.lib ..\..\sync.lib ..\..\mem.lib ..\..\os.lib tsos.c $(LFL)








