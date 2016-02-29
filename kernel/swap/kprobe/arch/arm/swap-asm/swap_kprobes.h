/**
 * @file kprobe/arch/asm-arm/swap_kprobes.h
 * @author Ekaterina Gorelkina <e.gorelkina@samsung.com>: initial implementation for ARM/MIPS
 * @author Alexey Gerenkov <a.gerenkov@samsung.com> User-Space Probes initial implementation; Support x86/ARM/MIPS for both user and kernel spaces.
 * @author Ekaterina Gorelkina <e.gorelkina@samsung.com>: redesign module for separating core and arch parts
 * @author Alexander Shirshikov <a.shirshikov@samsung.com>: initial implementation for Thumb
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * @section COPYRIGHT
 *
 * Copyright (C) Samsung Electronics, 2006-2010
 *
 * @section DESCRIPTION
 *
 * ARM arch-dependent kprobes interface declaration.
 */


#ifndef _SWAP_ASM_ARM_KPROBES_H
#define _SWAP_ASM_ARM_KPROBES_H

#include <linux/sched.h>
#include <linux/compiler.h>

typedef unsigned long kprobe_opcode_t;

#ifdef CONFIG_CPU_S3C2443
/** Breakpoint instruction */
#define BREAKPOINT_INSTRUCTION          0xe1200070
#else
/** Breakpoint instruction */
#define BREAKPOINT_INSTRUCTION          0xffffdeff
#endif /* CONFIG_CPU_S3C2443 */

#ifndef KPROBES_RET_PROBE_TRAMP

#ifdef CONFIG_CPU_S3C2443
/** Undefined instruction */
#define UNDEF_INSTRUCTION               0xe1200071
#else
/** Undefined instruction */
#define UNDEF_INSTRUCTION               0xfffffffe
#endif /* CONFIG_CPU_S3C2443 */

#endif /* KPROBES_RET_PROBE_TRAMP */

/** Maximum insn size */
#define MAX_INSN_SIZE                   1

/** Uprobes trampoline length */
#define UPROBES_TRAMP_LEN              9 * 4
/** Uprobes trampoline insn idx */
#define UPROBES_TRAMP_INSN_IDX         2
/** Uprobes trampoline ss break idx */
#define UPROBES_TRAMP_SS_BREAK_IDX     4
/** Uprobes trampoline ret break idx */
#define UPROBES_TRAMP_RET_BREAK_IDX    5
/** Kprobes trampoline length */
#define KPROBES_TRAMP_LEN              9 * 4
/** Kprobes trampoline insn idx */
# define KPROBES_TRAMP_INSN_IDX         UPROBES_TRAMP_INSN_IDX
/** Kprobes trampoline ss break idx */
# define KPROBES_TRAMP_SS_BREAK_IDX     UPROBES_TRAMP_SS_BREAK_IDX

/* TODO: remove (not needed for kprobe) */
# define KPROBES_TRAMP_RET_BREAK_IDX	UPROBES_TRAMP_RET_BREAK_IDX

/** User register offset */
#define UREGS_OFFSET 8

/**
 * @struct prev_kprobe
 * @brief Stores previous kprobe.
 * @var prev_kprobe::kp
 * Pointer to kprobe struct.
 * @var prev_kprobe::status
 * Kprobe status.
 */
struct prev_kprobe {
	struct kprobe *kp;
	unsigned long status;
};

/**
 * @brief Gets task pc.
 *
 * @param p Pointer to task_struct 
 * @return Value in pc.
 */
static inline unsigned long arch_get_task_pc(struct task_struct *p)
{
	return task_thread_info(p)->cpu_context.pc;
}

/**
 * @brief Sets task pc.
 *
 * @param p Pointer to task_struct.
 * @param val Value that should be set.
 * @return Void.
 */
static inline void arch_set_task_pc(struct task_struct *p, unsigned long val)
{
	task_thread_info(p)->cpu_context.pc = val;
}

/**
 * @brief Gets syscall registers.
 *
 * @param sp Pointer to stack.
 * @return Pointer to CPU regs data.
 */
static inline struct pt_regs *swap_get_syscall_uregs(unsigned long sp)
{
	return (struct pt_regs *)(sp + UREGS_OFFSET);
}

/**
 * @brief Gets stack pointer.
 *
 * @param regs Pointer to CPU registers data.
 * @return Stack address.
 */
static inline unsigned long swap_get_stack_ptr(struct pt_regs *regs)
{
	return regs->ARM_sp;
}

/**
 * @brief Gets instruction pointer.
 *
 * @param regs Pointer to CPU registers data.
 * @return Pointer to pc.
 */
static inline unsigned long swap_get_instr_ptr(struct pt_regs *regs)
{
	return regs->ARM_pc;
}

/**
 * @brief Sets instruction pointer.
 *
 * @param regs Pointer to CPU registers data.
 * @param val Address that should be stored in pc.
 * @return Void.
 */
