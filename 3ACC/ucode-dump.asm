include '3acc.inc'

        ;;
	;; use maintenance channel commands to dump microcode from the
	;; offline 3acc
	;;
	;;
	;;
	;; send message MSTOP, 0321
	li	r0,0321				; 003401 000321
	bsa	smch				; 037020 xxxxxx

	;; load microaddr (r5) in MCHTR
	lsr	sr_mchtr,r5			; 107405
	;; send message LDMAR, 0115
	li	r0,0115
	bsa	smch

	;; send message RTNMMH
	;; mis	rtnmmh
	;; mis vfd 8,mchc%t 8,ib%f (??)
	mis	0160706,037020
	bsa	smchf

	;; send maintenance channel subroutine
	;; ripped from pr-1c956-50 CSYSUB
	;; r0 - mch command to send. see table on sd-1c900-b9cj.
	;; mchtr - data to be sent, if any
	;; r2,r3 - 20 bit result (high-4 in r2, low-16 in r3)
smch:
	mi					; 113400
	;; l	mchc,r0
	vfd	8,mchc%t 8,r0%f			; 160523
	;; ldstmch (mclstr)
	data	ldstmch%			; 134072
	;; stmch
	data	stmch%				; 134330
	;; zmint
	data	zmint%				; 026750
	unpk	mchb				; 112403
tmch:
	;;  first, block parity checking
	unpk	ss				; 012417
	com	r3				; 015463
	ni	r3,m(bin)			; 003462 000004
	ln	r2,e(s(bpc)-16)			; 003044
	pack	ss_s				; 012320
	;; then ??
	mi					; 013400
	vfd	8,cr%t 8,nop%f			; 031760
	tmch%					; 016730
	vfd	8,r0%t 8,cr%f			; 051463
	zmint%					; 026750
	;; correct the parity in r0
	ni	r0,r0				; 014000
	pack	ss_r				; 012360
	;; return 0 ?
	bnc

	;; ???
	;; todo: rip from cdgnm
	;; why is this used differently
smchf:
	
