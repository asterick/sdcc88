;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
;--------------------------------------------------------
	
	.optsdcc -ms1c88 sdcccall(1)
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _fb_keep
	.globl _fb_wr_full
	.globl _fb_wr_w12
	.globl _fb_wr_w12_lit
	.globl _fb_wr_s
	.globl _fb_wr_mid
	.globl _fb_wr_mid_lit
	.globl _fb_wr_b0
	.globl _fb_rd_obj
	.globl _fb_rd_full
	.globl _fb_rd_s10
	.globl _fb_rd_w12
	.globl _fb_rd_s_wide
	.globl _fb_rd_s
	.globl _fb_rd_mid
	.globl _fb_rd_b0
	.globl _isr_far
	.globl _l2f
	.globl _f2l
	.globl _pdiff
	.globl _lt
	.globl _ne0
	.globl _eq
	.globl _add_ui
	.globl _sub_si
	.globl _add_si
	.globl _dec_p
	.globl _inc_p
	.globl _use_ret
	.globl _ret_off
	.globl _ret_arr
	.globl _ret_tbl
	.globl _ret_p
	.globl _cast_f2n
	.globl _cast_n2f
	.globl _nrd
	.globl _chain
	.globl _rdp
	.globl _setp
	.globl _lit_vali
	.globl _lit_val
	.globl _lit_rd
	.globl _lit_wr
	.globl _tbl_wr
	.globl _tbl_rd
	.globl _glob_wri
	.globl _glob_rdi
	.globl _glob_wr
	.globl _glob_rd
	.globl _wro
	.globl _rdo
	.globl _wri
	.globl _wr1
	.globl _rdi
	.globl _rd1
	.globl _gfw
	.globl _gff
	.globl _fbuf
	.globl _ftbl
	.globl _lp
	.globl _np
	.globl _fp
	.globl _isr_sink
	.globl _gi
	.globl _g
	.globl _buf
	.globl _ip
	.globl _q
	.globl _p
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _DATA
_p::
	.ds 3
_q::
	.ds 3
_ip::
	.ds 3
_buf::
	.ds 2
_g::
	.ds 1
_gi::
	.ds 2
_isr_sink::
	.ds 1
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _INITIALIZED
_fp::
	.ds 3
_np::
	.ds 2
_lp::
	.ds 3
;--------------------------------------------------------
; uninitialized external ram data
;--------------------------------------------------------
	.area _FAR
_ftbl:
	.db #0x01	; 1
	.db #0x02	; 2
	.db #0x03	; 3
	.db #0x04	; 4
	.db #0x05	; 5
	.db #0x06	; 6
	.db #0x07	; 7
	.db #0x08	; 8
_fbuf:
	.ds 4
_gff:
	.db 0xdb
_gfw:
	.db 0xbc
	.db 0x0a
	.db 0x38
	.db 0x03
	.db 0x34
	.db 0x12
;--------------------------------------------------------
; absolute external ram data
;--------------------------------------------------------
	.area _DABS (ABS)
;--------------------------------------------------------
; global & static initialisations
;--------------------------------------------------------
	.area _HOME
	.area _GSINIT
	.area _GSFINAL
	.area _GSINIT
;--------------------------------------------------------
; Home
;--------------------------------------------------------
	.area _HOME
	.area _HOME
;--------------------------------------------------------
; code
;--------------------------------------------------------
	.area _CODE
;	---------------------------------
; Function rd1
; ---------------------------------
_rd1::
	ld	hl, (_p)
	ld	a, (_p+2)
	ld	ep, a
	ld	a, (hl)
	ld	ep, #0x00
	ret
;	---------------------------------
; Function rdi
; ---------------------------------
_rdi::
	ld	a, (_ip)
	ld	b, a
	ld	hl, (_ip + 1)
	ld	a, h
	ld	h, l
	ld	l, b
	ld	ep, a
	ld	a, (hl)
	inc	hl
	ld	b, (hl)
	ld	ep, #0x00
	ret
;	---------------------------------
; Function wr1
; ---------------------------------
_wr1::
	ld	b, a
	ld	hl, (_p)
	ld	a, (_p+2)
	ld	ep, a
	ld	(hl), b
	ld	ep, #0x00
	ret
