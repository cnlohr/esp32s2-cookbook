.section .vectors
.global __vectors

__vectors:
	rjmp startup
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec

startup:
	eor r1, r1
	out	0x3f, r1	; 63
	ldi	r28, 0x5F	; 95
	ldi	r29, 0x01	; 1
	out	0x3e, r29	; 62
	out	0x3d, r28	; 61
	rcall main

novec:
	rjmp novec

/*
Disassembly of section .text:

00000000 <__vectors>:
   0:	12 c0       	rjmp	.+36     	; 0x26 <__ctors_end>
   2:	24 c0       	rjmp	.+72     	; 0x4c <__bad_interrupt>
   4:	23 c0       	rjmp	.+70     	; 0x4c <__bad_interrupt>
   6:	22 c0       	rjmp	.+68     	; 0x4c <__bad_interrupt>
   8:	21 c0       	rjmp	.+66     	; 0x4c <__bad_interrupt>
   a:	20 c0       	rjmp	.+64     	; 0x4c <__bad_interrupt>
   c:	1f c0       	rjmp	.+62     	; 0x4c <__bad_interrupt>
   e:	1e c0       	rjmp	.+60     	; 0x4c <__bad_interrupt>
  10:	1d c0       	rjmp	.+58     	; 0x4c <__bad_interrupt>
  12:	1c c0       	rjmp	.+56     	; 0x4c <__bad_interrupt>
  14:	1b c0       	rjmp	.+54     	; 0x4c <__bad_interrupt>
  16:	1a c0       	rjmp	.+52     	; 0x4c <__bad_interrupt>
  18:	19 c0       	rjmp	.+50     	; 0x4c <__bad_interrupt>
  1a:	18 c0       	rjmp	.+48     	; 0x4c <__bad_interrupt>
  1c:	17 c0       	rjmp	.+46     	; 0x4c <__bad_interrupt>
  1e:	16 c0       	rjmp	.+44     	; 0x4c <__bad_interrupt>
  20:	15 c0       	rjmp	.+42     	; 0x4c <__bad_interrupt>
  22:	14 c0       	rjmp	.+40     	; 0x4c <__bad_interrupt>
  24:	13 c0       	rjmp	.+38     	; 0x4c <__bad_interrupt>

00000026 <__ctors_end>:
  26:	11 24       	eor	r1, r1
  28:	1f be       	out	0x3f, r1	; 63
  2a:	cf e5       	ldi	r28, 0x5F	; 95
  2c:	d1 e0       	ldi	r29, 0x01	; 1
  2e:	de bf       	out	0x3e, r29	; 62
  30:	cd bf       	out	0x3d, r28	; 61

00000032 <__do_copy_data>:
  32:	10 e0       	ldi	r17, 0x00	; 0
  34:	a0 e6       	ldi	r26, 0x60	; 96
  36:	b0 e0       	ldi	r27, 0x00	; 0
  38:	e6 e8       	ldi	r30, 0x86	; 134
  3a:	f0 e0       	ldi	r31, 0x00	; 0
  3c:	02 c0       	rjmp	.+4      	; 0x42 <__SREG__+0x3>
  3e:	05 90       	lpm	r0, Z+
  40:	0d 92       	st	X+, r0
  42:	a4 36       	cpi	r26, 0x64	; 100
  44:	b1 07       	cpc	r27, r17
  46:	d9 f7       	brne	.-10     	; 0x3e <__SP_H__>
  48:	02 d0       	rcall	.+4      	; 0x4e <main>
  4a:	1b c0       	rjmp	.+54     	; 0x82 <_exit>

0000004c <__bad_interrupt>:
  4c:	d9 cf       	rjmp	.-78     	; 0x0 <__vectors>


*/
