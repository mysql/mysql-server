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

; Note that if you don't have a macro assembler (like MASM) to compile
; this file, you can instead compile all *.c files in the string
; directory.

	TITLE   Stringfunctions that we use often at MSDOS / Intel 8086

ifndef M_I386
	.8087
	DOSSEG
	.MODEL LARGE
	.CODE

	;
	; Some macros
	;

q_movs	MACRO				; as rep movsb but quicker
	shr	cx,1
	rep	movsw			; Move 2 bytes at a time
	adc	cx,cx
	rep	movsb			; Move last byte if any
	ENDM

q_stos	MACRO				; as rep stosb but quicker
	mov	ah,al			; For word store
	shr	cx,1
	rep	stosw			; Move 2 bytes at a time
	adc	cx,cx
	rep	stosb			; Move last byte if any
 	ENDM

ifndef  ZTC				; If not using ZORTECH compiler
	;
	; Compare memory
	; Args: s1,s2,length
	;

	PUBLIC	_bcmp
_bcmp	PROC
	mov	bx,bp			; Save bp
	mov	dx,di			; Save di
	mov	bp,sp
	push	ds
	push	si
	les	di,DWORD PTR [bp+8]	; s2
	lds	si,DWORD PTR [bp+4]	; s1
	mov	cx,WORD PTR [bp+12]	; Length of memory-area
	jcxz	@F			; Length = 0, return same
;	cld				; Work uppward
	repe	cmpsb			; Compare strings
	jz	@F			; Match found
	inc	cx			; return matchpoint +1
@@:	mov	ax,cx			; Return 0 if match, else pos from end
	pop	si
	pop	ds
	mov	di,dx
	mov	bp,bx
	ret
_bcmp	ENDP

	;
	; Find a char in a string
	; Arg: str,char
	; Ret: pointer to found char or NullS
	;

ifdef better_stringfunctions		; Breaks window linkage (broken linking)

	PUBLIC	_strchr
_strchr	PROC
	mov	bx,bp			; Save bp and di
	mov	dx,di
	mov	bp,sp
	les	di,DWORD PTR [bp+4]	; str
	mov	ah,BYTE PTR [bp+8]	; search
	xor	al,al			; for scasb to find end

@@:	cmp	ah,es:[di]
	jz	@F			; Found char
	scasb
	jnz	@B			; Not end
	xor	di,di			; Not found
	mov	es,di
@@:	mov	ax,di
	mov	di,dx			; Restore
	mov	dx,es			; Seg adr
	mov	bp,bx			; Restore
	ret
_strchr	ENDP

	;
	; Find length of string
	; arg: str
	; ret: length
	;

	PUBLIC	_strlen
_strlen	PROC
	mov	bx,sp
	mov	dx,di
	les	di,DWORD PTR ss:[bx+4]	; Str
	xor	al,al			; Find end of string
	mov	cx,-1
;	cld
	repne	scasb			; Find strend or length
	inc	cx			; Calc strlength
	not	cx
	mov	ax,cx
	mov	di,dx			; Restore register
	ret
_strlen	ENDP

endif

	;
	; Move a string
	; arg: dst,src
	; ret: end-null of to
	;

	PUBLIC	_strmov
_strmov	PROC
	mov	bx,bp
	mov	cx,si
	mov	bp,sp
	push	ds
	push	di
	les	di,DWORD PTR [bp+4]	; dst
	lds	si,DWORD PTR [bp+8]	; src
;	cld
@@:	mov	al,ds:[si]
	movsb				; move arg
	and	al,al
	jnz	@B			; Not last
	lea	ax,WORD PTR [di-1]	; Set DX:AX to point at last null
	mov	dx,es
	pop	di
	pop	ds
	mov	si,cx
	mov	bp,bx
	ret
_strmov	ENDP

	;
	; Fill a area of memory with a walue
	; Args: to,length,fillchar
	;

	PUBLIC	_bfill
_bfill	PROC
	mov	bx,sp			; Get args through BX
	mov	al,BYTE PTR ss:[bx+10]	; Fill
