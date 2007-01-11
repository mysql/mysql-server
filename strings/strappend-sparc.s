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

	.file	"strappend-sparc.s"
.section	".text"
	.align 4
	.global strappend
	.type	 strappend,#function
	.proc	020
strappend:
	add	%o0, %o1, %o3		! o3 = endpos
	ldsb	[%o0], %o4
.loop1:
	add	%o0, 1, %o0		! find end of str
	cmp	%o4, 0
	bne,a	.loop1
	ldsb	[%o0], %o4
	
	sub	%o0, 1, %o0
	cmp	%o0, %o3
	bgeu	.end
	nop
	
	stb	%o2, [%o0]
.loop2:
	add	%o0, 1, %o0
	cmp	%o0, %o3
	blu,a	.loop2
	stb	%o2, [%o0]
.end:
	retl
	stb	%g0, [%o3]
.strappend_end:
	.size	 strappend,.strappend_end-strappend
	.ident	"Matt Wagner & Monty"
