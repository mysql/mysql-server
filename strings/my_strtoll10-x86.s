# Copyright (C) 2003 MySQL AB
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307	USA

# Implemention of my_strtoll():  Converting a string to a 64 bit integer.
# For documentation, check my_strtoll.c
	
	.file	"my_strtoll10-x86.s"
	.version "01.02"
	
.text
	.align 4
	
.globl my_strtoll10
	.type	 my_strtoll10,@function


	# Used stack variables
	# ebp-4		dummy for storing endptr if endptr = 0
	# ebp-8		First 9 digits of return values
	# ebp-12	Pointer to first digit of second part
	# ebp-16	Store lowest 2 digits
	# ebp-20	!= 0 if value was negative
	# ebp-24	High max value
	# ebp-28	Middle max value
	# ebp-32	Low max value
	# ebp-36	Temp value

	# esi		Pointer to input string
	# ebx		End of string
	
my_strtoll10:
	pushl %ebp
	movl %esp,%ebp
	subl $48,%esp
	pushl %esi
	pushl %edi
	pushl %ebx
	movl 8(%ebp),%esi	# esi= nptr
	movl 16(%ebp),%ecx	# ecx= error (Will be overwritten later)
	movl 12(%ebp),%eax	# eax= endptr
	cmpl $0,%eax		# if (endptr)
	je .L110

# Fixed length string
	movl (%eax),%ebx	# bx= end-of-string
	.p2align 4,,7
.L100:
	cmpl %ebx,%esi
	je .Lno_conv
	movb (%esi), %al	# al= next byte
	incl %esi
	cmpb $32,%al		# Skip space
	je .L100
	cmpb $9,%al		# Skip tab
	je .L100
	jmp .L130

# String that ends with \0

.L110:
	leal -4(%ebp),%edi
	movl %edi,12(%ebp)	# endptr= &dummy, for easier end check
	.p2align 4,,7
.L120:
	movb (%esi), %al	# al= next byte
	incl %esi
	cmpb $32,%al
	je .L120
	cmpb $9,%al
	je .L120
	testb %al,%al		# Test if we found end \0
	je .Lno_conv
	leal 65535(%esi),%ebx	# ebx = end-of-string

.L130:
	cmpb $45,%al		# Test if '-'
	jne .Lpositive

	# negative number
	movl $-1,(%ecx)		# error = -1 (mark that number is negative)
	movl $1,-20(%ebp)	# negative= 1
	movl $92233720,-24(%ebp)
	movl $368547758,-28(%ebp)
	movl $8,-32(%ebp)
	jmp .L460

	.p2align 4,,7
.Lpositive:
	movl $0,(%ecx)		# error=0
	movl $0,-20(%ebp)	# negative= 0
	movl $184467440,-24(%ebp)
	movl $737095516,-28(%ebp)
	movl $15,-32(%ebp)
	cmpb $43,%al		# Check if '+'
	jne .L462

.L460:
	cmpl %ebx,%esi		# Check if overflow
	je .Lno_conv
	movb (%esi), %al	# al= next byte after sign
	incl %esi
		
	# Remove pre zero to be able to handle a lot of pre-zero
.L462:
	cmpb $48,%al
	jne .L475		# Number doesn't start with 0
	decl %esi
	.p2align 4,,7

	# Skip pre zeros
.L481:	
	incl %esi		# Skip processed byte
	cmpl %ebx,%esi
	je .Lms_return_zero
	cmpb (%esi),%al		# Test if next byte is also zero
	je .L481
	leal 9(%esi),%ecx	# ecx = end-of-current-part
	xorl %edi,%edi		# Store first 9 digits in edi
	jmp .L482
	.p2align 4,,7

	# Check if first char is a valid number
.L475:
	addb $-48,%al
	cmpb $9,%al
	ja .Lno_conv
.L477:	
	movzbl %al,%edi		# edi = first digit
	leal 8(%esi),%ecx	# ecx = end-of-current-part

	# Handle first 8/9 digits and store them in edi
.L482:
	cmpl %ebx,%ecx
	jbe .L522
	movl %ebx,%ecx		# ecx = min(end-of-current-part, end-of-string)
	jmp .L522

	.p2align 4,,7
.L488:
	movb (%esi), %al	# al= next byte
	incl %esi
	addb $-48,%al
	cmpb $9,%al
	ja .Lend_i_dec_esi

	# Calculate edi= edi*10 + al
	leal (%edi,%edi,4),%edx
	movzbl %al,%eax
	leal (%eax,%edx,2),%edi