;	---------------------------------
; Function wri
; ---------------------------------
_wri::
	push	ix
	ld	ix,sp
	push	hl
	ld	0 (sp), ba
	ld	a, (_ip)
	ld	b, a
	ld	hl, (_ip + 1)
	ld	a, h
	ld	h, l
	ld	l, b
	ld	ep, a
	ld	a, -2 (ix)
	ld	(hl), a
	inc	hl
	ld	a, -1 (ix)
	ld	(hl), a
	ld	ep, #0x00
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function rdo
; ---------------------------------
_rdo::
	ld	hl, (_p)
	ld	a, (_p+2)
	ld	ep, a
	add	hl, #3
	ld	a, (hl)
	ld	ep, #0x00
	ret
;	---------------------------------
; Function wro
; ---------------------------------
_wro::
	push	ix
	ld	ix,sp
	push	hl
	push	a
	ld	b, a
	ld	a, (_p+0)
	add	a, #0x05
	ld	-3 (ix), a
	ld	a, (_p+1)
	adc	a, #0x00
	ld	-2 (ix), a
	ld	a, (_p+2)
	adc	a, #0x00
	ld	-1 (ix), a
	ld	hl, 0 (sp)
	ld	a, -1 (ix)
	ld	ep, a
	ld	(hl), b
	ld	ep, #0x00
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function glob_rd
; ---------------------------------
_glob_rd::
	ld	hl, (_p)
	ld	a, (_p+2)
	ld	ep, a
	ld	a, (hl)
	ld	ep, #0x00
	ld	(_g), a
	ret
;	---------------------------------
; Function glob_wr
; ---------------------------------
_glob_wr::
	push	ix
	ld	ix,sp
	push	hl
	push	a
	ld	a, (_p+0)
	ld	-3 (ix), a
	ld	a, (_p+1)
	ld	-2 (ix), a
	ld	a, (_p+2)
	ld	-1 (ix), a
	ld	hl, 0 (sp)
	ld	a, -1 (ix)
	ld	ep, a
	ld	b, a
	ld	ep, #0x00
	ld	a, (_g)
	ex	a, b
	ld	ep, a
	ld	(hl), b
	ld	ep, #0x00
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function glob_rdi
; ---------------------------------
_glob_rdi::
	ld	a, (_ip)
	ld	b, a
	ld	hl, (_ip + 1)
	ld	a, h
	ld	h, l
	ld	l, b
	ld	ep, a
	ld	b, a
	ld	a, (hl)
	inc	hl
	ld	ep, #0x00
	ld	(_gi), a
	ld	a, b
	ld	ep, a
	ld	a, (hl)
	ld	ep, #0x00
	ld	(_gi + 1), a
	ret
;	---------------------------------
; Function glob_wri
; ---------------------------------
_glob_wri::
	push	ix
	ld	ix,sp
	push	hl
	push	a
	ld	a, (_ip+0)
	ld	-3 (ix), a
	ld	a, (_ip+1)
	ld	-2 (ix), a
	ld	a, (_ip+2)
	ld	-1 (ix), a
	ld	hl, 0 (sp)
	ld	a, -1 (ix)
	ld	ep, a
	ld	b, a
	ld	ep, #0x00
	ld	a, (_gi)
	ex	a, b
	ld	ep, a
	ld	(hl), b
	inc	hl
	ld	ep, #0x00
	ld	b, a
	ld	a, (_gi + 1)
	ex	a, b
	ld	ep, a
	ld	(hl), b
	ld	ep, #0x00
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function tbl_rd
; ---------------------------------
_tbl_rd::
	add	a,#<(_ftbl)
	ld	l, a
	ld	a, #>(_ftbl)
	adc	a, #0x00
	ld	h, a
	ld	a, #((_ftbl) >> 16)
	adc	a, #0x00
	ld	ep, a
	ld	a, (hl)
	ld	ep, #0x00
	ret
;	---------------------------------
; Function tbl_wr
; ---------------------------------
_tbl_wr::
	push	ix
	ld	ix,sp
	push	a
	ld	-1 (ix), a
	ld	b, l
	ld	a, #<(_fbuf)
	add	a, -1 (ix)
	ld	l, a
	ld	a, #>(_fbuf)
	adc	a, #0x00
	ld	h, a
	ld	a, #((_fbuf) >> 16)
	adc	a, #0x00
	ld	ep, a
	ld	(hl), b
	ld	ep, #0x00
	add	sp, #1
	pop	ix
	ret
