# Copyright (C) 2000 MySQL AB
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# Optimized longlong2str function for Intel 80x86  (gcc/gas syntax) 
# Some set sequences are optimized for pentuimpro II 

	.file	"longlong2str.s"
	.version "1.01"

.text
	.align 4

.globl	longlong2str
	.type	 longlong2str,@function
	
longlong2str:
	subl $80,%esp
	pushl %ebp
	pushl %esi
	pushl %edi
	pushl %ebx
	movl 100(%esp),%esi	# Lower part of val 
	movl 104(%esp),%ebp	# Higher part of val 
	movl 108(%esp),%edi	# get dst 
	movl 112(%esp),%ebx	# Radix 
	movl %ebx,%eax
	testl %eax,%eax
	jge .L144

	addl $36,%eax
	cmpl $34,%eax
	ja .Lerror		# Wrong radix 
	testl %ebp,%ebp
	jge .L146
	movb $45,(%edi)		# Add sign 
	incl %edi		# Change sign of val 
	negl %esi
	adcl $0,%ebp
	negl %ebp
.L146:
	negl %ebx		# Change radix to positive 
	jmp .L148
	.align 4
.L144:
	addl $-2,%eax
	cmpl $34,%eax
	ja .Lerror		# Radix in range 

.L148:
	movl %esi,%eax		# Test if zero (for easy loop) 
	orl %ebp,%eax
	jne .L150
	movb $48,(%edi)
	incl %edi
	jmp .L10_end
	.align 4

.L150:
	leal 92(%esp),%ecx	# End of buffer 
	jmp  .L155
	.align 4

.L153:
	# val is stored in in ebp:esi 

	movl %ebp,%eax		# High part of value 
	xorl %edx,%edx
	divl %ebx
	movl %eax,%ebp
	movl %esi,%eax
	divl %ebx
	decl %ecx
	movl %eax,%esi		# quotent in ebp:esi 
	movb _dig_vec_upper(%edx),%al   # al is faster than dl 
	movb %al,(%ecx)		# store value in buff 
	.align 4
.L155:
	testl %ebp,%ebp
	ja .L153
	testl %esi,%esi		# rest value 
	jl .L153
	je .L10_mov		# Ready 
	movl %esi,%eax
	movl $_dig_vec_upper,%ebp
	.align 4

.L154:				# Do rest with integer precision 
	cltd
	divl %ebx
	decl %ecx
	movb (%edx,%ebp),%dl	# bh is always zero as ebx=radix < 36 
	testl %eax,%eax
	movb %dl,(%ecx)
	jne .L154

.L10_mov:
	movl %ecx,%esi
	leal 92(%esp),%ecx	# End of buffer 
	subl %esi,%ecx
	rep
	movsb

.L10_end:
	movl %edi,%eax		# Pointer to end null 
	movb $0,(%edi)		# Store the end null 

.L165:
	popl %ebx
	popl %edi
	popl %esi
	popl %ebp
	addl $80,%esp
	ret

.Lerror:
	xorl %eax,%eax		# Wrong radix 
	jmp .L165

.Lfe3:
	.size	 longlong2str,.Lfe3-longlong2str

#
# This is almost equal to the above, except that we can do the final
# loop much more efficient	
#	

	.align 4
.Ltmp:
        .long 0xcccccccd
	.align 4
	
.globl	longlong10_to_str
	.type	 longlong10_to_str,@function
longlong10_to_str:
	subl $80,%esp
	pushl %ebp
	pushl %esi
	pushl %edi
	pushl %ebx
	movl 100(%esp),%esi	# Lower part of val 
	movl 104(%esp),%ebp	# Higher part of val 
	movl 108(%esp),%edi	# get dst 
	movl 112(%esp),%ebx	# Radix (10 or -10)
	testl %ebx,%ebx
	jge .L10_10		# Positive radix

	negl %ebx		# Change radix to positive (= 10)

	testl %ebp,%ebp		# Test if negative value
	jge .L10_10
	movb $45,(%edi)		# Add sign 
	incl %edi
	negl %esi		# Change sign of val (ebp:esi)
	adcl $0,%ebp
	negl %ebp
	.align 4

.L10_10:
	leal 92(%esp),%ecx	# End of buffer 
	movl %esi,%eax		# Test if zero (for easy loop) 
	orl %ebp,%eax
	jne .L10_30		# Not zero

	# Here when value is zero
	movb $48,(%edi)
	incl %edi
	jmp .L10_end
	.align 4

.L10_20:
	# val is stored in in ebp:esi 
	movl %ebp,%eax		# High part of value 
	xorl %edx,%edx
	divl %ebx		# Divide by 10
	movl %eax,%ebp
	movl %esi,%eax
	divl %ebx		# Divide by 10
	decl %ecx
	movl %eax,%esi		# quotent in ebp:esi 
	addl $48,%edx		# Convert to ascii
	movb %dl,(%ecx)		# store value in buff 

.L10_30:
	testl %ebp,%ebp
	ja .L10_20
	testl %esi,%esi		# rest value 
	jl .L10_20		# Unsigned, do ulonglong div once more
	je .L10_mov		# Ready
	movl %esi,%ebx		# Move val to %ebx

	# The following code uses some tricks to change division by 10 to
	# multiplication and shifts
	movl .Ltmp,%esi		# set %esi to 0xcccccccd
	
.L10_40:
        movl %ebx,%eax
        mull %esi
        decl %ecx
        shrl $3,%edx
        leal (%edx,%edx,4),%eax
        addl %eax,%eax
        subb %al,%bl		# %bl now contains val % 10
        addb $48,%bl
        movb %bl,(%ecx)
        movl %edx,%ebx
        testl %ebx,%ebx
	jne .L10_40
	jmp .L10_mov		# Shared end with longlong10_to_str

.L10end:
	.size	 longlong10_to_str,.L10end-longlong10_to_str