.L522:
	cmpl %ecx,%esi		# If more digits at this level
	jne .L488
	cmpl %ebx,%esi		# If end of string
	je .Lend_i

	movl %edi,-8(%ebp)	# Store first 9 digits
	movl %esi,-12(%ebp)	# store pos to first digit of second part

	# Calculate next 9 digits and store them in edi

	xorl %edi,%edi
	leal 9(%esi),%ecx	# ecx= end-of-current-part
	movl %ecx,-36(%ebp)	# Store max length
	cmpl %ebx,%ecx
	jbe .L498
	movl %ebx,%ecx		# ecx = min(end-of-current-part, end-of-string)

	.p2align 4,,7
.L498:
	movb (%esi), %al	# al= next byte
	incl %esi
	addb $-48,%al
	cmpb $9,%al
	ja .Lend_i_and_j_decl_esi

	# Calculate edi= edi*10 + al
	leal (%edi,%edi,4),%edx
	movzbl %al,%eax
	leal (%eax,%edx,2),%edi

	cmpl %ecx,%esi		# If end of current part
	jne .L498
	cmpl %ebx,%esi		# If end of string
	jne .L500
	cmpl -36(%ebp),%esi	# Test if string is less than 18 digits
	jne .Lend_i_and_j
.L499:	
	movl $1000000000,%eax	
	jmp .Lgot_factor	# 18 digit string

	# Handle the possible next to last digit and store in ecx
.L500:
	movb (%esi),%al
	addb $-48,%al
	cmpb $9,%al
	ja .L499		# 18 digit string

	incl %esi
	movzbl %al,%ecx
	cmpl %ebx,%esi		# If end of string
	je .Lend4

	movb (%esi),%al		# Read last digit
	addb $-48,%al
	cmpb $9,%al
	ja .Lend4

	# ecx= ecx*10 + al
	leal (%ecx,%ecx,4),%edx
	movzbl %al,%eax
	leal (%eax,%edx,2),%ecx

	movl 12(%ebp),%eax	# eax = endptr
	incl %esi
	movl %esi,(%eax)	# *endptr = end-of-string
	cmpl %ebx,%esi
	je .L505		# At end of string

	movb (%esi),%al		# check if extra digits
	addb $-48,%al
	cmpb $9,%al
	jbe .Loverflow

	# At this point we have:
	# -8(%ebp)	First 9 digits
	# edi		Next 9 digits
	# ecx		Last 2 digits
	# *endpos	end-of-string
	
.L505:	# Check that we are not going to get overflow for unsigned long long
	movl -8(%ebp),%eax	# First 9 digits
	cmpl -24(%ebp),%eax
	ja .Loverflow
	jne .L507
	cmpl -28(%ebp),%edi
	ja .Loverflow
	jne .L507
	cmpl -32(%ebp),%ecx
	ja .Loverflow

.L507:
	movl %edi,-4(%ebp)	# Save middle bytes
	movl %ecx,%esi		# esi = 2 last digits
	movl $1215752192,%ecx	# %ecx= lower_32_bits(100000000000)
	mull %ecx
	imull $23,-8(%ebp),%ecx
	movl $0,-36(%ebp)
	movl %eax,%ebx
	imull $1215752192,-36(%ebp),%eax
	movl %edx,%edi
	addl %ecx,%edi
	addl %eax,%edi		# Temp in edi:ebx

	movl $100,%eax		# j= j*100
	mull -4(%ebp)
	addl %ebx,%eax		# edx:eax+= edi:ebx
	adcl %edi,%edx
	addl %esi,%eax
	adcl $0,%edx
	jmp .Lms_return

.Loverflow:
	# When we come here, *endptr is already updated

	movl 16(%ebp),%edx	# edx= error
	movl $34,(%edx)		# *error = 34
	movl $-1,%eax
	movl %eax,%edx
	cmpl $0,-20(%ebp)	# If negative
	je .Lms_return
	xor %eax,%eax		# edx:eax = LONGLONG_LMIN
	movl $-2147483648,%edx
	jmp .Lms_return

	# Return value that is in %edi as long long
	.p2align 4,,7
.Lend_i_dec_esi:
	decl %esi		# Fix so that it points at last digit
