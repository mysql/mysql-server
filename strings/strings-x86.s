# Copyright (C) 2000 MySQL AB
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# Optimized string functions Intel 80x86  (gcc/gas syntax)

	.file	"strings.s"
	.version "1.00"

.text

#	Move a alligned, not overlapped, by (long) divided memory area
#	Args: to,from,length

.globl bmove_align
	.type	 bmove_align,@function
bmove_align:	
	movl	%edi,%edx
	push	%esi
	movl	4(%esp),%edi		# to
	movl	8(%esp),%esi		# from
	movl	12(%esp),%ecx		# length
	addw	$3,%cx			# fix if not divisible with long
	shrw	$2,%cx
	jz	.ba_20
	.p2align 4,,7
.ba_10:
	movl	-4(%esi,%ecx),%eax
	movl	 %eax,-4(%edi,%ecx)
	decl	%ecx
	jnz	.ba_10
.ba_20:	pop	%esi
	movl	%edx,%edi
	ret

.bmove_align_end:	
	.size	 bmove_align,.bmove_align_end-bmove_align

	# Move a string from higher to lower
	# Arg from_end+1,to_end+1,length

.globl bmove_upp
	.type bmove_upp,@function
bmove_upp:	
	movl	%edi,%edx		# Remember %edi
	push	%esi
	movl	8(%esp),%edi		# dst
	movl	16(%esp),%ecx		# length
	movl	12(%esp),%esi		# source
	test	%ecx,%ecx
	jz	.bu_20
	subl	%ecx,%esi		# To start of strings
	subl	%ecx,%edi
	
	.p2align 4,,7
.bu_10:	movb	-1(%esi,%ecx),%al
	movb	 %al,-1(%edi,%ecx)
	decl	%ecx
	jnz	.bu_10
.bu_20:	pop	%esi
	movl	%edx,%edi
	ret

.bmove_upp_end:	
	.size bmove_upp,.bmove_upp_end-bmove_upp

	# Append fillchars to string
	# Args: dest,len,fill

.globl strappend
	.type strappend,@function
strappend:	
	pushl	%edi
	movl	8(%esp),%edi		#  Memory pointer
	movl	12(%esp),%ecx		#  Length
	clrl	%eax			#  Find end of string
	repne
	scasb
	jnz	sa_99			#  String to long, shorten it
	movzb	16(%esp),%eax		#  Fillchar
	decl	%edi			#  Point at end null
	incl	%ecx			#  rep made one dec for null-char

	movb	%al,%ah			# (2) Set up a 32 bit pattern.
	movw	%ax,%dx			# (2)
	shll	$16,%eax		# (3)
	movw	%dx,%ax			# (2) %eax has the 32 bit pattern.

	movl	%ecx,%edx		# (2) Save the count of bytes.
	shrl	$2,%ecx			# (2) Number of dwords.
	rep
	stosl				# (5 + 5n)
	movb	$3,%cl			# (2)
	and	%edx,%ecx		# (2) Fill in the odd bytes
	rep
	stosb				#  Move last bytes if any

sa_99:	movb	$0,(%edi)		#  End of string
	popl	%edi
	ret
.strappend_end:	
	.size strappend,.strappend_end-strappend

	# Find if string contains any char in another string
	# Arg: str,set
	# Ret: Pointer to first found char in str

.globl strcont
	.type strcont,@function
strcont:	
	movl	%edi,%edx
	pushl	%esi
	movl	8(%esp),%esi		#  str
	movl	12(%esp),%ecx		#  set
	clrb	%ah			#  For endtest
	jmp	sc_60

sc_10:	scasb
	jz	sc_fo			#  Found char
sc_20:	cmp	(%edi),%ah		#  Test if null
	jnz	sc_10			#  Not end of set yet
	incl	%esi			#  Next char in str
sc_60:	movl	%ecx,%edi		#  %edi = Set
	movb	(%esi),%al		#  Test if this char exist
	andb	%al,%al
	jnz	sc_20			#  Not end of string
	clrl	%esi			#  Return Null
sc_fo:	movl	%esi,%eax		#  Char found here
	movl	%edx,%edi		#  Restore
	popl	%esi
	ret