static inline void swap_set_instr_ptr(struct pt_regs *regs, unsigned long val)
{
	regs->ARM_pc = val;
}

/**
 * @brief Gets return address.
 *
 * @param regs Pointer to CPU registers data.
 * @return Return address.
 */
static inline unsigned long swap_get_ret_addr(struct pt_regs *regs)
{
	return regs->ARM_lr;
}

/**
 * @brief Sets return address.
 *
 * @param regs Pointer to CPU registers data.
 * @param val New return address.
 * @return Void.
 */
static inline void swap_set_ret_addr(struct pt_regs *regs, unsigned long val)
{
	regs->ARM_lr = val;
}

/**
 * @brief Gets specified argument.
 *
 * @param regs Pointer to CPU registers data.
 * @param num Number of the argument.
 * @return Argument value.
 */
static inline unsigned long swap_get_arg(struct pt_regs *regs, int num)
{
	return regs->uregs[num];
}

/**
 * @brief Sets specified argument.
 *
 * @param regs Pointer to CPU registers data.
 * @param num Number of the argument.
 * @param val New argument value.
 * @return Void.
 */
static inline void swap_set_arg(struct pt_regs *regs, int num,
				unsigned long val)
{
	regs->uregs[num] = val;
}

// undefined
# define MASK_ARM_INSN_UNDEF		0x0FF00000		// xxxx1111 1111xxxx xxxxxxxx xxxxxxxx
# define PTRN_ARM_INSN_UNDEF		0x03000000		// cccc0011 0000xxxx xxxxxxxx xxxxxxxx

# define MASK_THUMB_INSN_UNDEF		0xFE00			// 11111111xxxxxxxx
# define PTRN_THUMB_INSN_UNDEF		0xDE00			// 11011110xxxxxxxx

// architecturally undefined
# define MASK_ARM_INSN_AUNDEF           0x0FF000F0
# define PTRN_ARM_INSN_AUNDEF           0x07F000F0

// branches
# define MASK_ARM_INSN_B		0x0F000000		// xxxx1111xxxxxxxxxxxxxxxxxxxxxxxx
# define PTRN_ARM_INSN_B		0x0A000000		// cccc1010xxxxxxxxxxxxxxxxxxxxxxxx

# define MASK_THUMB_INSN_B1		0xF000			// 1111xxxxxxxxxxxx
# define PTRN_THUMB_INSN_B1		0xD000			// 1101xxxxxxxxxxxx						// b<cond> label

# define MASK_THUMB_INSN_B2		0xF800			// 11111xxxxxxxxxxx
# define PTRN_THUMB_INSN_B2		0xE000			// 11100xxxxxxxxxxx						// b label

# define MASK_THUMB_INSN_CBZ		0xF500			// 1111x1x1xxxxxxxx
# define PTRN_THUMB_INSN_CBZ		0xB100			// 1011x0x1xxxxxxxx						// CBZ/CBNZ

# define MASK_THUMB2_INSN_B1		0xD000F800		// 11x1xxxxxxxxxxxx 11111xxxxxxxxxxx				// swapped
# define PTRN_THUMB2_INSN_B1		0x8000F000		// 10x0xxxxxxxxxxxx 11110xxxxxxxxxxx				// swapped

# define MASK_THUMB2_INSN_B2		0xD000F800		// 11x1xxxxxxxxxxxx 11111xxxxxxxxxxx				// swapped
# define PTRN_THUMB2_INSN_B2		0x9000F000		// 10x1xxxxxxxxxxxx 11110xxxxxxxxxxx				// swapped

# define MASK_ARM_INSN_BL		0x0F000000		// xxxx1111xxxxxxxxxxxxxxxxxxxxxxxx
# define PTRN_ARM_INSN_BL		0x0B000000		// cccc1011xxxxxxxxxxxxxxxxxxxxxxxx

//# define MASK_THUMB_INSN_BL		0xF800			// 11111xxxxxxxxxxx
//# define PTRN_THUMB_INSN_BL		0xF000			// 11110xxxxxxxxxxx						// shared between BL and BLX
//# define PTRN_THUMB_INSN_BL		0xF800			// 11111xxxxxxxxxxx

# define MASK_THUMB2_INSN_BL		0xD000F800		// 11x1xxxxxxxxxxxx 11111xxxxxxxxxxx				// swapped
# define PTRN_THUMB2_INSN_BL		0xD000F000		// 11x1xxxxxxxxxxxx 11110xxxxxxxxxxx				// bl imm  swapped

# define MASK_ARM_INSN_BLX1		0xFE000000		// 1111111axxxxxxxxxxxxxxxxxxxxxxxx
# define PTRN_ARM_INSN_BLX1		0xFA000000		// 1111101axxxxxxxxxxxxxxxxxxxxxxxx

