; Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
; 
; This library is free software; you can redistribute it and/or
; modify it under the terms of the GNU Library General Public
; License as published by the Free Software Foundation; either
; version 2 of the License, or (at your option) any later version.
; 
; This library is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
; Library General Public License for more details.
; 
; You should have received a copy of the GNU Library General Public
; License along with this library; if not, write to the Free
; Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
; MA 02111-1307, USA

	TITLE   Optimized cmp of pointer to strings of unsigned chars

ifndef M_I386
	.8087
	DOSSEG
	.MODEL LARGE
	.DATA
compare_length dw	0
	.CODE STRINGS

	PUBLIC	_get_ptr_compare
_get_ptr_compare	PROC
	mov	bx,sp
	mov	cx,ss:[BX+4]
	mov	compare_length,cx
	mov	dx,seg strings:_ptr_cmp
	mov	ax,offset _ptr_cmp_0
	jcxz	@F
	mov	ax,offset _ptr_cmp_1
	dec	cx
	jz	@F
	mov	ax,offset _ptr_cmp
@@:	ret
_get_ptr_compare ENDP

_ptr_cmp_0	PROC
	mov	AX,0			; Emptyt strings are always equal
	ret
_ptr_cmp_0	ENDP


_ptr_cmp_1	PROC
	mov	bx,sp
	mov	dx,si			; Save si and ds
	mov	cx,ds
	lds	si,DWORD PTR ss:[bx+4]	; s1
	lds	si,DWORD PTR ds:[si]
	mov	al,ds:[si]
	xor	ah,ah
	lds	si,DWORD PTR ss:[bx+8]	; s2
	lds	si,DWORD PTR ds:[si]
	mov	bl,ds:[si]
	mov	bh,ah
	sub	ax,bx
	mov	ds,cx			; restore si and ds
	mov	si,dx
	ret
_ptr_cmp_1 ENDP

_ptr_cmp	PROC
	mov	bx,bp			; Save bp
	mov	dx,di			; Save di
	mov	bp,sp
	push	ds
	push	si
	mov	cx,compare_length	; Length of memory-area
	lds	si,DWORD PTR [bp+4]	; s1
	lds	si,DWORD PTR ds:[si]
	les	di,DWORD PTR [bp+8]	; s2
	les	di,DWORD PTR es:[di]
;	cld				; Work uppward
	xor	ax,ax
	repe	cmpsb			; Compare strings
	je	@F			; Strings are equal
	sbb	ax,ax
	cmc
	adc	ax,0

@@:	pop	si
	pop	ds
	mov	di,dx
	mov	bp,bx
	ret
_ptr_cmp ENDP

else

include macros.asm

fix_es  MACRO   fix_cld                 ; Load ES if neaded
  ife ESeqDS
        mov     ax,ds
        mov     es,ax
  endif
  ifnb <fix_cld>
        cld
  endif
        ENDM

	begdata
compare_length dd	0		; Length of strings
	enddata

	begcode get_ptr_compare
	public	_get_ptr_compare
_get_ptr_compare	proc near
	mov	ecx,P-SIZEPTR[esp]
	mov	compare_length,ecx
	mov	eax,offset _TEXT:_ptr_cmp_0
	jecxz	@F
	mov	eax,offset _TEXT:_ptr_cmp_1
	dec	ecx
	jz	@F
	mov	eax,offset _TEXT:_ptr_cmp
@@:	ret
_get_ptr_compare endp
	endcode _get_ptr_compare


	begcode	ptr_cmp_0
_ptr_cmp_0	PROC
	mov	EAX,0			; Emptyt strings are always equal
	ret
_ptr_cmp_0	ENDP
	endcode	ptr_cmp_0


	begcode	ptr_cmp_1
_ptr_cmp_1	proc near
	mov	edx,esi			; Save esi
	mov	esi,P-SIZEPTR[esp]	; *s1
	mov	esi,[esi]
	movzx	eax,[esi]
	mov	esi,P[esp]		; *s2
	mov	esi,[esi]
	movzx	ecx,[esi]
	sub	eax,ecx
	mov	esi,edx			; Restore esi
	ret
_ptr_cmp_1 ENDP
	endcode ptr_cmp_1


	begcode	ptr_cmp
_ptr_cmp	proc near
	fix_es	1
	push	ebp
	mov	ebp,esp
	mov	edx,edi			; Save esi
	push	esi
	mov	esi,P[ebp]		; *s1
	mov	esi,[esi]
	mov	edi,P+SIZEPTR[ebp]	; *s2
	mov	edi,[edi]
	mov	ecx,compare_length	; Length of memory-area
	xor	eax,eax
	repe	cmpsb			; Compare strings
	je	@F			; Strings are equal

	sbb	eax,eax
	cmc
	adc	eax,0

@@:	pop	esi
	mov	edi,edx
	pop	ebp
	ret
_ptr_cmp ENDP
	endcode	ptr_cmp

endif

	END
