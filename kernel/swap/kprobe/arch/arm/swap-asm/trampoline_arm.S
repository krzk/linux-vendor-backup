/*
 * Function return probe trampoline:
 * 	- init_kprobes() establishes a probepoint here
 * 	- When the probed function returns, this probe
 * 		causes the handlers to fire
 */

		.global gen_insn_execbuf
gen_insn_execbuf:
		nop
		nop
		nop			//original instruction
		nop
		ldr	pc, [pc, #4]	//ssbreak 
		nop			//retbreak
		nop
		nop			//stored PC-4(next insn addr)



		.global pc_dep_insn_execbuf
pc_dep_insn_execbuf:
		str	r0, [sp, #-4]
		ldr	r0, [pc, #12]
		nop			// instruction with replaced PC
		ldr	r0, [sp, #-4]
		ldr	pc, [pc, #4]	//ssbreak
		nop			// retbreak
		nop			// stored PC
		nop			// stored PC-4 (next insn addr)

	.global b_r_insn_execbuf
b_r_insn_execbuf:
		nop			// bx, blx (Rm)
		ldr	pc, np1
		nop
		nop
		nop
		nop			// retbreak
		nop
np1:		nop			// stored PC-4 (next insn addr)

	.global b_cond_insn_execbuf
b_cond_insn_execbuf:
		beq	condway
		ldr	pc, np2
condway:	ldr	pc, bd2
		nop
		nop
		nop			// retbreak
bd2:		nop			// branch displacement
np2:		nop			// stored PC-4 (next insn addr)

		.global blx_off_insn_execbuf
blx_off_insn_execbuf:
		ldreq	lr, bd3
		blxeq	lr
		ldr	pc, np3
		nop
		nop
		nop			// retbreak
bd3:		nop			// branch displacement
np3:		nop			// stored PC-4 (next insn addr)