.strcont_end:	
	.size strcont,.strcont_end-strcont

	# Find end of string
	# Arg: str
	# ret: Pointer to end null

.globl strend
	.type strend,@function
strend:	
	movl	%edi,%edx		#  Save
	movl	4(%esp),%edi		#  str
	clrl	%eax			#  Find end of string
	movl	%eax,%ecx
	decl	%ecx			#  ECX = -1
	repne
	scasb
	movl	%edi,%eax
	decl	%eax			#  End of string
	movl	%edx,%edi		#  Restore
	ret
.strend_end:	
	.size strend,.strend_end-strend

	# Make a string with len fill-chars and endnull
	# Args: dest,len,fill
	# Ret:  dest+len

.globl strfill
	.type strfill,@function
strfill:
	pushl	%edi
	movl	8(%esp),%edi		#  Memory pointer
	movl	12(%esp),%ecx		#  Length
	movzb	16(%esp),%eax		#  Fill

	movb	%al,%ah			# (2) Set up a 32 bit pattern
	movw	%ax,%dx			# (2)
	shll	$16,%eax		# (3)
	movw	%dx,%ax			# (2) %eax has the 32 bit pattern.

	movl	%ecx,%edx		# (2) Save the count of bytes.
	shrl	$2,%ecx			# (2) Number of dwords.
	rep
	stosl				# (5 + 5n)
	movb	$3,%cl			# (2)
	and	%edx,%ecx		# (2) Fill in the odd bytes
	rep
	stosb				#  Move last bytes if any

	movb	%cl,(%edi)		#  End NULL
	movl	%edi,%eax		#  End i %eax
	popl	%edi
	ret
.strfill_end:	
	.size strfill,.strfill_end-strfill


	# Find a char in or end of a string
	# Arg: str,char
	# Ret: pointer to found char or NullS

.globl strcend
	.type strcend,@function
strcend:
	movl	%edi,%edx
	movl	4(%esp),%edi		# str
	movb	8(%esp),%ah		# search
	clrb	%al			# for scasb to find end

se_10:	cmpb	(%edi),%ah
	jz	se_20			# Found char
	scasb
	jnz	se_10			# Not end
	dec 	%edi			# Not found, point at end of string
se_20:	movl	%edi,%eax
	movl	%edx,%edi		# Restore
	ret
.strcend_end:	
	.size strcend,.strcend_end-strcend

	# Test if string has a given suffix

.globl is_prefix
	.type is_prefix,@function
is_prefix:	
	movl	%edi,%edx		# Save %edi
	pushl	%esi			# and %esi
	movl	12(%esp),%esi		# get suffix
	movl	8(%esp),%edi		# s1
	movl	$1,%eax			# Ok and zero-test
ip_10:	cmpb	(%esi),%ah
	jz	suf_ok			# End of string/ found suffix
	cmpsb				# Compare strings
	jz	ip_10			# Same, possible prefix
	xor	%eax,%eax		# Not suffix
suf_ok:	popl	%esi
	movl	%edx,%edi
	ret
.is_prefix_end:	
	.size is_prefix,.is_prefix_end-is_prefix

	# Find a substring in string
	# Arg: str,search

.globl strstr
	.type strstr,@function

strstr:	
	pushl	%edi
	pushl	%esi
	movl	12(%esp),%esi		#  str
	movl	16(%esp),%edi		#  search
	movl	%edi,%ecx
	incl	%ecx			#  %ecx = search+1
	movb	(%edi),%ah		#  %ah = First char in search
	jmp	sf_10

sf_00:	movl	%edx,%esi		#  si = Current str-pos
sf_10:	movb	(%esi),%al		#  Test if this char exist
	andb	%al,%al
	jz	sf_90			#  End of string, didn't find search
	incl	%esi
	cmpb	%al,%ah
	jnz	sf_10			#  Didn't find first char, continue
	movl	%esi,%edx		#  Save str-pos in %edx
	movl	%ecx,%edi
sf_20:	cmpb	$0,(%edi)
	jz	sf_fo			#  Found substring
	cmpsb
	jz	sf_20			#  Char ok
	jmp	sf_00			#  Next str-pos