//# define MASK_THUMB_INSN_BLX1		0xF800			// 11111xxxxxxxxxxx						/ blx imm
//# define PTRN_THUMB_INSN_BLX1		0xF000			// 11101xxxxxxxxxxx

# define MASK_THUMB2_INSN_BLX1		0xD001F800		// 11x1xxxxxxxxxxx1 11111xxxxxxxxxxx				// swapped
# define PTRN_THUMB2_INSN_BLX1		0xC000F000		// 11x0xxxxxxxxxxx0 11110xxxxxxxxxxx				// swapped

# define MASK_ARM_INSN_BLX2		0x0FF000F0		// xxxx11111111xxxxxxxxxxxx1111xxxx
# define PTRN_ARM_INSN_BLX2		0x01200030		// cccc00010010xxxxxxxxxxxx0011xxxx

# define MASK_THUMB_INSN_BLX2		0xFF80			// 111111111xxxxxxx						/ blx reg
# define PTRN_THUMB_INSN_BLX2		0x4780			// 010001111xxxxxxx

# define MASK_ARM_INSN_BX		0x0FF000F0		// cccc11111111xxxxxxxxxxxx1111xxxx
# define PTRN_ARM_INSN_BX		0x01200010		// cccc00010010xxxxxxxxxxxx0001xxxx

# define MASK_THUMB_INSN_BX		0xFF80			// 111111111xxxxxxx
# define PTRN_THUMB_INSN_BX		0x4700			// 010001110xxxxxxx

# define MASK_ARM_INSN_BXJ		0x0FF000F0		// xxxx11111111xxxxxxxxxxxx1111xxxx
# define PTRN_ARM_INSN_BXJ		0x01200020		// cccc00010010xxxxxxxxxxxx0010xxxx

# define MASK_THUMB2_INSN_BXJ		0xD000FFF0		// 11x1xxxxxxxxxxxx 111111111111xxxx				// swapped
# define PTRN_THUMB2_INSN_BXJ		0x8000F3C0		// 10x0xxxxxxxxxxxx 111100111100xxxx				// swapped


// software interrupts
# define MASK_ARM_INSN_SWI		0x0F000000		// cccc1111xxxxxxxxxxxxxxxxxxxxxxxx
# define PTRN_ARM_INSN_SWI		0x0F000000		// cccc1111xxxxxxxxxxxxxxxxxxxxxxxx

# define MASK_THUMB_INSN_SWI		0xFF00			// 11111111xxxxxxxx
# define PTRN_THUMB_INSN_SWI		0xDF00			// 11011111xxxxxxxx

// break
# define MASK_ARM_INSN_BREAK		0xFFF000F0		// 111111111111xxxxxxxxxxxx1111xxxx
# define PTRN_ARM_INSN_BREAK		0xE1200070		// 111000010010xxxxxxxxxxxx0111xxxx				/? A8-56 ARM DDI 046B if cond != ‘1110’ then UNPREDICTABLE;

# define MASK_THUMB_INSN_BREAK		0xFF00			// 11111111xxxxxxxx
# define PTRN_THUMB_INSN_BREAK		0xBE00			// 10111110xxxxxxxx

// CLZ
# define MASK_ARM_INSN_CLZ		0x0FFF0FF0		// xxxx111111111111xxxx11111111xxxx
# define PTRN_ARM_INSN_CLZ		0x016F0F10		// cccc000101101111xxxx11110001xxxx

// Data processing immediate shift
# define MASK_ARM_INSN_DPIS		0x0E000010
# define PTRN_ARM_INSN_DPIS		0x00000000
// Data processing register shift
# define MASK_ARM_INSN_DPRS		0x0E000090
# define PTRN_ARM_INSN_DPRS		0x00000010

# define MASK_THUMB2_INSN_DPRS		0xFFE00000		// 11111111111xxxxxxxxxxxxxxxxxxxxx
# define PTRN_THUMB2_INSN_DPRS		0xEA000000		// 1110101xxxxxxxxxxxxxxxxxxxxxxxxx

// Data processing immediate
# define MASK_ARM_INSN_DPI		0x0E000000
# define PTRN_ARM_INSN_DPI		0x02000000

# define MASK_THUMB_INSN_DP		0xFC00			// 111111xxxxxxxxxx
# define PTRN_THUMB_INSN_DP		0x4000			// 010000xxxxxxxxxx

# define MASK_THUMB_INSN_APC		0xF800			// 11111xxxxxxxxxxx
# define PTRN_THUMB_INSN_APC		0xA000			// 10100xxxxxxxxxxx	ADD Rd, [PC, #<imm8> * 4]