.Lend_i:
	xorl %edx,%edx
	movl %edi,%eax
	cmpl $0,-20(%ebp)
	je .Lreturn_save_endptr	# Positive number
	negl %eax
	cltd			# Neg result in edx:eax
	jmp .Lreturn_save_endptr

	# Return value (%ebp-8) * lfactor[(uint) (edx-start)] + edi
	.p2align 4,,7
.Lend_i_and_j_decl_esi:
	decl %esi		# Fix so that it points at last digit
.Lend_i_and_j:
	movl %esi,%ecx
	subl -12(%ebp),%ecx	# ecx= number of digits in second part

	# Calculate %eax= 10 ** %cl, where %cl <= 8
	# With an array one could do this with:
	# movl 10_factor_table(,%ecx,4),%eax
	# We calculate the table here to avoid problems in
	# position independent code (gcc -pic)

	cmpb  $3,%cl
	ja    .L4_to_8
	movl  $1000, %eax
	je    .Lgot_factor	# %cl=3, eax= 1000
	movl  $10, %eax
	cmpb  $1,%cl		# %cl is here 0 - 2
	je    .Lgot_factor	# %cl=1, eax= 10
	movl  $100, %eax	
	ja    .Lgot_factor	# %cl=2, eax=100
	movl  $1, %eax		
	jmp   .Lgot_factor	# %cl=0, eax=1

.L4_to_8:			# %cl is here 4-8
	cmpb  $5,%cl
	movl  $100000, %eax
	je   .Lgot_factor	# %cl=5, eax=100000
	movl  $10000, %eax
	jbe  .Lgot_factor	# %cl=4, eax=10000
	movl  $10000000, %eax
	cmpb  $7,%cl
	je   .Lgot_factor	# %cl=7, eax=10000000
	movl  $100000000, %eax	
	ja   .Lgot_factor	# %cl=8, eax=100000000
	movl  $1000000, %eax	# %cl=6, eax=1000000

	# Return -8(%ebp) * %eax + edi
	.p2align 4,,7
.Lgot_factor:
	mull -8(%ebp)
	addl %edi,%eax
	adcl $0,%edx
	cmpl $0,-20(%ebp)	# if negative
	je .Lreturn_save_endptr
	negl %eax		# Neg edx:%eax
	adcl $0,%edx
	negl %edx
	jmp .Lreturn_save_endptr

	# Return -8(%ebp) * $10000000000 + edi*10 + ecx
	.p2align 4,,7
.Lend4:
	movl %ecx,-16(%ebp)	# store lowest digits
	movl 12(%ebp),%ebx
	movl %esi,(%ebx)	# *endpos = end-of-string
	movl -8(%ebp),%eax	# First 9 digits
	movl $1410065408,%ecx	# ecx= lower_32_bits(10000000000)
	mull %ecx
	movl $0,-36(%ebp)
	movl %eax,%ebx		# Store lowest 32 byte from multiplication
	imull $1410065408,-36(%ebp),%eax
	movl -8(%ebp),%ecx	# First 9 digits
	movl %edx,%esi
	addl %ecx,%ecx
	addl %ecx,%esi
	addl %eax,%esi		# %esi:%ebx now has -8(%ebp) * $10000000000

	movl $10,%eax		# Calc edi*10
	mull %edi
	addl %ebx,%eax		# And add to result
	adcl %esi,%edx
	addl -16(%ebp),%eax	# Add lowest digit
	adcl $0,%edx
	cmpl $0,-20(%ebp)	# if negative
	je .Lms_return

	cmpl $-2147483648,%edx	# Test if too big signed integer
	ja .Loverflow
	jne .L516
	testl %eax,%eax
	ja .Loverflow

.L516:	
	negl %eax
	adcl $0,%edx
	negl %edx
	jmp .Lms_return

	.p2align 4,,7
.Lno_conv:			# Not a legal number
	movl 16(%ebp),%eax
	movl $33,(%eax)		# error= edom

.Lms_return_zero:
	xorl %eax,%eax		# Return zero
	xorl %edx,%edx

	.p2align 4,,7
.Lreturn_save_endptr:
	movl 12(%ebp),%ecx	# endptr= end-of-string
	movl %esi,(%ecx)	# *endptr= end-of-string

.Lms_return:
	popl %ebx
	popl %edi
	popl %esi
	movl %ebp,%esp
	popl %ebp
	ret

.my_strtoll10_end:
	.size	my_strtoll10,.my_strtoll10_end-my_strtoll10
        .comm   res,240,32
        .comm   end_ptr,120,32
        .comm   error,120,32
	.ident	"Monty"