bfill_10:
	mov	dx,di			; Save di
	les	di,DWORD PTR ss:[bx+4]	; Memory pointer
	mov	cx,WORD PTR ss:[bx+8]	; Length
;	cld
	q_stos
	mov	di,dx
	ret
_bfill	ENDP

	;
	; Fill a area with null
	; Args: to,length

	PUBLIC	_bzero
_bzero	PROC
	mov	bx,sp			; Get args through BX
	mov	al,0			; Fill with null
	jmp	short bfill_10
_bzero	ENDP

endif	; ZTC

	;
	; Move a memory area
	; Args: to,from,length
	;

	PUBLIC	_bmove
_bmove	PROC
	mov	bx,bp
	mov	dx,di
	mov	ax,si
	mov	bp,sp
	push	ds
	lds	si,DWORD PTR [bp+8]	; from
	les	di,DWORD PTR [bp+4]	; to
	mov	cx,WORD PTR [bp+12]	; Length of memory-area
;	cld				; Work uppward
	rep	movsb			; Not q_movs because overlap ?
	pop	ds
	mov	si,ax
	mov	di,dx
	mov	bp,bx
	ret
_bmove	ENDP

	;
	; Move a alligned, not overlapped, by (long) divided memory area
	; Args: to,from,length
	;

	PUBLIC	_bmove_allign
_bmove_allign	PROC
	mov	bx,bp
	mov	dx,di
	mov	ax,si
	mov	bp,sp
	push	ds
	lds	si,DWORD PTR [bp+8]	; from
	les	di,DWORD PTR [bp+4]	; to
	mov	cx,WORD PTR [bp+12]	; Length of memory-area
;	cld				; Work uppward
	inc	cx			; fix if not divisible with word
	shr	cx,1
	rep	movsw			; Move 2 bytes at a time
	pop	ds
	mov	si,ax
	mov	di,dx
	mov	bp,bx
	ret
_bmove_allign	ENDP

	;
	; Move a string from higher to lower
	; Arg from+1,to+1,length
	;

	PUBLIC	_bmove_upp
_bmove_upp	PROC
	mov	bx,bp
	mov	dx,di
	mov	ax,si
	mov	bp,sp
	push	ds
	lds	si,DWORD PTR [bp+8]	; from
	les	di,DWORD PTR [bp+4]	; to
	mov	cx,WORD PTR [bp+12]	; Length of memory-area
	dec	di			; Don't move last arg
	dec	si
	std				; Work downward
	rep	movsb			; Not q_movs because overlap ?
	cld				; C compilator want cld
	pop	ds
	mov	si,ax
	mov	di,dx
	mov	bp,bx
	ret
_bmove_upp ENDP

	;
	; Append fillchars to string
	; Args: dest,len,fill
	;

	PUBLIC	_strappend
_strappend	PROC
	mov	bx,bp
	mov	dx,di
	mov	bp,sp
	les	di,DWORD PTR [bp+4]	; Memory pointer
	mov	cx,WORD PTR [bp+8]	; Length
	sub	al,al			; Find end of string
;	cld
	repne	scasb
	jnz	sa_99			; String to long, shorten it
	mov	al,BYTE PTR [bp+10]	; Fillchar
	dec	di			; Point at end null
	inc	cx			; rep made one dec for null-char
	q_stos				; Store al in string
sa_99:	mov	BYTE PTR es:[di],0	; End of string
	mov	di,dx
	mov	bp,bx
	ret
_strappend	ENDP

	;
	; Find if string contains any char in another string
	; Arg: str,set
	; Ret: Pointer to first found char in str
	;

	PUBLIC	_strcont
_strcont	PROC
	mov	bx,bp			; Save bp and di in regs
	mov	dx,di
	mov	bp,sp
	push	ds
	push	si
	lds	si,DWORD PTR [bp+4]	; str
	les	di,DWORD PTR [bp+8]	; Set
	mov	cx,di			; Save for loop
	xor	ah,ah			; For endtest
	jmp	sc_60

sc_10:	scasb
	jz	sc_fo			; Found char
sc_20:	cmp	ah,es:[di]		; Test if null
	jnz	sc_10			; Not end of set yet
	inc	si			; Next char in str
	mov	di,cx			; es:di = Set
