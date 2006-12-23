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

# Optimized longlong2str function for Intel 80x86  (gcc/gas syntax) 
# Some set sequences are optimized for pentuimpro II 

	.file	"longlong2str-x86.s"
	.version "1.02"

.text
	.align 4

.globl	longlong2str_with_dig_vector
	.type	 longlong2str_with_dig_vector,@function
	
longlong2str_with_dig_vector:
	subl  $80,%esp          # Temporary buffer for up to 64 radix-2 digits
	pushl %ebp
	pushl %esi
	pushl %edi
	pushl %ebx
	movl 100(%esp),%esi	# esi = Lower part of val 
	movl 112(%esp),%ebx	# ebx = Radix 
	movl 104(%esp),%ebp	# ebp = Higher part of val 
	movl 108(%esp),%edi	# edi = dst

	testl %ebx,%ebx
	jge .L144		# Radix was positive
	negl %ebx		# Change radix to positive 
	testl %ebp,%ebp		# Test if given value is negative
	jge .L144
	movb $45,(%edi)		# Add sign 
	incl %edi		# Change sign of val 
	negl %esi
	adcl $0,%ebp
	negl %ebp
	
.L144:	# Test that radix is between 2 and 36
	movl %ebx, %eax
	addl $-2,%eax		# Test that radix is between 2 and 36
	cmpl $34,%eax
	ja .Lerror		# Radix was not in range

	leal 92(%esp),%ecx	# End of buffer 
	movl %edi, 108(%esp)    # Store possible modified dest
	movl 116(%esp), %edi    # dig_vec_upper
	testl %ebp,%ebp		# Test if value > 0xFFFFFFFF
	jne .Llongdiv
	cmpl %ebx, %esi		# Test if <= radix, for easy loop
	movl %esi, %eax		# Value in eax (for Llow)
	jae .Llow

	# Value is one digit (negative or positive)
	movb (%eax,%edi),%bl
	movl 108(%esp),%edi	# get dst
	movb %bl,(%edi)
	incl %edi		# End null here
	jmp .L10_end

.Llongdiv:
	# Value in ebp:esi. div the high part by the radix,
        # then div remainder + low part by the radix.
	movl %ebp,%eax		# edx=0,eax=high(from ebp)
	xorl %edx,%edx
	decl %ecx
	divl %ebx
	movl %eax,%ebp		# edx=result of last, eax=low(from esi)
	movl %esi,%eax
	divl %ebx
	movl %eax,%esi		# ebp:esi = quotient
	movb (%edx,%edi),%dl	# Store result number in temporary buffer
	testl %ebp,%ebp
	movb %dl,(%ecx)		# store value in buff 
	ja .Llongdiv		# (Higher part of val still > 0)
	
	.align 4
.Llow:				# Do rest with integer precision 
	# Value in 0:eax. div 0 + low part by the radix.
	xorl  %edx,%edx
	decl %ecx
	divl %ebx
	movb (%edx,%edi),%dl	# bh is always zero as ebx=radix < 36 
	testl %eax,%eax
	movb %dl,(%ecx)
	jne .Llow

.L160:
	movl 108(%esp),%edi	# get dst 

.Lcopy_end:	
	leal 92(%esp),%esi	# End of buffer 
.Lmov:				# mov temporary buffer to result (%ecx -> %edi)
	movb (%ecx), %al
	movb %al, (%edi)
	incl %ecx
	incl %edi
	cmpl  %ecx,%esi
	jne  .Lmov

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
	.size	 longlong2str_with_dig_vector,.Lfe3-longlong2str_with_dig_vector

#
# This is almost equal to the above, except that we can do the final
# loop much more efficient	
#	

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

.L10_10:
	leal 92(%esp),%ecx	# End of buffer 
	testl %ebp,%ebp		# Test if value > 0xFFFFFFFF
	jne .L10_longdiv
	cmpl $10, %esi		# Test if <= radix, for easy loop
	movl %esi, %ebx		# Value in eax (for L10_low)
	jae .L10_low

	# Value is one digit (negative or positive)
	addb $48, %bl
	movb %bl,(%edi)
	incl %edi
	jmp .L10_end
	.align 4

.L10_longdiv:
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
	ja .L10_longdiv
	movl %esi,%ebx		# Move val to %ebx

.L10_low:
	# The following code uses some tricks to change division by 10 to
	# multiplication and shifts
	movl $0xcccccccd,%esi
		
.L10_40:			# Divide %ebx with 10
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
	jmp .Lcopy_end		# Shared end with longlong2str

.L10end:
	.size	 longlong10_to_str,.L10end-longlong10_to_str