# define MASK_THUMB2_INSN_DPI		0xFBE08000		// 11111x11111xxxxx 1xxxxxxxxxxxxxxx
//# define PTRN_THUMB2_INSN_DPI		0xF0000000		// 11110x0xxxxxxxxx 0xxxxxxxxxxxxxxx				/? A6-19 ARM DDI 0406B
# define PTRN_THUMB2_INSN_DPI		0xF2000000		// 11110x1xxxxxxxxx 0xxxxxxxxxxxxxxx				/? A6-19 ARM DDI 0406B

# define MASK_THUMB_INSN_MOV3		0xFF00			// 11111111xxxxxxxx
# define PTRN_THUMB_INSN_MOV3		0x4600			// 01000110xxxxxxxx	MOV Rd, PC

# define MASK_THUMB2_INSN_RSBW		0x8000fbe0		// 1xxxxxxxxxxxxxxx 11111x11111xxxxx	// swapped
# define PTRN_THUMB2_INSN_RSBW		0x0000f1c0		// 0xxxxxxxxxxxxxxx 11110x01110xxxxx	RSB{S}.W Rd, Rn, #<const> // swapped

# define MASK_THUMB2_INSN_RORW		0xf0f0ffe0		// 1111xxxx1111xxxx 11111111111xxxxx	// swapped
# define PTRN_THUMB2_INSN_RORW		0xf000fa60		// 1111xxxx0000xxxx 11111010011xxxxx	ROR{S}.W Rd, Rn, Rm // swapped

# define MASK_THUMB2_INSN_ROR		0x0030ffef		// xxxxxxxxxx11xxxx 11111111111x1111	// swapped
# define PTRN_THUMB2_INSN_ROR		0x0030ea4f		// xxxxxxxxxx11xxxx 11101010010x1111	ROR{S} Rd, Rm, #<imm> // swapped

# define MASK_THUMB2_INSN_LSLW1		0xf0f0ffe0		// 1111xxxx1111xxxx 11111111111xxxxx	// swapped
# define PTRN_THUMB2_INSN_LSLW1		0xf000fa00		// 1111xxxx0000xxxx 11111010000xxxxx	LSL{S}.W Rd, Rn, Rm // swapped

# define MASK_THUMB2_INSN_LSLW2		0x0030ffef		// xxxxxxxxxx11xxxx 11111111111x1111	// swapped
# define PTRN_THUMB2_INSN_LSLW2		0x0000ea4f		// xxxxxxxxxx00xxxx 11101010010x1111	LSL{S}.W Rd, Rm, #<imm5> // swapped

# define MASK_THUMB2_INSN_LSRW1		0xf0f0ffe0		// 1111xxxx1111xxxx 11111111111xxxxx	// swapped
# define PTRN_THUMB2_INSN_LSRW1		0xf000fa20		// 1111xxxx0000xxxx 11111010001xxxxx	LSR{S}.W Rd, Rn, Rm // swapped

# define MASK_THUMB2_INSN_LSRW2		0x0030ffef		// xxxxxxxxxx11xxxx 11111111111x1111	// swapped
# define PTRN_THUMB2_INSN_LSRW2		0x0010ea4f		// xxxxxxxxxx01xxxx 11101010010x1111	LSR{S}.W Rd, Rm, #<imm5> // swapped

# define MASK_THUMB2_INSN_TEQ1		0x8f00fbf0		// 1xxx1111xxxxxxxx 11111x111111xxxx	// swapped
# define PTRN_THUMB2_INSN_TEQ1		0x0f00f090		// 0xxx1111xxxxxxxx 11110x001001xxxx	TEQ Rn, #<const> // swapped

# define MASK_THUMB2_INSN_TEQ2		0x0f00fff0		// xxxx1111xxxxxxxx 111111111111xxxx	// swapped
# define PTRN_THUMB2_INSN_TEQ2		0x0f00ea90		// xxxx1111xxxxxxxx 111010101001xxxx	TEQ Rn, Rm{,<shift>} // swapped

# define MASK_THUMB2_INSN_TST1		0x8f00fbf0		// 1xxx1111xxxxxxxx 11111x111111xxxx	// swapped
# define PTRN_THUMB2_INSN_TST1		0x0f00f010		// 0xxx1111xxxxxxxx 11110x000001xxxx	TST Rn, #<const> // swapped

# define MASK_THUMB2_INSN_TST2		0x0f00fff0		// xxxx1111xxxxxxxx 111111111111xxxx	// swapped
# define PTRN_THUMB2_INSN_TST2		0x0f00ea10		// xxxx1111xxxxxxxx 111010100001xxxx	TST Rn, Rm{,<shift>} // swapped


// Load immediate offset
# define MASK_ARM_INSN_LIO		0x0E100000
# define PTRN_ARM_INSN_LIO		0x04100000

# define MASK_THUMB_INSN_LIO1		0xF800			// 11111xxxxxxxxxxx
# define PTRN_THUMB_INSN_LIO1		0x6800			// 01101xxxxxxxxxxx	LDR