sc_60:	mov	al,ds:[si]		; Test if this char exist
	and	al,al
	jnz	sc_20			; Not end of string
	sub	si,si			; Return Null
	mov	ds,si
sc_fo:	mov	ax,si			; Char found here
	mov	di,dx			; Restore
	mov	dx,ds			; Seg of found char
	pop	si
	pop	ds
	mov	bp,bx
	ret
_strcont	ENDP

	;
	; Found end of string
	; Arg: str
	; ret: Pointer to end null
	;

	PUBLIC	_strend
_strend	PROC
	mov	bx,sp
	mov	dx,di			; Save
	les	di,DWORD PTR ss:[bx+4]	; str
	mov	cx,-1
	sub	al,al			; Find end of string
;	cld
	repne	scasb
	lea	ax,WORD PTR [di-1]	; Endpos i DX:AX
	mov	di,dx			; Restore
	mov	dx,es
	ret
_strend	ENDP

	;
	; Make a string with len fill-chars and endnull
	; Args: dest,len,fill
	; Ret:  dest+len
	;

	PUBLIC	_strfill
_strfill	PROC
	mov	bx,bp			; Save sp
	mov	bp,sp
	push	di
	les	di,DWORD PTR [bp+4]	; Memory pointer
	mov	cx,WORD PTR [bp+8]	; Length
	mov	al,BYTE PTR [bp+10]	; Fill
;	cld
	q_stos
	mov	BYTE PTR es:[di],0	; End NULL
	mov	ax,di			; End i DX:AX
	mov	dx,es
	pop	di
	mov	bp,bx
	ret
_strfill	ENDP

	;
	; Find a char in or end of a string
	; Arg: str,char
	; Ret: pointer to found char or NullS
	;

	PUBLIC	_strcend
_strcend	PROC
	mov	bx,bp			; Save bp and di
	mov	dx,di
	mov	bp,sp
	les	di,DWORD PTR [bp+4]	; str
	mov	ah,BYTE PTR [bp+8]	; search
	xor	al,al			; for scasb to find end

@@:	cmp	ah,es:[di]
	jz	@F			; Found char
	scasb
	jnz	@B			; Not end
	dec 	di			; Not found, point at end of string
@@:	mov	ax,di
	mov	di,dx			; Restore
	mov	dx,es			; Seg adr
	mov	bp,bx			; Restore
	ret
_strcend	ENDP

	;
	; Test if string has a given suffix
	;

PUBLIC  _is_prefix
_is_prefix PROC
	mov	dx,di			; Save di
	mov	bx,sp			; Arguments through bx
	push	ds
	push	si
	les	di,DWORD PTR ss:[bx+8]	; s2
	lds	si,DWORD PTR ss:[bx+4]	; s1
	mov	ax,1			; Ok and zero-test
;	cld				; Work uppward
@@:	cmp	ah,es:[di]
	jz	suf_ok			; End of string; found suffix
	cmpsb				; Compare strings
	jz	@B			; Same, possible prefix
	xor	ax,ax			; Not suffix
suf_ok:	pop	si
	pop	ds
	mov	di,dx
	ret
_is_prefix ENDP

	;
	; Find a substring in string
	; Arg: str,search
	;

	PUBLIC	_strstr
_strstr	PROC
	mov	bx,bp
	mov	bp,sp
	push	ds
	push	di
	push	si
	lds	si,DWORD PTR [bp+4]	; str
	les	di,DWORD PTR [bp+8]	; search
	mov	cx,di
	inc	cx			; CX = search+1
	mov	ah,es:[di]		; AH = First char in search
	jmp	sf_10

sf_00:	mov	si,dx			; si = Current str-pos
sf_10:	mov	al,ds:[si]		; Test if this char exist
	and	al,al
	jz	sf_90			; End of string, didn't find search
	inc	si
	cmp	al,ah
	jnz	sf_10			; Didn't find first char, continue
	mov	dx,si			; Save str-pos in DX
	mov	di,cx
sf_20:	cmp	BYTE PTR es:[di],0
	jz	sf_fo			; Found substring
	cmpsb
	jz	sf_20			; Char ok
	jmp	sf_00			; Next str-pos