;	---------------------------------
; Function lit_wr
; ---------------------------------
_lit_wr::
	ld	hl, #0x3456
	ld	a, #0x02
	ld	ep, a
	ld	(hl), #0x07
	ld	ep, #0x00
	ret
;	---------------------------------
; Function lit_rd
; ---------------------------------
_lit_rd::
	ld	hl, #0x3456
	ld	a, #0x02
	ld	ep, a
	ld	a, (hl)
	ld	ep, #0x00
	ret
;	---------------------------------
; Function lit_val
; ---------------------------------
_lit_val::
	ld	hl, (_p)
	ld	a, (_p+2)
	ld	ep, a
	ld	(hl), #0x2a
	ld	ep, #0x00
	ret
;	---------------------------------
; Function lit_vali
; ---------------------------------
_lit_vali::
	ld	a, (_ip)
	ld	b, a
	ld	hl, (_ip + 1)
	ld	a, h
	ld	h, l
	ld	l, b
	ld	ep, a
	ld	(hl), #0x34
	inc	hl
	ld	(hl), #0x12
	ld	ep, #0x00
	ret
;	---------------------------------
; Function setp
; ---------------------------------
_setp::
	ld	iy, sp
	add	iy, #3
	ld	a, 0 (iy)
	ld	(_p+0), a
	ld	a, 1 (iy)
	ld	(_p+1), a
	ld	a, 2 (iy)
	ld	(_p+2), a
	ret
;	---------------------------------
; Function rdp
; ---------------------------------
_rdp::
	push	ix
	ld	ix,sp
	ld	hl, 5 (sp)
	ld	a, 7 (ix)
	ld	ep, a
	ld	a, (hl)
	ld	ep, #0x00
	pop	ix
	ret
;	---------------------------------
; Function chain
; ---------------------------------
_chain::
	push	ix
	ld	ix,sp
	ld	a, 5 (ix)
	ld	(_p+0), a
	ld	a, 6 (ix)
	ld	(_p+1), a
	ld	a, 7 (ix)
	ld	(_p+2), a
	ld	hl, (_p)
	ld	a, (_p+2)
	ld	ep, a
	ld	(hl), #0x01
	ld	ep, #0x00
	pop	ix
	ret
;	---------------------------------
; Function nrd
; ---------------------------------
_nrd::
	ld	hl, (_np)
	ld	a, (hl)
	ret
;	---------------------------------
; Function cast_n2f
; ---------------------------------
_cast_n2f::
	ld	a, (_np+0)
	ld	(_p+0), a
	ld	a, (_np+1)
	ld	(_p+1), a
	xor	a, a
	ld	(_p+2), a
	ret
;	---------------------------------
; Function cast_f2n
; ---------------------------------
_cast_f2n::
	ld	hl, (_p)
	ld	(_np), hl
	ret
;	---------------------------------
; Function ret_p
; ---------------------------------
_ret_p::
	ld	hl, (_p)
	ld	a, (_p+2)
	ret
;	---------------------------------
; Function ret_tbl
; ---------------------------------
_ret_tbl::
	ld	hl, #_ftbl
	ld	a, #((_ftbl) >> 16)
	ret
;	---------------------------------
; Function ret_arr
; ---------------------------------
_ret_arr::
	ld	hl, #_buf
	ld	a, #((_buf) >> 16)
	ret
;	---------------------------------
; Function ret_off
; ---------------------------------
_ret_off::
	push	ix
	ld	ix,sp
	push	hl
	push	a
	ld	a, (_p+0)
	add	a, #0x02
	ld	-3 (ix), a
	ld	a, (_p+1)
	adc	a, #0x00
	ld	-2 (ix), a
	ld	a, (_p+2)
	adc	a, #0x00
	ld	-1 (ix), a
	ld	hl, 0 (sp)
	ld	a, -1 (ix)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function use_ret
; ---------------------------------
_use_ret::
	bcall	_ret_tbl
	ld	ep, a
	ld	a, (hl)
	ld	ep, #0x00
	ret
;	---------------------------------
; Function inc_p
; ---------------------------------
_inc_p::
	ld	a, (_p+0)
	inc	a
	ld	(_p+0), a
	jrs	NZ, 00103$
	ld	a, (_p+1)
	inc	a
	ld	(_p+1), a
	jrs	NZ, 00103$
	ld	a, (_p+2)
	inc	a
	ld	(_p+2), a
00103$:
	ret