# define MASK_THUMB_INSN_LIO2		MASK_THUMB_INSN_LIO1
# define PTRN_THUMB_INSN_LIO2		0x7800			// 01111xxxxxxxxxxx	LDRB

# define MASK_THUMB_INSN_LIO3		MASK_THUMB_INSN_LIO1
# define PTRN_THUMB_INSN_LIO3		0x8800			// 10001xxxxxxxxxxx	LDRH

# define MASK_THUMB_INSN_LIO4		MASK_THUMB_INSN_LIO1
# define PTRN_THUMB_INSN_LIO4		0x9800			// 10011xxxxxxxxxxx	LDR SP relative

# define MASK_THUMB2_INSN_LDRW		0x0000fff0		// xxxxxxxxxxxxxxxx 111111111111xxxx	// swapped
# define PTRN_THUMB2_INSN_LDRW		0x0000f850		// xxxxxxxxxxxxxxxx 111110000101xxxx	LDR.W Rt, [Rn, #-<imm12>]// swapped

# define MASK_THUMB2_INSN_LDRW1		MASK_THUMB2_INSN_LDRW
# define PTRN_THUMB2_INSN_LDRW1		0x0000f8d0		// xxxxxxxxxxxxxxxx 111110001101xxxx	LDR.W Rt, [Rn, #<imm12>]// swapped

# define MASK_THUMB2_INSN_LDRBW		MASK_THUMB2_INSN_LDRW
# define PTRN_THUMB2_INSN_LDRBW		0x0000f810		// xxxxxxxxxxxxxxxx 111110000001xxxx	LDRB.W Rt, [Rn, #-<imm8>]// swapped

# define MASK_THUMB2_INSN_LDRBW1	MASK_THUMB2_INSN_LDRW
# define PTRN_THUMB2_INSN_LDRBW1	0x0000f890		// xxxxxxxxxxxxxxxx 111110001001xxxx	LDRB.W Rt, [Rn, #<imm12>]// swapped

# define MASK_THUMB2_INSN_LDRHW		MASK_THUMB2_INSN_LDRW
# define PTRN_THUMB2_INSN_LDRHW		0x0000f830		// xxxxxxxxxxxxxxxx 111110000011xxxx	LDRH.W Rt, [Rn, #-<imm8>]// swapped

# define MASK_THUMB2_INSN_LDRHW1	MASK_THUMB2_INSN_LDRW
# define PTRN_THUMB2_INSN_LDRHW1	0x0000f8b0		// xxxxxxxxxxxxxxxx 111110001011xxxx	LDRH.W Rt, [Rn, #<imm12>]// swapped

# define MASK_THUMB2_INSN_LDRD		0x0000fed0		// xxxxxxxxxxxxxxxx 1111111x11x1xxxx	// swapped
# define PTRN_THUMB2_INSN_LDRD		0x0000e850		// xxxxxxxxxxxxxxxx 1110100x01x1xxxx	LDRD Rt, Rt2, [Rn, #-<imm8>]// swapped

# define MASK_THUMB2_INSN_LDRD1		MASK_THUMB2_INSN_LDRD
# define PTRN_THUMB2_INSN_LDRD1		0x0000e8d0		// xxxxxxxxxxxxxxxx 1110100x11x1xxxx	LDRD Rt, Rt2, [Rn, #<imm8>]// swapped

# define MASK_THUMB2_INSN_LDRWL		0x0fc0fff0		// xxxx111111xxxxxx 111111111111xxxx	// swapped
# define PTRN_THUMB2_INSN_LDRWL		0x0000f850		// xxxxxxxxxxxxxxxx 111110000101xxxx	LDR.W Rt, [Rn, Rm, LSL #<imm2>]// swapped

# define MASK_THUMB2_INSN_LDREX		0x0f00ffff		// xxxx1111xxxxxxxx 1111111111111111	// swapped
# define PTRN_THUMB2_INSN_LDREX		0x0f00e85f		// xxxx1111xxxxxxxx 1110100001011111	LDREX Rt, [PC, #<imm8>]// swapped

# define MASK_THUMB2_INSN_MUL		0xf0f0fff0		// 1111xxxx1111xxxx 111111111111xxxx	// swapped
# define PTRN_THUMB2_INSN_MUL		0xf000fb00		// 1111xxxx0000xxxx 111110110000xxxx	MUL Rd, Rn, Rm// swapped

# define MASK_THUMB2_INSN_DP		0x0000ff00		// xxxxxxxxxxxxxxxx 11111111xxxxxxxx	// swapped
# define PTRN_THUMB2_INSN_DP		0x0000eb00		// xxxxxxxxxxxxxxxx 11101011xxxxxxxx	// swapped	ADD/SUB/SBC/...Rd, Rn, Rm{,<shift>}




// Store immediate offset
# define MASK_ARM_INSN_SIO		MASK_ARM_INSN_LIO
# define PTRN_ARM_INSN_SIO		0x04000000