sf_90:	sub	dx,dx			; Return Null
	mov	ds,dx
	inc	dx			; Because of following dec
sf_fo:	mov	ax,dx			; Char found here
	dec	ax			; Pointed one after
	mov	dx,ds
	pop	si
	pop	di			; End
	pop	ds
	mov	bp,bx
	ret
_strstr	ENDP

	;
	; Find a substring in string, return index
	; Arg: str,search
	;

	PUBLIC	_strinstr
_strinstr	PROC
	push	bp
	mov	bp,sp
	push	di
	les	di,DWORD PTR [bp+10]	; search
	push	es
	push	di
	les	di,DWORD PTR [bp+6]	; str
	push	es
	push	di
	call	_strstr
	mov	cx,ax
	or	cx,dx
	jz	si_99
	sub	ax,di			; Pos from start
	inc	ax			; And first pos = 1
si_99:	add	sp,8
	pop	di
	pop	bp
	ret
_strinstr	ENDP

	;
	; Make a string of len length from another string
	; Arg: dst,src,length
	; ret: end of dst
	;

	PUBLIC	_strmake
_strmake	PROC
	mov	bx,bp
	mov	bp,sp
	push	ds
	push	di
	push	si
	les	di,DWORD PTR [bp+4]	; dst
	lds	si,DWORD PTR [bp+8]	; src
	mov	cx,WORD PTR [bp+12]	; Length of memory-area
	xor	al,al			; For test of end-null
	jcxz	sm_90			; Nothing to move, put zero at end.
;	cld				; Work uppward

@@:	cmp	al,ds:[si]		; Next char to move
	movsb				; move arg
	jz	sm_99			; last char, we are ready
	loop	@B			; Continue moving
sm_90:	mov	BYTE PTR es:[di],al	; Set end pos
	inc	di			; Fix that di points at end null
sm_99:	dec	di			; di points now at end null
	mov	ax,di			; Ret value in DX:AX
	mov	dx,es
	pop	si
	pop	di
	pop	ds
	mov	bp,bx
	ret
_strmake	ENDP

	;
	; Find length of string with maxlength
	; arg: str,maxlength
	; ret: length
	;

	PUBLIC	_strnlen
_strnlen	PROC
	mov	bx,bp
	mov	bp,sp
	push	di
	les	di,DWORD PTR [bp+4]	; Str
	mov	cx,WORD PTR [bp+8]	; length
	mov	dx,di			; Save str to calc length
	jcxz	sn_10			; Length = 0
	xor	al,al			; Find end of string
;	cld
	repne	scasb			; Find strend or length
	jnz	sn_10
	dec	di			; DI points at last null
sn_10:	mov	ax,di
	sub	ax,dx			; Ax = length
	pop	di
	mov	bp,bx
	ret
_strnlen	ENDP

	;
	; Move a string with max len chars
	; arg: dst,src,len
	; ret: pos to first null or dst+len

	PUBLIC	_strnmov
_strnmov	PROC
	mov	bx,bp
	mov	bp,sp
	push	ds
	push	di
	push	si
	les	di,DWORD PTR [bp+4]	; dst
	lds	si,DWORD PTR [bp+8]	; src
	mov	cx,WORD PTR [bp+12]	; length
	jcxz	snm_99			; Nothing to do
	xor	al,al			; For test of end-null
;	cld

@@:	cmp	al,ds:[si]		; Next char to move
	movsb				; move arg
	jz	snm_20			; last char, fill with null
	loop	@B			; Continue moving
	inc	di			; Point two after last
snm_20:	dec	di			; Point at first null (or last+1)
snm_99:	mov	ax,di			; Pointer at last char
	mov	dx,es			; To-segment
	pop	si
	pop	di
	pop	ds
	mov	bp,bx			; Restore
	ret
_strnmov	ENDP

else	; M_I386

include macros.asm