;	---------------------------------
; Function dec_p
; ---------------------------------
_dec_p::
	ld	hl, #_p
	ld	a, (hl)
	add	a, #0xff
	ld	(hl), a
	inc	hl
	ld	a, (hl)
	adc	a, #0xff
	ld	(hl), a
	inc	hl
	ld	a, (hl)
	adc	a, #0xff
	ld	(hl), a
	ret
;	---------------------------------
; Function add_si
; ---------------------------------
_add_si::
	push	ix
	ld	ix,sp
	push	hl
	push	a
	ex	a, b
	ld	-3 (ix), b
	ld	-2 (ix), a
	rlc	a
	sbc	a, a
	ld	-1 (ix), a
	ld	hl, #_p
	ld	a, (hl)
	add	a, -3 (ix)
	ld	(hl), a
	inc	hl
	ld	a, (hl)
	adc	a, -2 (ix)
	ld	(hl), a
	inc	hl
	ld	a, (hl)
	adc	a, -1 (ix)
	ld	(hl), a
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function sub_si
; ---------------------------------
_sub_si::
	push	ix
	ld	ix,sp
	push	hl
	push	a
	ex	a, b
	ld	-3 (ix), b
	ld	-2 (ix), a
	rlc	a
	sbc	a, a
	ld	-1 (ix), a
	ld	hl, #_p
	ld	a, (hl)
	sub	a, -3 (ix)
	ld	(hl), a
	inc	hl
	ld	a, (hl)
	sbc	a, -2 (ix)
	ld	(hl), a
	inc	hl
	ld	a, (hl)
	sbc	a, -1 (ix)
	ld	(hl), a
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function add_ui
; ---------------------------------
_add_ui::
	push	ix
	ld	ix,sp
	push	hl
	ld	0 (sp), ba
	ld	hl, #_p
	ld	a, (hl)
	add	a, -2 (ix)
	ld	(hl), a
	inc	hl
	ld	a, (hl)
	adc	a, -1 (ix)
	ld	(hl), a
	jrs	NC, 00103$
	inc	hl
	inc	(hl)
00103$:
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function eq
; ---------------------------------
_eq::
	ld	a, (_p+0)
	ld	iy, #_q
	sub	a, 0 (iy)
	jrs	NZ, 00103$
	ld	a, (_p+1)
	sub	a, 1 (iy)
	jrs	NZ, 00103$
	ld	a, (_p+2)
	sub	a, 2 (iy)
	ld	a, #0x01
	jrs	Z, 00104$
00103$:
	xor	a, a
00104$:
	ret
;	---------------------------------
; Function ne0
; ---------------------------------
_ne0::
	ld	a, (_p+2)
	ld	iy, #_p
	or	a, 1 (iy)
	or	a, 0 (iy)
	sub	a, #0x01
	ld	a, #0x00
	rl	a
	xor	a, #0x01
	ret
;	---------------------------------
; Function lt
; ---------------------------------
_lt::
	ld	hl, #_q
	ld	a, (_p+0)
	sub	a, (hl)
	ld	a, (_p+1)
	inc	hl
	sbc	a, (hl)
	ld	a, (_p+2)
	inc	hl
	sbc	a, (hl)
	ld	a, #0x00
	rl	a
	ret
;	---------------------------------
; Function pdiff
; ---------------------------------
_pdiff::
	ld	hl, #_p
	ld	a, (_q+0)
	sub	a, (hl)
	ld	b, a
	ld	a, (_q+1)
	inc	hl
	sbc	a, (hl)
	ld	l, a
	ld	a, b
	ld	b, l
	ret
;	---------------------------------
; Function f2l
; ---------------------------------
_f2l::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	ld	a, (_p+0)
	ld	-4 (ix), a
	ld	a, (_p+1)
	ld	-3 (ix), a
	ld	a, (_p+2)
	ld	-2 (ix), a
	xor	a, a
	ld	-1 (ix), a
	ld	a, -4 (ix)
	ld	b, -3 (ix)
	ld	hl, 2 (sp)
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function l2f
; ---------------------------------
_l2f::
	push	a
	push	sc
	ld	a, b
	ld	h, l
	ld	l, a
	pop	sc
	pop	a
	ld	(_p+0), a
	ld	(_p + 1), hl
	ret
