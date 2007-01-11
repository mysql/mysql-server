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

	.file	"strstr-sparc.s"
.section	".text"
	.align 4
	.global strstr
	.type	 strstr,#function
	.proc	0102
strstr:

!char *strstr(register const char *str,const char *search)
!{
! register char *i,*j;
!skipp:
!  while (*str != '\0') {
!    if (*str++ == *search) {
!      i=(char*) str; j=(char*) search+1;

	ldsb	[%o1],%o2		! o2= First char of search
.top:
	ldsb	[%o0],%o4		! o4= First char of rest of str
	cmp	%o4,0
	be	.abort			! Found end null		; 
	cmp	%o4,%o2
	bne	.top
	add	%o0,1,%o0

.outloop1:

!      while (*j)
!	if (*i++ != *j++) goto skipp;

	or	%g0,%o0,%o3
	add	%o1,1,%o4		! o4= search+1
	ldsb	[%o0],%o5		! o5= [current_str+1]

.loop2:
	ldsb	[%o4],%g4
	add	%o4,1,%o4
	cmp	%g4,0
	be	.end	
	cmp	%o5,%g4
	bne	.top
	add	%o3,1,%o3
	ba	.loop2
	ldsb	[%o3],%o5

.end:
	retl
	sub	%o0,1,%o0
.abort:	
	retl
	or	%g0,0,%o0

.strstr_end:
	.size	 strstr,.strstr_end-strstr
	.ident	"Matt Wagner & Monty"