q_stos	MACRO				; as rep stosb but quicker, Uses edx
	mov	ah,al			;(2) Set up a 32 bit pattern.
	mov	edx,eax			;(2)
	shl	edx,16			;(3)
	or	eax,edx			;(2) EAX has the 32 bit pattern.

	mov	edx,ecx			;(2) Save the count of bytes.
	shr	ecx,2			;(2) Number of dwords.
	rep	stosd			;(5 + 5n)
	mov	cl,3			;(2)
	and	ecx,edx			;(2) Fill in the remaining odd bytes.
	rep	stosb			; Move last bytes if any
	ENDM

fix_es	MACRO	fix_cld			; Load ES if neaded
  ife ESeqDS
	mov	ax,ds
	mov	es,ax
  endif
  ifnb <fix_cld>
	cld
  endif
	ENDM

	;
	; Move a memory area
	; Args: to,from,length
	; Acts as one byte was moved a-time from dst to source.
	;

	begcode bmove
	public	_bmove
_bmove	proc near
	fix_es	1
	mov	edx,edi
	mov	eax,esi
	mov	edi,P-SIZEPTR[esp]	;p1
	mov	esi,P[esp]		;p2
	mov	ecx,P+SIZEPTR[esp]
	rep	movsb			; Not q_movs because overlap ?
	mov	esi,eax
	mov	edi,edx
	ret
_bmove	ENDP
	endcode bmove

	;
	; Move a alligned, not overlapped, by (long) divided memory area
	; Args: to,from,length
	;

	begcode	bmove_allign
	public	_bmove_allign
_bmove_allign	proc near
	fix_es	1
	mov	edx,edi
	mov	eax,esi
	mov	edi,P-SIZEPTR[esp]	;to
	mov	esi,P[esp]		;from
	mov	ecx,P+SIZEPTR[esp]	;length
	add	cx,3			;fix if not divisible with long
	shr	cx,2
	rep	movsd
	mov	esi,eax
	mov	edi,edx
	ret
_bmove_allign	ENDP
	endcode bmove_allign

	;
	; Move a string from higher to lower
	; Arg from+1,to+1,length
	;

	begcode	bmove_upp
	public	_bmove_upp
_bmove_upp	proc near
	fix_es
	std				; Work downward
	mov	edx,edi
	mov	eax,esi
	mov	edi,P-SIZEPTR[esp]	;p1
	mov	esi,P[esp]		;p2
	mov	ecx,P+SIZEPTR[esp]
	dec	edi			; Don't move last arg
	dec	esi
	rep	movsb			; One byte a time because overlap !
	cld				; C compilator wants cld
	mov	esi,eax
	mov	edi,edx
	ret
_bmove_upp ENDP
	endcode bmove_upp

	;
	; Append fillchars to string
	; Args: dest,len,fill
	;

	begcode	strappend
	public	_strappend
_strappend	proc near
	push	ebp
	mov	ebp,esp
	fix_es  1
	push	edi
	mov	edi,P[ebp]		; Memory pointer
	mov	ecx,P+SIZEPTR[ebp]	; Length
	clr	eax			; Find end of string
	repne	scasb
	jnz	sa_99			; String to long, shorten it
	movzx	eax,byte ptr P+(2*SIZEPTR)[ebp]	; Fillchar
	dec	edi			; Point at end null
	inc	ecx			; rep made one dec for null-char
	q_stos				; Store al in string
sa_99:	mov	BYTE PTR [edi],0	; End of string
	pop	edi
	pop	ebp
	ret
_strappend	ENDP
	endcode strappend

	;
	; Find if string contains any char in another string
	; Arg: str,set
	; Ret: Pointer to first found char in str
	;

	begcode strcont
	PUBLIC	_strcont
_strcont proc near
	push	ebp
	mov	ebp,esp
	fix_es	1
	mov	edx,edi
	push	esi
	mov	esi,P[ebp]		; str
	mov	ecx,P+SIZEPTR[ebp]	; Set
	clr	ah			; For endtest
	jmps	sc_60

sc_10:	scasb
	jz	sc_fo			; Found char
sc_20:	cmp	ah,[edi]		; Test if null
	jnz	sc_10			; Not end of set yet
	inc	esi			; Next char in str
sc_60:	mov	edi,ecx			; edi = Set
	mov	al,[esi]		; Test if this char exist
	and	al,al
	jnz	sc_20			; Not end of string
	clr	esi			; Return Null