# define MASK_THUMB_INSN_SIO1		MASK_THUMB_INSN_LIO1
# define PTRN_THUMB_INSN_SIO1		0x6000			// 01100xxxxxxxxxxx	STR

# define MASK_THUMB_INSN_SIO2		MASK_THUMB_INSN_LIO1
# define PTRN_THUMB_INSN_SIO2		0x7000			// 01110xxxxxxxxxxx	STRB

# define MASK_THUMB_INSN_SIO3		MASK_THUMB_INSN_LIO1
# define PTRN_THUMB_INSN_SIO3		0x8000			// 10000xxxxxxxxxxx	STRH

# define MASK_THUMB_INSN_SIO4		MASK_THUMB_INSN_LIO1
# define PTRN_THUMB_INSN_SIO4		0x9000			// 10010xxxxxxxxxxx	STR SP relative

# define MASK_THUMB2_INSN_STRW		0x0fc0fff0		// xxxx111111xxxxxx 111111111111xxxx	// swapped
# define PTRN_THUMB2_INSN_STRW		0x0000f840		// xxxx000000xxxxxx 111110000100xxxx	STR.W Rt, [Rn, Rm, {LSL #<imm2>}]// swapped

# define MASK_THUMB2_INSN_STRW1		0x0000fff0		// xxxxxxxxxxxxxxxx 111111111111xxxx	// swapped
# define PTRN_THUMB2_INSN_STRW1		0x0000f8c0		// xxxxxxxxxxxxxxxx 111110001100xxxx	STR.W Rt, [Rn, #imm12]// swapped				// STR.W Rt, [PC, #imm12] shall be skipped, because it hangs on Tegra. WTF

# define MASK_THUMB2_INSN_STRHW		MASK_THUMB2_INSN_STRW
# define PTRN_THUMB2_INSN_STRHW		0x0000f820		// xxxx000000xxxxxx 111110000010xxxx	STRH.W Rt, [Rn, Rm, {LSL #<imm2>}]// swapped

# define MASK_THUMB2_INSN_STRHW1	0x0000fff0		// xxxxxxxxxxxxxxxx 111111111111xxxx	// swapped
# define PTRN_THUMB2_INSN_STRHW1	0x0000f8a0		// xxxxxxxxxxxxxxxx 111110001010xxxx	STRH.W Rt, [Rn, #<imm12>]// swapped

# define MASK_THUMB2_INSN_STRHT		0x0f00fff0		// xxxx1111xxxxxxxx 111111111111xxxx	// swapped							// strht r1, [pc, #imm] illegal instruction on Tegra. WTF
# define PTRN_THUMB2_INSN_STRHT		0x0e00f820		// xxxx1110xxxxxxxx 111110000010xxxx	STRHT Rt, [Rn, #<imm8>]// swapped

# define MASK_THUMB2_INSN_STRT		0x0f00fff0		// xxxx1111xxxxxxxx 111111111111xxxx	// swapped
# define PTRN_THUMB2_INSN_STRT		0x0e00f840		// xxxx1110xxxxxxxx 111110000100xxxx	STRT Rt, [Rn, #<imm8>]// swapped

# define MASK_THUMB2_INSN_STRBW		MASK_THUMB2_INSN_STRW	// xxxx111111xxxxxx 111111111111xxxx	// swapped
# define PTRN_THUMB2_INSN_STRBW		0x0000f800		// xxxx000000xxxxxx 111110000100xxxx	STRB.W Rt, [Rn, Rm, {LSL #<imm2>}]// swapped

# define MASK_THUMB2_INSN_STRBW1	0x0000fff0		// xxxxxxxxxxxxxxxx 111111111111xxxx	// swapped
# define PTRN_THUMB2_INSN_STRBW1	0x0000f880		// xxxxxxxxxxxxxxxx 111110001000xxxx	STRB.W Rt, [Rn, #<imm12>]// swapped				// STRB.W Rt, [PC, #imm12] shall be skipped, because it hangs on Tegra. WTF

# define MASK_THUMB2_INSN_STRBT		0x0f00fff0		// xxxx1111xxxxxxxx 111111111111xxxx	// swapped
# define PTRN_THUMB2_INSN_STRBT		0x0e00f800		// xxxx1110xxxxxxxx 111110000000xxxx	STRBT Rt, [Rn, #<imm8>}]// swapped

# define MASK_THUMB2_INSN_STRD		0x0000fe50		// xxxxxxxxxxxxxxxx 1111111xx1x1xxxx	// swapped
# define PTRN_THUMB2_INSN_STRD		0x0000e840		// xxxxxxxxxxxxxxxx 1110100xx1x0xxxx	STR{D, EX, EXB, EXH, EXD} Rt, Rt2, [Rn, #<imm8>]// swapped