sf_90:	movl	$1,%edx			#  Return Null
sf_fo:	movl	%edx,%eax		#  Char found here
	decl	%eax			#  Pointed one after
	popl	%esi
	popl	%edi
	ret
.strstr_end:	
	.size strstr,.strstr_end-strstr


	# Find a substring in string, return index
	# Arg: str,search

.globl strinstr
	.type strinstr,@function

strinstr:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	12(%ebp)		#  search
	pushl	8(%ebp)			#  str
	call	strstr
	add	$8,%esp
	or	%eax,%eax
	jz	si_99			#  Not found, return NULL
	sub	8(%ebp),%eax		#  Pos from start
	inc	%eax			#  And first pos = 1
si_99:	popl	%ebp
	ret
.strinstr_end:	
	.size strinstr,.strinstr_end-strinstr

	# Make a string of len length from another string
	# Arg: dst,src,length
	# ret: end of dst

.globl strmake
	.type strmake,@function

strmake:	
	pushl	%edi
	pushl	%esi
	mov	12(%esp),%edi		# dst
	movl	$0,%edx
	movl	20(%esp),%ecx		# length
	movl	16(%esp),%esi		# src
	cmpl	%edx,%ecx
	jz	sm_90
sm_00:	movb	(%esi,%edx),%al
	cmpb	$0,%al
	jz	sm_90
	movb	%al,(%edi,%edx)
	incl	%edx
	cmpl	%edx,%ecx
	jnz	sm_00
sm_90:	movb	$0,(%edi,%edx)
sm_99:	lea	(%edi,%edx),%eax	# Return pointer to end null
	pop	%esi
	pop	%edi
	ret
.strmake_end:	
	.size strmake,.strmake_end-strmake

	# Move a string with max len chars
	# arg: dst,src,len
	# ret: pos to first null or dst+len

.globl strnmov
	.type strnmov,@function
strnmov:	
	pushl	%edi
	pushl	%esi
	movl	12(%esp),%edi		#  dst
	movl	16(%esp),%esi		#  src
	movl	20(%esp),%ecx		#  Length of memory-area
	jecxz	snm_99			#  Nothing to do
	clrb	%al			#  For test of end-null

snm_10:	cmpb	(%esi),%al		#  Next char to move
	movsb				#  move arg
	jz	snm_20			#  last char, fill with null
	loop	snm_10			#  Continue moving
	incl	%edi			#  Point two after last
snm_20:	decl	%edi			#  Point at first null (or last+1)
snm_99:	movl	%edi,%eax		#  Pointer at last char
	popl	%esi
	popl	%edi
	ret
.strnmov_end:	
	.size strnmov,.strnmov_end-strnmov

	
.globl strmov
	.type strmov,@function
strmov:	
	movl	%esi,%ecx		#  Save old %esi and %edi
	movl	%edi,%edx
	movl	8(%esp),%esi		#  get source pointer (s2)
	movl	4(%esp),%edi		#  %edi -> s1
smo_10:	movb	(%esi),%al
	movsb				#  move arg
	andb	%al,%al
	jnz	smo_10			#  Not last
	movl	%edi,%eax
	dec	%eax
	movl	%ecx,%esi		#  Restore
	movl	%edx,%edi
	ret
.strmov_end:	
	.size strmov,.strmov_end-strmov

.globl strxmov
	.type	 strxmov,@function
strxmov:
	movl	%ebx,%edx		#  Save %ebx, %esi and %edi
	mov	%esi,%ecx
	push	%edi
	leal	8(%esp),%ebx		#  Get destination
	movl	(%ebx),%edi
	xorb	%al,%al
	jmp	next_str		#  Handle source ebx+4

start_str:
	movsb
	cmpb	-1(%edi),%al
	jne	start_str
	decl	%edi			#  Don't copy last null

next_str:
	addl	$4,%ebx
	movl	(%ebx),%esi
	orl	%esi,%esi
	jne	start_str
	movb	%al,0(%edi)		#  Force last to ASCII 0

	movl	%edi,%eax		#  Return ptr to ASCII 0
	pop	%edi			#  Restore registers
	movl	%ecx,%esi
	movl	%edx,%ebx
	ret
.strxmov_end:
	.size	 strxmov,.strxmov_end-strxmov