sc_fo:	mov	eax,esi			; Char found here
	mov	edi,edx			; Restore
	pop	esi
	pop	ebp
	ret
_strcont	ENDP
	endcode strcont

	;
	; Found end of string
	; Arg: str
	; ret: Pointer to end null
	;

	begcode strend
	public	_strend
_strend	proc near
	fix_es	1
	mov	edx,edi			; Save
	mov	edi,P-SIZEPTR[esp]	; str
	clr	eax			; Find end of string
	mov	ecx,eax
	dec	ecx			; ECX = -1
	repne	scasb
	mov	eax,edi
	dec	eax
	mov	edi,edx			; Restore
	ret
_strend	endp
	endcode strend

	;
	; Make a string with len fill-chars and endnull
	; Args: dest,len,fill
	; Ret:  dest+len
	;

	begcode	strfill
	public	_strfill
_strfill proc near
	push	ebp
	mov	ebp,esp
	fix_es  1
	push	edi
	mov	edi,P[ebp]		; Memory pointer
	mov	ecx,P+SIZEPTR[ebp]	; Length
	movzx	eax,byte ptr P+(2*SIZEPTR)[ebp]	; Fill
	q_stos
	mov	BYTE PTR [edi],0	; End NULL
	mov	eax,edi			; End i DX:AX
	pop	edi
	pop	ebp
	ret
_strfill endp
	endcode strfill

	;
	; Find a char in or end of a string
	; Arg: str,char
	; Ret: pointer to found char or NullS
	;

	begcode strcend
	public	_strcend
_strcend proc near
	push	ebp
	mov	ebp,esp
	fix_es  1
	mov	edx,edi
	mov	edi,P[ebp]		; str
	mov	ah,P+SIZEPTR[ebp]	; search
	clr	al			; for scasb to find end

@@:	cmp	ah,[edi]
	jz	@F			; Found char
	scasb
	jnz	@B			; Not end
	dec 	edi			; Not found, point at end of string
@@:	mov	eax,edi
	mov	edi,edx			; Restore
	pop	ebp
	ret
_strcend	ENDP
	endcode strcend

	;
	; Test if string has a given suffix
	;

	begcode is_prefix
	public	_is_prefix
_is_prefix proc near
	fix_es	1
	mov	edx,edi			; Save edi
	mov	eax,esi			; Save esi
	mov	esi,P[esp]		; get suffix
	mov	edi,P-SIZEPTR[esp]	; s1
	push	eax			; push esi
	mov	eax,1			; Ok and zero-test
@@:	cmp	ah,[esi]
	jz	suf_ok			; End of string; found suffix
	cmpsb				; Compare strings
	jz	@B			; Same, possible prefix
	xor	eax,eax			; Not suffix
suf_ok:	pop	esi
	mov	edi,edx
	ret
_is_prefix endp
	endcode	_is_prefix

	;
	; Find a substring in string
	; Arg: str,search
	;

	begcode strstr
	public	_strstr
_strstr proc near
	push	ebp
	mov	ebp,esp
	fix_es	1
	push	EDI
	push	ESI
	mov	esi,P[ebp]		; str
	mov	edi,P+SIZEPTR[ebp]	; search
	mov	ecx,edi
	inc	ecx			; ECX = search+1
	mov	ah,[edi]		; AH = First char in search
	jmps	sf_10

sf_00:	mov	esi,edx			; si = Current str-pos
sf_10:	mov	al,[esi]		; Test if this char exist
	and	al,al
	jz	sf_90			; End of string, didn't find search
	inc	esi
	cmp	al,ah
	jnz	sf_10			; Didn't find first char, continue
	mov	edx,esi			; Save str-pos in EDX
	mov	edi,ecx
sf_20:	cmp	BYTE PTR [edi],0
	jz	sf_fo			; Found substring
	cmpsb
	jz	sf_20			; Char ok
	jmps	sf_00			; Next str-pos

sf_90:	mov	edx,1			; Return Null
sf_fo:	mov	eax,edx			; Char found here
	dec	eax			; Pointed one after
	pop	ESI
	pop	EDI
	pop	ebp
	ret
