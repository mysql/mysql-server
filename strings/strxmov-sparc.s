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

	.file	"strxmov-sparc.s"
.section	".text"
	.align 4
	.global strxmov
	.type	 strxmov,#function
	.proc	0102
strxmov:
	st	%o2, [%sp+76]		! store 3rd param before other params
	st	%o3, [%sp+80]		! store 4th param "   "
	cmp	%o1, 0			! check if no from args
	st	%o4, [%sp+84]		! store 5th param
	be	.end
	st	%o5, [%sp+88]		! store last
	add	%sp, 76, %g2		! put pointer to 3rd arg
.loop:
	ldub	[%o1], %g1		! set values of src (o1)
	add	%o1, 1, %o1		! inc src
	stb	%g1, [%o0]		!   and dst (o2) equal
	cmp	%g1, 0			! second while cmp
	bne,a	.loop
	add	%o0, 1, %o0		! inc dst
	ld	[%g2], %o1		! get next param
	cmp	%o1, 0			! check if last param
	bne	.loop
	add	%g2, 4, %g2		! advance to next param
.end:
	retl
	stb	%g0, [%o0]
.strxmov_end:
	.size	 strxmov,.strxmov_end-strxmov
	.ident	"Matt Wagner & Monty"

