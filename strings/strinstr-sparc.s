! Copyright (C) 2000 MySQL AB
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

	.file	"strinstr-sparc.s"
.section	".text"
	.align 4
	.global strinstr
	.type	 strinstr,#function
	.proc	0102
strinstr:
	save	%sp,-96,%sp
	or	%g0,%i1,%o1
	call	strstr,2	! Result = %o0
	or	%g0,%i0,%o0
	orcc	%g0,%o0,%o0
	bne	.end
	sub	%o0,%i0,%i0
	ret
	restore	%g0,%g0,%o0
.end:
	ret
	restore	%i0,1,%o0	! Offset for return value is from 1 

.strinstr_end:
	.size	 strinstr,.strinstr_end-strinstr
	.ident	"Matt Wagner & Monty"



