! Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
! 
! This library is free software; you can redistribute it and/or
! modify it under the terms of the GNU Library General Public
! License as published by the Free Software Foundation; either
! version 2 of the License, or (at your option) any later version.
! 
! This library is distributed in the hope that it will be useful,
! but WITHOUT ANY WARRANTY; without even the implied warranty of
! MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
! Library General Public License for more details.
! 
! You should have received a copy of the GNU Library General Public
! License along with this library; if not, write to the Free
! Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
! MA 02111-1307, USA

	.file	"strmov-sparc.s"
.section	".text"
	.align 4
	.global strmov
	.type	 strmov,#function
	.proc	0102
strmov:
.loop:
	ldub	[%o1], %g2
	stb	%g2, [%o0]
	add	%o1, 1, %o1
	cmp	%g2, 0
	bne,a	.loop
	add	%o0, 1, %o0
	retl
	nop
.strmov_end:
	.size	 strmov,.strmov_end-strmov
	.ident	"Matt Wagner"
