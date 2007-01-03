! Copyright (C) 2000, 2002 MySQL AB
! 
! This library is free software; you can redistribute it and/or
! modify it under the terms of the GNU Library General Public
! License as published by the Free Software Foundation; version 2
! of the License.
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

	.file	"strend-sparc.s"
.section	".text"
	.align 4
	.global strend
	.type	 strend,#function
	.proc	0102
strend:
	ldsb	[%o0], %o3		! Handle first char differently to make
.loop:					! a faster loop
	add	%o0, 1, %o0
	cmp	%o3, 0
	bne,a	.loop
	ldsb	[%o0], %o3
	retl
	sub	%o0,1,%o0
.strend_end:
	.size	 strend,.strend_end-strend
	.ident	"Matt Wagner & Monty"
