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

	TITLE   Optimized strxmov for MSDOS / Intel 8086

ifndef M_I386
	.8087
	DOSSEG
	.MODEL LARGE
	.CODE

	PUBLIC	_strxmov
_strxmov	PROC
	mov	bx,sp
	add	bx,4
	push	si
	push	di
	mov	cx,ds			; Save ds
ASSUME	DS:	NOTHING
ASSUME	ES:	NOTHING
	les	di,DWORD PTR ss:[bx]	; dst
	jmp	next_str

start_str:
	mov	al,ds:[si]
	movsb				; move arg
	and	al,al
	jnz	start_str		; Not last
	dec	di

next_str:
	add	bx,4
	lds	si,DWORD PTR ss:[bx]
	mov	ax,ds
	or	ax,si
	jnz	start_str

	mov	byte ptr es:[di],0	; Force end null (if no source)
	mov	ds,cx
	mov	ax,di			; Return ptr to last 0
	mov	dx,es
	pop	di
	pop	si
	ret
_strxmov	ENDP

else

include macros.asm

	begcode strxmov
	public	_strxmov

_strxmov	PROC near
ASSUME	DS:	NOTHING
ASSUME	ES:	NOTHING
		push	EBP
		mov	EBP,ESP
		mov	EDX,EBX		; Save EBX
		mov	ECX,ESI		; Save ESI
		push	EDI
		mov	EDI,8[EBP]	; Get destination
		lea	EBX,8[EBP]	; Get adress to first source - 4
		xor	al,al
		jmp	next_str

start_str:	movsb
		cmp	AL,[EDI-1]
		jne	start_str
		dec	EDI		; Don't copy last null

next_str:	add	EBX,4
		mov	ESI,[EBX]
		or	ESI,ESI
		jne	start_str
		mov	byte ptr [EDI],0	; Force last null

		mov	EAX,EDI		; Return ptr to null
		pop	EDI
		mov	ESI,ECX
		mov	EBX,EDX
		pop	EBP
		ret
_strxmov endp
	endcode strxmov

endif

	END