// Load register offset
# define MASK_ARM_INSN_LRO		0x0E100010
# define PTRN_ARM_INSN_LRO		0x06100000

# define MASK_THUMB_INSN_LRO1		0xFE00			// 1111111xxxxxxxxx
# define PTRN_THUMB_INSN_LRO1		0x5600			// 0101011xxxxxxxxx	LDRSB

# define MASK_THUMB_INSN_LRO2		MASK_THUMB_INSN_LRO1
# define PTRN_THUMB_INSN_LRO2		0x5800			// 0101100xxxxxxxxx	LDR

# define MASK_THUMB_INSN_LRO3		0xf800			// 11111xxxxxxxxxxx
# define PTRN_THUMB_INSN_LRO3		0x4800			// 01001xxxxxxxxxxx	LDR Rd, [PC, #<imm8> * 4]

# define MASK_THUMB_INSN_LRO4		MASK_THUMB_INSN_LRO1
# define PTRN_THUMB_INSN_LRO4		0x5A00			// 0101101xxxxxxxxx	LDRH

# define MASK_THUMB_INSN_LRO5		MASK_THUMB_INSN_LRO1
# define PTRN_THUMB_INSN_LRO5		0x5C00			// 0101110xxxxxxxxx	LDRB

# define MASK_THUMB_INSN_LRO6		MASK_THUMB_INSN_LRO1
# define PTRN_THUMB_INSN_LRO6		0x5E00			// 0101111xxxxxxxxx	LDRSH

# define MASK_THUMB2_INSN_ADR		0x8000fa1f		// 1xxxxxxxxxxxxxxx 11111x1xxxx11111	// swapped
# define PTRN_THUMB2_INSN_ADR		0x0000f20f		// 0xxxxxxxxxxxxxxx 11110x1xxxx01111	// swapped



// Store register offset
# define MASK_ARM_INSN_SRO		MASK_ARM_INSN_LRO
# define PTRN_ARM_INSN_SRO		0x06000000

# define MASK_THUMB_INSN_SRO1		MASK_THUMB_INSN_LRO1
# define PTRN_THUMB_INSN_SRO1		0x5000			// 0101000xxxxxxxxx	STR

# define MASK_THUMB_INSN_SRO2		MASK_THUMB_INSN_LRO1
# define PTRN_THUMB_INSN_SRO2		0x5200			// 0101001xxxxxxxxx	STRH

# define MASK_THUMB_INSN_SRO3		MASK_THUMB_INSN_LRO1
# define PTRN_THUMB_INSN_SRO3		0x5400			// 0101010xxxxxxxxx	STRB

// Load multiple
# define MASK_ARM_INSN_LM		0x0E100000
# define PTRN_ARM_INSN_LM		0x08100000

# define MASK_THUMB2_INSN_LDMIA		0x8000ffd0		// 1xxxxxxxxxxxxxxx 1111111111x1xxxx	// swapped
# define PTRN_THUMB2_INSN_LDMIA		0x8000e890		// 1xxxxxxxxxxxxxxx 1110100010x1xxxx	LDMIA(.W) Rn(!), {Rx, ..., PC}// swapped

# define MASK_THUMB2_INSN_LDMDB		0x8000ffd0		// 1xxxxxxxxxxxxxxx 1111111111x1xxxx	// swapped
# define PTRN_THUMB2_INSN_LDMDB		0x8000e910		// 1xxxxxxxxxxxxxxx 1110100100x1xxxx	LDMDB(.W) Rn(!), {Rx, ..., PC}// swapped

// Store multiple
# define MASK_ARM_INSN_SM		MASK_ARM_INSN_LM
# define PTRN_ARM_INSN_SM		0x08000000


// Coprocessor load/store and double register transfers
# define MASK_ARM_INSN_CLS		0x0E000000
# define PTRN_ARM_INSN_CLS		0x0C000000
// Coprocessor register transfers
# define MASK_ARM_INSN_CRT		0x0F000010
# define PTRN_ARM_INSN_CRT		0x0E000010

# define ARM_INSN_MATCH(name, insn)		((insn & MASK_ARM_INSN_##name) == PTRN_ARM_INSN_##name)
# define THUMB_INSN_MATCH(name, insn)		(((insn & 0x0000FFFF) & MASK_THUMB_INSN_##name) == PTRN_THUMB_INSN_##name)
# define THUMB2_INSN_MATCH(name, insn)		((insn & MASK_THUMB2_INSN_##name) == PTRN_THUMB2_INSN_##name)

# define ARM_INSN_REG_RN(insn)			((insn & 0x000F0000)>>16)

# define ARM_INSN_REG_SET_RN(insn, nreg)	{insn &= ~0x000F0000; insn |= nreg<<16;}

# define ARM_INSN_REG_RD(insn)			((insn & 0x0000F000)>>12)

# define ARM_INSN_REG_SET_RD(insn, nreg)	{insn &= ~0x0000F000; insn |= nreg<<12;}

