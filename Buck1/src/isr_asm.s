; ******************************************************************************
; * © 2008 Microchip Technology Inc.
; *
; SOFTWARE LICENSE AGREEMENT:
; Microchip Technology Incorporated ("Microchip") retains all ownership and 
; intellectual property rights in the code accompanying this message and in all 
; derivatives hereto.  You may use this code, and any derivatives created by 
; any person or entity by or on your behalf, exclusively with Microchip's
; proprietary products.  Your acceptance and/or use of this code constitutes 
; agreement to the terms and conditions of this notice.
;
; CODE ACCOMPANYING THIS MESSAGE IS SUPPLIED BY MICROCHIP "AS IS".  NO 
; WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED 
; TO, IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A 
; PARTICULAR PURPOSE APPLY TO THIS CODE, ITS INTERACTION WITH MICROCHIP'S 
; PRODUCTS, COMBINATION WITH ANY OTHER PRODUCTS, OR USE IN ANY APPLICATION. 
;
; YOU ACKNOWLEDGE AND AGREE THAT, IN NO EVENT, SHALL MICROCHIP BE LIABLE, WHETHER 
; IN CONTRACT, WARRANTY, TORT (INCLUDING NEGLIGENCE OR BREACH OF STATUTORY DUTY), 
; STRICT LIABILITY, INDEMNITY, CONTRIBUTION, OR OTHERWISE, FOR ANY INDIRECT, SPECIAL, 
; PUNITIVE, EXEMPLARY, INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, FOR COST OR EXPENSE OF 
; ANY KIND WHATSOEVER RELATED TO THE CODE, HOWSOEVER CAUSED, EVEN IF MICROCHIP HAS BEEN 
; ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST EXTENT 
; ALLOWABLE BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY RELATED TO 
; THIS CODE, SHALL NOT EXCEED THE PRICE YOU PAID DIRECTLY TO MICROCHIP SPECIFICALLY TO 
; HAVE THIS CODE DEVELOPED.
;
; You agree that you are solely responsible for testing the code and 
; determining its suitability.  Microchip has no obligation to modify, test, 
; certify, or support the code.
;
; *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.equ __33FJ16GS502, 1
.include "p33FJ16GS502.inc"

; Buck 1 Maximum Duty cycle for voltage mode control
.equ Buck1MaxDC, 2024		; 7E8h	[ns]	; equiv to D~81%	;agp

; Buck 1 Minimum Duty cycle for voltage mode control
.equ Buck1MinDC, 72			; 48h	[ns]	; equiv to D~03%	;agp

.equ    offsetabcCoefficients, 0
.equ    offsetcontrolHistory, 2
.equ    offsetcontrolOutput, 4
.equ    offsetmeasuredOutput, 6
.equ    offsetcontrolReference, 8

.data
.text


.global __ADCP0Interrupt


__ADCP0Interrupt:
    push w0
    push w1
    push w2
    push w3

    mov #_Buck1VoltagePID, w0

    mov ADCBUF1, w1
    sl  w1, #5, w1						; Since only a 10-bit ADC shift left 
										; by 5 leaving the 16-bit 0 for a positive #

    mov w1, [w0+#offsetmeasuredOutput]
    call _PID  							; Call PID routine
    mov.w [w0+#offsetcontrolOutput], w1 ; Clamp PID output to allowed limits
    
    asr w1, #4, w0						; Scaling factor for voltage mode control

	mov.w #Buck1MinDC, w1				; Saturate to minimum duty cycle
	cpsgt w0, w1
	mov.w w1, w0
	mov.w #Buck1MaxDC, w1				; Saturate to maximum duty cycle
	cpslt w0, w1
	mov.w w1, w0

	mov.w w0, PDC1						; Update new Duty Cycle [ns]
    mov.w w0, TRIG1						; Update new trigger value to correspond to new duty cycle

	lsr w0, #1, w0						; divide w0 by two and overwrite w0
	mov.w w0, SEVTCMP					; update special event trigger counts = 1/2 PDC1	;agp

    mov ADCBUF0, w0					; read output curr
    mov PDC1, w4					; read on-time
;    lsr w3, #1, w3					; & scale down by 2
    mov 0x0858, w1					; read curr accum
	add.w w0, w1, w5				; add output curr to curr accum
	mov 0x0856, w2					; read ctr
    clr.w w0
    bset.b w0, #0
	add w2, w0, w2					; increment ctr
	sl w0, #10, w0					; set ctr limit, where 35ns is min ADC period (multiplied by ADCS value)
;	mov PDC1, w0					; set alt ctr limit (but must also divide accum by # of samples counted this instance)
									; so for ADCS = 6, ~30 samples per switching period are taken
	cpsgt w0, w2					; compare ctr to limit & if gtr
	mpy w4*w5, A				; mult on-time by output curr to get InputCurrentDC			
	cpsgt w0, w2					; compare ctr to limit & if gtr
	mov ACCAH, w4				; update average value for OutputCurrentDC			
	cpsgt w0, w2					; compare ctr to limit & if gtr
	mov w4, 0x085E				; update average value for OutputCurrentDC			
	cpsgt w0, w2					; compare ctr to limit & if gtr
	mov ACCAL, w5				; update average value for OutputCurrentDC			
	cpsgt w0, w2					; compare ctr to limit & if gtr
	mov w5, 0x085C				; update average value for OutputCurrentDC			
	cpsgt w0, w2					; compare ctr to limit & if gtr
	clr.w, w5						; reset curr accum
	mov.w w5, 0x0858				; save curr accum
	cpsgt w0, w2					; compare ctr to limit & if gtr
	clr.w, w2						; reset ctr
	mov.w w2, 0x0856				; save ctr

    bclr	ADSTAT,	#0					; Clear Pair 0 conversion status bit
	bclr 	IFS6, #14					; Clear Pair 0 Interrupt Flag

    pop w3
    pop w2
    pop w1
    pop w0
	retfie								; Return from interrupt

	.end