_strstr endp
	endcode strstr

	;
	; Find a substring in string, return index
	; Arg: str,search
	;

	begcode	strinstr
	public	_strinstr
_strinstr proc near
	push	ebp
	mov	ebp,esp
	push	P+SIZEPTR[ebp]		; search
	push	P[ebp]			; str
	call	_strstr
	add	esp,SIZEPTR*2
	or	eax,eax
	jz	si_99			; Not found, return NULL
	sub	eax,P[ebp]		; Pos from start
	inc	eax			; And first pos = 1
si_99:	pop	ebp
	ret
_strinstr	endp
	endcode strinstr

	;
	; Make a string of len length from another string
	; Arg: dst,src,length
	; ret: end of dst
	;

	begcode strmake
	public	_strmake
_strmake proc near
	push	ebp
	mov	ebp,esp
	fix_es	1
	push	EDI
	push	ESI
	mov	edi,P[ebp]		; dst
	mov	esi,P+SIZEPTR[ebp]	; src
	mov	ecx,P+SIZEPTR*2[ebp]	; Length of memory-area
	clr	al			; For test of end-null
	jcxz	sm_90			; Nothing to move, put zero at end.

@@:	cmp	al,[esi]		; Next char to move
	movsb				; move arg
	jz	sm_99			; last char, we are ready
	loop	@B			; Continue moving
sm_90:	mov	BYTE PTR [edi],al	; Set end pos
	inc	edi			; Fix that di points at end null
sm_99:	dec	edi			; di points now at end null
	mov	eax,edi			; Ret value in DX:AX
	pop	ESI
	pop	EDI
	pop	ebp
	ret
_strmake	ENDP
	endcode strmake

	;
	; Find length of string with maxlength
	; arg: str,maxlength
	; ret: length
	;

	begcode	strnlen
	public	_strnlen
_strnlen proc near
	push	ebp
	mov	ebp,esp
	fix_es	1
	push	edi
	mov	edi,P[ebp]		; Str
	mov	ecx,P+SIZEPTR[ebp]	; length
	mov	edx,edi			; Save str to calc length
	jcxz	sn_10			; Length = 0
	clr	al			; Find end of string
	repne	scasb			; Find strend or length
	jnz	sn_10
	dec	edi			; DI points at last null
sn_10:	mov	eax,edi
	sub	eax,edx			; Ax = length
	pop	edi
	pop	ebp
	ret
_strnlen	ENDP
	endcode strnlen

	;
	; Move a string with max len chars
	; arg: dst,src,len
	; ret: pos to first null or dst+len

	begcode	strnmov
	public	_strnmov
_strnmov PROC near
	push	ebp
	mov	ebp,esp
	fix_es	1
	push	EDI
	push	ESI
	mov	edi,P[ebp]		; dst
	mov	esi,P+SIZEPTR[ebp]	; src
	mov	ecx,P+(SIZEPTR*2)[ebp]	; length
	jcxz	snm_99			; Nothing to do
	clr	al			; For test of end-null

@@:	cmp	al,[esi]		; Next char to move
	movsb				; move arg
	jz	snm_20			; last char, fill with null
	loop	@B			; Continue moving
	inc	edi			; Point two after last
snm_20:	dec	edi			; Point at first null (or last+1)
snm_99:	mov	eax,edi			; Pointer at last char
	pop	ESI
	pop	EDI
	pop	ebp
	ret
_strnmov	ENDP
	endcode strnmov

;
; Zortech has this one in standard library
;

	begcode strmov
	public	_strmov
_strmov proc	near
	mov	ecx,esi			; Save old esi and edi
	mov	edx,edi
	mov	esi,P[esp]		; get source pointer (s2)
	mov	edi,P-SIZEPTR[esp]	; EDI -> s1
	fix_es	1
@@:	mov	al,[esi]
	movsb				; move arg
	and	al,al
	jnz	@B			; Not last
	mov	eax,edi
	dec	eax
	mov	esi,ecx			; Restore args
	mov	edi,edx
	ret
_strmov endp
	endcode strmov

endif ; M_I386

	END