;	---------------------------------
; Function isr_far
; ---------------------------------
_isr_far::
_irq_v1::
	push	ba
	push	hl
	push	iy
	ld	a, ep
	push	a
	ld	ep, #0x00
	ld	hl, (_p)
	ld	a, (_p+2)
	ld	ep, a
	ld	a, (hl)
	ld	ep, #0x00
	ld	(_isr_sink), a
	pop	a
	ld	ep, a
	pop	iy
	pop	hl
	pop	ba
	rete
;	---------------------------------
; Function fb_rd_b0
; ---------------------------------
_fb_rd_b0::
	push	ix
	ld	ix,sp
	ld	hl, 5 (sp)
	ld	a, 7 (ix)
	ld	ep, a
	ld	a, (hl)
	ld	ep, #0x00
	and	a, #0x01
	pop	ix
	ret
;	---------------------------------
; Function fb_rd_mid
; ---------------------------------
_fb_rd_mid::
	push	ix
	ld	ix,sp
	ld	hl, 5 (sp)
	ld	a, 7 (ix)
	ld	ep, a
	ld	a, (hl)
	ld	ep, #0x00
	rrc	a
	and	a, #0x07
	pop	ix
	ret
;	---------------------------------
; Function fb_rd_s
; ---------------------------------
_fb_rd_s::
	push	ix
	ld	ix,sp
	ld	hl, 5 (sp)
	ld	a, 7 (ix)
	ld	ep, a
	ld	a, (hl)
	ld	ep, #0x00
	rlc	a
	rlc	a
	rlc	a
	rlc	a
	and	a, #0x0f
	bit	a, #0x08
	jrs	Z, 00103$
	or	a, #0xf0
00103$:
	pop	ix
	ret
;	---------------------------------
; Function fb_rd_s_wide
; ---------------------------------
_fb_rd_s_wide::
	push	ix
	ld	ix,sp
	ld	hl, 5 (sp)
	ld	a, 7 (ix)
	ld	ep, a
	ld	a, (hl)
	ld	ep, #0x00
	rlc	a
	rlc	a
	rlc	a
	rlc	a
	and	a, #0x0f
	bit	a, #0x08
	jrs	Z, 00103$
	or	a, #0xf0
00103$:
	ld	l, a
	rlc	a
	sbc	a, a
	ld	b, a
	ld	a, l
	pop	ix
	ret
;	---------------------------------
; Function fb_rd_w12
; ---------------------------------
_fb_rd_w12::
	push	ix
	ld	ix,sp
	ld	hl, 5 (sp)
	ld	a, 7 (ix)
	ld	ep, a
	ld	b, (hl)
	inc	hl
	ld	a, (hl)
	ld	ep, #0x00
	and	a, #0x0f
	ex	a, b
	pop	ix
	ret
;	---------------------------------
; Function fb_rd_s10
; ---------------------------------
_fb_rd_s10::
	push	ix
	ld	ix,sp
	ld	hl, 5 (sp)
	ld	a, 7 (ix)
	ld	ep, a
	add	hl, #2
	ld	b, (hl)
	inc	hl
	ld	a, (hl)
	ld	ep, #0x00
	and	a, #0x03
	bit	a, #0x02
	jrs	Z, 00103$
	or	a, #0xfc
00103$:
	ex	a, b
	pop	ix
	ret
;	---------------------------------
; Function fb_rd_full
; ---------------------------------
_fb_rd_full::
	push	ix
	ld	ix,sp
	ld	hl, 5 (sp)
	ld	a, 7 (ix)
	ld	ep, a
	add	hl, #4
	ld	b, (hl)
	inc	hl
	ld	a, (hl)
	ld	ep, #0x00
	ex	a, b
	pop	ix
	ret
;	---------------------------------
; Function fb_rd_obj
; ---------------------------------
_fb_rd_obj::
	ld	hl, #_gff
	ld	a, #((_gff) >> 16)
	ld	ep, a
	ld	a, (hl)
	ld	ep, #0x00
	rrc	a
	and	a, #0x07
	ret
;	---------------------------------
; Function fb_wr_b0
; ---------------------------------
_fb_wr_b0::
	push	ix
	ld	ix,sp
	ld	hl, 5 (sp)
	ld	a, 7 (ix)
	ld	ep, a
	ld	a, (hl)
	or	a, #0x01
	ld	(hl), a
	ld	ep, #0x00
	pop	ix
	ret
;	---------------------------------
; Function fb_wr_mid_lit
; ---------------------------------
_fb_wr_mid_lit::
	push	ix
	ld	ix,sp
	ld	hl, 5 (sp)
	ld	a, 7 (ix)
	ld	ep, a
	ld	a, (hl)
	and	a, #0xf1
	or	a, #0x0a
	ld	(hl), a
	ld	ep, #0x00
	pop	ix
	ret
