include ..\..\makefile.i

doall: tssrv tscli isql

tssrv: ..\btr.lib tssrv.c
	$(CCOM) $(CFL) -I.. -I..\.. ..\btr.lib ..\..\eval.lib ..\..\ibuf.lib ..\..\trx.lib ..\..\pars.lib ..\..\que.lib ..\..\lock.lib ..\..\row.lib ..\..\read.lib ..\..\srv.lib ..\..\com.lib ..\..\usr.lib ..\..\thr.lib ..\..\fut.lib ..\..\fsp.lib ..\..\page.lib ..\..\dyn.lib ..\..\mtr.lib ..\..\log.lib ..\..\rem.lib ..\..\fil.lib ..\..\buf.lib ..\..\dict.lib ..\..\data.lib ..\..\mach.lib ..\..\ha.lib ..\..\ut.lib ..\..\sync.lib ..\..\mem.lib ..\..\os.lib tssrv.c $(LFL)

tscli: ..\btr.lib tscli.c
	$(CCOM) $(CFL) -I.. -I..\.. ..\btr.lib ..\..\ib_odbc.lib ..\..\eval.lib ..\..\ibuf.lib ..\..\trx.lib ..\..\pars.lib ..\..\que.lib ..\..\lock.lib ..\..\row.lib ..\..\read.lib ..\..\srv.lib ..\..\com.lib ..\..\usr.lib ..\..\thr.lib ..\..\fut.lib ..\..\fsp.lib ..\..\page.lib ..\..\dyn.lib ..\..\mtr.lib ..\..\log.lib ..\..\rem.lib ..\..\fil.lib ..\..\buf.lib ..\..\dict.lib ..\..\data.lib ..\..\mach.lib ..\..\ha.lib ..\..\ut.lib ..\..\sync.lib ..\..\mem.lib ..\..\os.lib tscli.c $(LFL)

isql: ..\btr.lib isql.c
	$(CCOM) $(CFL) -I.. -I..\.. ..\btr.lib ..\..\ib_odbc.lib ..\..\eval.lib ..\..\ibuf.lib ..\..\trx.lib ..\..\pars.lib ..\..\que.lib ..\..\lock.lib ..\..\row.lib ..\..\read.lib ..\..\srv.lib ..\..\com.lib ..\..\usr.lib ..\..\thr.lib ..\..\fut.lib ..\..\fsp.lib ..\..\page.lib ..\..\dyn.lib ..\..\mtr.lib ..\..\log.lib ..\..\rem.lib ..\..\fil.lib ..\..\buf.lib ..\..\dict.lib ..\..\data.lib ..\..\mach.lib ..\..\ha.lib ..\..\ut.lib ..\..\sync.lib ..\..\mem.lib ..\..\os.lib isql.c $(LFL)

tsrecv: ..\btr.lib tsrecv.c
	$(CCOM) $(CFL) -I.. -I..\.. ..\btr.lib ..\..\ibuf.lib ..\..\trx.lib ..\..\pars.lib ..\..\que.lib ..\..\lock.lib ..\..\row.lib ..\..\read.lib ..\..\srv.lib ..\..\com.lib ..\..\usr.lib ..\..\thr.lib ..\..\fut.lib ..\..\fsp.lib ..\..\page.lib ..\..\dyn.lib ..\..\mtr.lib ..\..\log.lib ..\..\rem.lib ..\..\fil.lib ..\..\buf.lib ..\..\dict.lib ..\..\data.lib ..\..\mach.lib ..\..\ha.lib ..\..\ut.lib ..\..\sync.lib ..\..\mem.lib ..\..\os.lib tsrecv.c $(LFL)

