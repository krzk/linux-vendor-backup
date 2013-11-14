/*
 * arch/arm/kernel/probes-arm.h
 *
 * Copyright 2013 Linaro Ltd.
 * Written by: David A. Long
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#ifndef _ARM_KERNEL_PROBES_THUMB_H
#define  _ARM_KERNEL_PROBES_THUMB_H

/*
 * True if current instruction is in an IT block.
 */
#define in_it_block(cpsr)	((cpsr & 0x06000c00) != 0x00000000)

/*
 * Return the condition code to check for the currently executing instruction.
 * This is in ITSTATE<7:4> which is in CPSR<15:12> but is only valid if
 * in_it_block returns true.
 */
#define current_cond(cpsr)	((cpsr >> 12) & 0xf)

enum probes_t32_action {
	PROBES_T32_EMULATE_NONE,
	PROBES_T32_SIMULATE_NOP,
	PROBES_T32_LDMSTM,
	PROBES_T32_LDRDSTRD,
	PROBES_T32_TABLE_BRANCH,
	PROBES_T32_TST,
	PROBES_T32_CMP,
	PROBES_T32_MOV,
	PROBES_T32_ADDSUB,
	PROBES_T32_LOGICAL,
	PROBES_T32_ADDWSUBW_PC,
	PROBES_T32_ADDWSUBW,
	PROBES_T32_MOVW,
	PROBES_T32_SAT,
	PROBES_T32_BITFIELD,
	PROBES_T32_SEV,
	PROBES_T32_WFE,
	PROBES_T32_MRS,
	PROBES_T32_BRANCH_COND,
	PROBES_T32_BRANCH,
	PROBES_T32_PLDI,
	PROBES_T32_LDR_LIT,
	PROBES_T32_LDRSTR,
	PROBES_T32_SIGN_EXTEND,
	PROBES_T32_MEDIA,
	PROBES_T32_REVERSE,
	PROBES_T32_MUL_ADD,
	PROBES_T32_MUL_ADD2,
	PROBES_T32_MUL_ADD_LONG
};

enum probes_t16_action {
	PROBES_T16_ADD_SP,
	PROBES_T16_CBZ,
	PROBES_T16_SIGN_EXTEND,
	PROBES_T16_PUSH,
	PROBES_T16_POP,
	PROBES_T16_SEV,
	PROBES_T16_WFE,
	PROBES_T16_IT,
	PROBES_T16_CMP,
	PROBES_T16_ADDSUB,
	PROBES_T16_LOGICAL,
	PROBES_T16_BLX,
	PROBES_T16_HIREGOPS,
	PROBES_T16_LDR_LIT,
	PROBES_T16_LDRHSTRH,
	PROBES_T16_LDRSTR,
	PROBES_T16_ADR,
	PROBES_T16_LDMSTM,
	PROBES_T16_BRANCH_COND,
	PROBES_T16_BRANCH
};

void __kprobes t16_simulate_bxblx(struct kprobe *p, struct pt_regs *regs);
void __kprobes t16_simulate_ldr_literal(struct kprobe *p, struct pt_regs *regs);
void __kprobes t16_simulate_ldrstr_sp_relative(struct kprobe *p,
		struct pt_regs *regs);
void __kprobes t16_simulate_reladr(struct kprobe *p, struct pt_regs *regs);
void __kprobes t16_simulate_add_sp_imm(struct kprobe *p, struct pt_regs *regs);
void __kprobes t16_simulate_cbz(struct kprobe *p, struct pt_regs *regs);
void __kprobes t16_simulate_it(struct kprobe *p, struct pt_regs *regs);
void __kprobes t16_singlestep_it(struct kprobe *p, struct pt_regs *regs);
enum kprobe_insn __kprobes t16_decode_it(kprobe_opcode_t insn,
	struct arch_specific_insn *asi);
void __kprobes t16_simulate_cond_branch(struct kprobe *p, struct pt_regs *regs);
enum kprobe_insn __kprobes t16_decode_cond_branch(kprobe_opcode_t insn,
	struct arch_specific_insn *asi);
void __kprobes t16_simulate_branch(struct kprobe *p, struct pt_regs *regs);
void __kprobes t16_emulate_loregs_rwflags(struct kprobe *p,
	struct pt_regs *regs);
void __kprobes t16_emulate_loregs_noitrwflags(struct kprobe *p,
	struct pt_regs *regs);
void __kprobes t16_emulate_hiregs(struct kprobe *p, struct pt_regs *regs);
enum kprobe_insn __kprobes t16_decode_hiregs(kprobe_opcode_t insn,
	struct arch_specific_insn *asi);
void __kprobes t16_emulate_push(struct kprobe *p, struct pt_regs *regs);
enum kprobe_insn __kprobes t16_decode_push(kprobe_opcode_t insn,
	struct arch_specific_insn *asi);
void __kprobes t16_emulate_pop_nopc(struct kprobe *p, struct pt_regs *regs);
void __kprobes t16_emulate_pop_pc(struct kprobe *p, struct pt_regs *regs);
enum kprobe_insn __kprobes t16_decode_pop(kprobe_opcode_t insn,
	struct arch_specific_insn *asi);

void __kprobes t32_simulate_table_branch(struct kprobe *p,
	struct pt_regs *regs);
void __kprobes t32_simulate_mrs(struct kprobe *p, struct pt_regs *regs);
void __kprobes t32_simulate_cond_branch(struct kprobe *p, struct pt_regs *regs);
enum kprobe_insn __kprobes t32_decode_cond_branch(kprobe_opcode_t insn,
	struct arch_specific_insn *asi);
void __kprobes t32_simulate_branch(struct kprobe *p, struct pt_regs *regs);
void __kprobes t32_simulate_ldr_literal(struct kprobe *p, struct pt_regs *regs);
enum kprobe_insn __kprobes t32_decode_ldmstm(kprobe_opcode_t insn,
	struct arch_specific_insn *asi);
void __kprobes t32_emulate_ldrdstrd(struct kprobe *p, struct pt_regs *regs);
void __kprobes t32_emulate_ldrstr(struct kprobe *p, struct pt_regs *regs);
void __kprobes t32_emulate_rd8rn16rm0_rwflags(struct kprobe *p,
	struct pt_regs *regs);
void __kprobes t32_emulate_rd8pc16_noflags(struct kprobe *p,
	struct pt_regs *regs);
void __kprobes t32_emulate_rd8rn16_noflags(struct kprobe *p,
	struct pt_regs *regs);
void __kprobes t32_emulate_rdlo12rdhi8rn16rm0_noflags(struct kprobe *p,
	struct pt_regs *regs);

#endif
