		.thumb

		.global gen_insn_execbuf_thumb
gen_insn_execbuf_thumb:
		nop
		nop
		nop			// original instruction
		nop			// original instruction
		nop
		nop
		nop
		sub	sp, sp, #8
		str	r0, [sp, #0]
		ldr	r0, [pc, #12]
		str	r0, [sp, #4]
		nop
		pop	{r0, pc}	// ssbreak
		nop			// retbreak
		nop
		nop
		nop			// stored PC-4(next insn addr) hi
		nop			// stored PC-4(next insn addr) lo

		nop

		.global pc_dep_insn_execbuf_thumb
		.align 4
pc_dep_insn_execbuf_thumb:
		push	{r6, r7}
		ldr	r6, i1
		mov	r7, sp
		mov	sp, r6
		nop			// PC -> SP
		nop			// PC -> SP
		mov	sp, r7
		pop	{r6, r7}
		push	{r0, r1}
		ldr	r0, i2
		nop
		str	r0, [sp, #4]
		pop	{r0, pc}	// ssbreak
		nop			// retbreak
i1:		nop			// stored PC hi
		nop			// stored PC lo
i2:		nop			// stored PC-4(next insn addr) hi
		nop			// stored PC-4(next insn addr) lo

		.global b_r_insn_execbuf_thumb
		.align 4
b_r_insn_execbuf_thumb:
                nop
                nop
                nop
                nop
                nop                     // bx,blx (Rm)
                nop                     //
                push {r0,r1}
                ldr r0, np
                nop
                str r0, [sp, #4]
                pop {r0,pc}
                nop
                nop                     // ssbreak
                nop                     // retbreak
                nop
                nop
np:             nop                     // stored PC-4(next insn addr) hi
                nop                     // stored PC-4(next insn addr) lo

		.global b_off_insn_execbuf_thumb
		.align 4
b_off_insn_execbuf_thumb:
                push {r0,r1}
                ldr r0, bd
                str r0, [sp, #4]
                pop {r0, pc}
                nop
                nop
                push {r0,r1}
                ldr r0, np2
                nop
                str r0, [sp, #4]
                pop {r0,pc}
                nop
                nop                     // ssbreak
                nop                     // retbreak
bd:		nop			// branch displacement hi
                nop			// branch displacement lo
np2:            nop                     // stored PC-4(next insn addr) hi
                nop                     // stored PC-4(next insn addr) lo

		.global blx_off_insn_execbuf_thumb
		.align 4
blx_off_insn_execbuf_thumb:
                push {r0}
                ldr r0, bd3
                mov lr, r0
                pop {r0}
                blx lr
                nop
                push {r0,r1}
                ldr r0, np3
                nop
                str r0, [sp, #4]
                pop {r0,pc}
                nop
                nop                     // ssbreak
                nop                     // retbreak
bd3:		nop			// branch displacement hi
                nop			// branch displacement lo
np3:            nop                     // stored PC-4(next insn addr) hi
                nop                     // stored PC-4(next insn addr) lo

		.global b_cond_insn_execbuf_thumb
		.align 4
b_cond_insn_execbuf_thumb:
		beq condway
                push {r0,r1}
                ldr r0, np4
                nop
                str r0, [sp, #4]
                pop {r0,pc}
condway:	push {r0,r1}
                ldr r0, bd4
                str r0, [sp, #4]
                pop {r0,pc}
                nop
                nop
                nop                     // ssbreak
                nop                     // retbreak
bd4:		nop			// branch displacement hi
                nop			// branch displacement lo
np4:            nop                     // stored PC-4(next insn addr) hi
                nop                     // stored PC-4(next insn addr) lo

		.global cbz_insn_execbuf_thumb
		.align 4
cbz_insn_execbuf_thumb:
		nop			// cbz
                push {r0,r1}
                ldr r0, np5
                nop
                str r0, [sp, #4]
                pop {r0,pc}
		push {r0,r1}
                ldr r0, bd5
                str r0, [sp, #4]
                pop {r0,pc}
                nop
                nop
                nop                     // ssbreak
                nop                     // retbreak
bd5:		nop			// branch displacement hi
                nop			// branch displacement lo
np5:            nop                     // stored PC-4(next insn addr) hi
                nop                     // stored PC-4(next insn addr) lo
