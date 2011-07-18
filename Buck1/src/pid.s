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


        ; Local inclusions.
        .nolist
        .include        "dspcommon.inc"         ; fractsetup
        .list

        .equ    offsetabcCoefficients, 0
        .equ    offsetcontrolHistory, 2
        .equ    offsetcontrolOutput, 4
        .equ    offsetmeasuredOutput, 6
        .equ    offsetcontrolReference, 8

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

        .section .libdsp, code

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; _PID:
; Prototype:
;              tPID PID ( tPID *fooPIDStruct )
;
; Operation:
;
;                                             ----   Proportional
;                                            |    |  Output
;                             ---------------| Kp |-----------------
;                            |               |    |                 |
;                            |                ----                  |
;Reference                   |                                     ---
;Input         ---           |           --------------  Integral | + | Control   -------
;     --------| + |  Control |          |      Ki      | Output   |   | Output   |       |
;             |   |----------|----------| ------------ |----------|+  |----------| Plant |--
;        -----| - |Difference|          |  1 - Z^(-1)  |          |   |          |       |  |
;       |      ---  (error)  |           --------------           | + |           -------   |
;       |                    |                                     ---                      |
;       | Measured           |         -------------------  Deriv   |                       |
;       | Outut              |        |                   | Output  |                       |
;       |                     --------| Kd * (1 - Z^(-1)) |---------                        |
;       |                             |                   |                                 |
;       |                              -------------------                                  |
;       |                                                                                   |
;       |                                                                                   |
;        -----------------------------------------------------------------------------------
;
;   controlOutput[n] = controlOutput[n-1]
;                    + controlHistory[n] * abcCoefficients[0]
;                    + controlHistory[n-1] * abcCoefficients[1]
;                    + controlHistory[n-2] * abcCoefficients[2]
;
;  where:
;   abcCoefficients[0] = Kp + Ki + Kd
;   abcCoefficients[1] = -(Kp + 2*Kd)
;   abcCoefficients[2] = Kd
;   controlHistory[n] = measuredOutput[n] - referenceInput[n]
;  where:
;   abcCoefficients, controlHistory, controlOutput, measuredOutput and controlReference
;   are all members of the data structure tPID.
;
; Input:
;       w0 = Address of tPID data structure

; Return:
;       w0 = Address of tPID data structure
;
; System resources usage:
;       {w0..w5}        used, not restored
;       {w8,w10}        saved, used, restored
;        AccA, AccB     used, not restored
;        CORCON         saved, used, restored
;
; DO and REPEAT instruction usage.
;       0 level DO instruction
;       0 REPEAT intructions
;
; Program words (24-bit instructions):
;       28
;
; Cycles (including C-function call and return overheads):
;       30
;............................................................................

        .global _PID                    ; provide global scope to routine
_PID:
        ;btg LATD, #1
        ; Save working registers.
        push.s
        push    w4
        push    w5        
        push    w8
        push    w10

        
        push    CORCON                  ; Prepare CORCON for fractional computation.


        fractsetup      w8

        mov [w0 + #offsetabcCoefficients], w8    ; w8 = Base Address of _abcCoefficients array [(Kp+Ki+Kd), -(Kp+2Kd), Kd]
        mov [w0 + #offsetcontrolHistory], w10    ; w10 = Address of _ControlHistory array (state/delay line)

        mov [w0 + #offsetcontrolOutput], w1
        mov [w0 + #offsetmeasuredOutput], w2
        mov [w0 + #offsetcontrolReference], w3

        ; Calculate most recent error with saturation, no limit checking required
        lac     w3, a                   ; A = tPID.controlReference
        lac     w2, b                   ; B = tPID.MeasuredOutput
        sub     a                       ; A = tPID.controlReference - tPID.measuredOutput
        sac.r   a, [w10]                ; tPID.ControlHistory[n] = Sat(Rnd(A))

        ; Calculate PID Control Output
        clr     a, [w8]+=2, w4, [w10]+=2, w5            ; w4 = (Kp+Ki+Kd), w5 = _ControlHistory[n]
        lac     w1, a                                   ; A = ControlOutput[n-1]
        mac     w4*w5, a, [w8]+=2, w4, [w10]+=2, w5     ; A += (Kp+Ki+Kd) * _ControlHistory[n]
                                                        ; w4 = -(Kp+2Kd), w5 = _ControlHistory[n-1]
        mac     w4*w5, a, [w8], w4, [w10]-=2, w5        ; A += -(Kp+2Kd) * _ControlHistory[n-1]
                                                        ; w4 = Kd, w5 = _ControlHistory[n-2]
        mac     w4*w5, a, [w10]+=2, w5                  ; A += Kd * _ControlHistory[n-2]
                                                        ; w5 = _ControlHistory[n-1]
                                                        ; w10 = &_ControlHistory[n-2]
        sac.r   a, w1                                   ; ControlOutput[n] = Sat(Rnd(A))
        
        mov     w1, [w0 + #offsetcontrolOutput]

        ;Update the error history on the delay line
        mov     w5, [w10]               ; _ControlHistory[n-2] = _ControlHistory[n-1]
        mov     [w10 + #-4], w5         ; _ControlHistory[n-1] = ControlHistory[n]
        mov     w5, [--w10]

        pop     CORCON                  ; restore CORCON.
        pop     w10                     ; Restore working registers.
        pop     w8
        pop     w5
        pop     w4
        pop.s
       ; bclr LATD, #1        
        return

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; _PIDInit:
;
; Prototype:
; void PIDInit ( tPID *fooPIDStruct )
;
; Operation: This routine clears the delay line elements in the array
;            _ControlHistory, as well as clears the current PID output
;            element, _ControlOutput
;
; Input:
;       w0 = Address of data structure tPID (type defined in dsp.h)
;
; Return:
;       (void)
;
; System resources usage:
;       w0             used, restored
;
; DO and REPEAT instruction usage.
;       0 level DO instruction
;       0 REPEAT intructions
;
; Program words (24-bit instructions):
;       11
;
; Cycles (including C-function call and return overheads):
;       13
;............................................................................
        .global _PIDInit                ; provide global scope to routine
_PIDInit:
        push    w0
        add     #offsetcontrolOutput, w0 ;clear controlOutput
        clr     [w0]
	pop	w0
	push	w0
                                        ;Set up pointer to the base of
                                        ;controlHistory variables within tPID
        mov     [w0 + #offsetcontrolHistory], w0
                                        ; Clear controlHistory variables
                                        ; within tPID
        clr     [w0++]                  ; ControlHistory[n]=0
        clr     [w0++]                  ; ControlHistory[n-1] = 0
        clr     [w0]                    ; ControlHistory[n-2] = 0
        pop     w0                      ;Restore pointer to base of tPID
        return

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

        .end

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; OEF