;	---------------------------------
; Function fb_wr_mid
; ---------------------------------
_fb_wr_mid::
	push	ix
	ld	ix,sp
	push	hl
	push	a
	ld	b, a
	ld	a, 5 (ix)
	ld	-3 (ix), a
	ld	a, 6 (ix)
	ld	-2 (ix), a
	ld	a, 7 (ix)
	ld	-1 (ix), a
	ld	a, b
	rlc	a
	and	a, #0x0e
	ld	b, a
	ld	hl, 0 (sp)
	ld	a, -1 (ix)
	ld	ep, a
	ld	a, (hl)
	and	a, #0xf1
	or	a, b
	ld	(hl), a
	ld	ep, #0x00
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function fb_wr_s
; ---------------------------------
_fb_wr_s::
	push	ix
	ld	ix,sp
	push	hl
	push	a
	ld	b, a
	ld	a, 5 (ix)
	ld	-3 (ix), a
	ld	a, 6 (ix)
	ld	-2 (ix), a
	ld	a, 7 (ix)
	ld	-1 (ix), a
	ld	a, b
	add	a, a
	add	a, a
	add	a, a
	add	a, a
	ld	b, a
	ld	hl, 0 (sp)
	ld	a, -1 (ix)
	ld	ep, a
	ld	a, (hl)
	and	a, #0x0f
	or	a, b
	ld	(hl), a
	ld	ep, #0x00
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function fb_wr_w12_lit
; ---------------------------------
_fb_wr_w12_lit::
	push	ix
	ld	ix,sp
	ld	hl, 5 (sp)
	ld	b, 7 (ix)
	ld	a, b
	ld	ep, a
	ld	(hl), #0xab
	inc	hl
	ld	a, (hl)
	and	a, #0xf0
	or	a, #0x09
	ld	(hl), a
	ld	ep, #0x00
	pop	ix
	ret
;	---------------------------------
; Function fb_wr_w12
; ---------------------------------
_fb_wr_w12::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	a
	ld	3 (sp), ba
	ld	a, 5 (ix)
	ld	-5 (ix), a
	ld	a, 6 (ix)
	ld	-4 (ix), a
	ld	a, 7 (ix)
	ld	-3 (ix), a
	ld	a, -2 (ix)
	ld	b, -1 (ix)
	push	ba
	ld	hl, 2 (sp)
	ld	a, -3 (ix)
	ld	ep, a
	pop	ba
	ld	(hl), a
	inc	hl
	ld	a, b
	and	a, #0x0f
	ld	b, a
	ld	a, (hl)
	and	a, #0xf0
	or	a, b
	ld	(hl), a
	ld	ep, #0x00
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function fb_wr_full
; ---------------------------------
_fb_wr_full::
	push	ix
	ld	ix,sp
	push	hl
	push	hl
	push	a
	ld	3 (sp), ba
	ld	a, 5 (ix)
	ld	hl, 11 (sp)
	add	a, #0x04
	ld	-5 (ix), a
	ld	a, l
	adc	a, #0x00
	ld	-4 (ix), a
	ld	a, h
	adc	a, #0x00
	ld	-3 (ix), a
	ld	a, -2 (ix)
	ld	b, -1 (ix)
	push	ba
	ld	hl, 2 (sp)
	ld	a, -3 (ix)
	ld	ep, a
	pop	ba
	ld	(hl), a
	inc	hl
	ld	(hl), b
	ld	ep, #0x00
	ld	sp, ix
	pop	ix
	ret
;	---------------------------------
; Function fb_keep
; ---------------------------------
_fb_keep::
	push	ix
	ld	ix,sp
	ld	b, a
	ld	hl, 5 (sp)
	ld	a, 7 (ix)
	ld	ep, a
	ld	a, (hl)
	ld	ep, #0x00
	rrc	a
	and	a, #0x07
	add	a, b
	pop	ix
	ret
	.area _CODE
	.area _INITIALIZER
__xinit__fp:
	.byte _ftbl, (_ftbl >> 8), (_ftbl >> 16)
__xinit__np:
	.dw _buf
__xinit__lp:
; generic printIvalPtr
	.byte #0x45,#0x23,#0x01
	.area _CABS (ABS)