# define ARM_INSN_REG_RS(insn)			((insn & 0x00000F00)>>8)

# define ARM_INSN_REG_SET_RS(insn, nreg)	{insn &= ~0x00000F00; insn |= nreg<<8;}

# define ARM_INSN_REG_RM(insn)			(insn & 0x0000000F)

# define ARM_INSN_REG_SET_RM(insn, nreg)	{insn &= ~0x0000000F; insn |= nreg;}

# define ARM_INSN_REG_MR(insn, nreg)		(insn & (1 << nreg))

# define ARM_INSN_REG_SET_MR(insn, nreg)	{insn |= (1 << nreg);}

# define ARM_INSN_REG_CLEAR_MR(insn, nreg)	{insn &= ~(1 << nreg);}

# define THUMB2_INSN_REG_RT(insn)		((insn & 0xf0000000) >> 28)
# define THUMB2_INSN_REG_RT2(insn)		((insn & 0x0f000000) >> 24)
# define THUMB2_INSN_REG_RN(insn)		(insn & 0x0000000f)
# define THUMB2_INSN_REG_RD(insn)		((insn & 0x0f000000) >> 24)
# define THUMB2_INSN_REG_RM(insn)		((insn & 0x000f0000) >> 16)




/**
 * @struct kprobe_ctlblk
 * @brief Per-cpu kprobe control block.
 * @var kprobe_ctlblk::kprobe_status
 * Kprobe status.
 * @var kprobe_ctlblk::prev_kprobe
 * Previous kprobe.
 */
struct kprobe_ctlblk {
	unsigned long kprobe_status;
	struct prev_kprobe prev_kprobe;
};

/**
 * @struct arch_specific_insn
 * @brief Architecture specific copy of original instruction.
 * @var arch_specific_insn::insn
 * Copy of the original instruction.
 */
struct arch_specific_insn {
	kprobe_opcode_t *insn;
};

typedef kprobe_opcode_t (*entry_point_t) (unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);

struct undef_hook;

void swap_register_undef_hook(struct undef_hook *hook);
void swap_unregister_undef_hook(struct undef_hook *hook);

int arch_init_module_deps(void);

int arch_make_trampoline_arm(unsigned long addr, unsigned long insn,
			     unsigned long *tramp);

struct slot_manager;
struct kretprobe;
struct kretprobe_instance;
int swap_arch_prepare_kprobe(struct kprobe *p, struct slot_manager *sm);
void swap_arch_prepare_kretprobe(struct kretprobe_instance *ri,
				 struct pt_regs *regs);

void swap_arch_arm_kprobe(struct kprobe *p);
void swap_arch_disarm_kprobe(struct kprobe *p);

int swap_setjmp_pre_handler(struct kprobe *p, struct pt_regs *regs);
int swap_longjmp_break_handler(struct kprobe *p, struct pt_regs *regs);

void save_previous_kprobe(struct kprobe_ctlblk *kcb, struct kprobe *cur_p);
void restore_previous_kprobe(struct kprobe_ctlblk *kcb);
void set_current_kprobe(struct kprobe *p, struct pt_regs *regs, struct kprobe_ctlblk *kcb);

void __naked swap_kretprobe_trampoline(void);

/**
 * @brief Gets arguments of kernel functions.
 *
 * @param regs Pointer to CPU registers data.
 * @param n Number of the argument.
 * @return Argument value.
 */
static inline unsigned long swap_get_karg(struct pt_regs *regs, unsigned long n)
{
	switch (n) {
	case 0:
		return regs->ARM_r0;
	case 1:
		return regs->ARM_r1;
	case 2:
		return regs->ARM_r2;
	case 3:
		return regs->ARM_r3;
	}

	return *((unsigned long *)regs->ARM_sp + n - 4);
}

/**
 * @brief swap_get_karg wrapper.
 *
 * @param regs Pointer to CPU registers data.
 * @param n Number of the argument.
 * @return Argument value.
 */
static inline unsigned long swap_get_sarg(struct pt_regs *regs, unsigned long n)
{
	return swap_get_karg(regs, n);
}

/* jumper */
typedef unsigned long (*jumper_cb_t)(void *);

int set_kjump_cb(struct pt_regs *regs, jumper_cb_t cb,
		 void *data, size_t size);

unsigned long get_jump_addr(void);
int set_jump_cb(unsigned long ret_addr, struct pt_regs *regs,
		jumper_cb_t cb, void *data, size_t size);

int swap_arch_init_kprobes(void);
void swap_arch_exit_kprobes(void);

//void gen_insn_execbuf (void);
//void pc_dep_insn_execbuf (void);
//void gen_insn_execbuf_holder (void);
//void pc_dep_insn_execbuf_holder (void);

#endif /* _SWAP_ASM_ARM_KPROBES_H */
