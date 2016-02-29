/**
 * uprobe/arch/asm-arm/swap_uprobes.c
 * @author Alexey Gerenkov <a.gerenkov@samsung.com> User-Space Probes initial
 * implementation; Support x86/ARM/MIPS for both user and kernel spaces.
 * @author Ekaterina Gorelkina <e.gorelkina@samsung.com>: redesign module for
 * separating core and arch parts
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
 * Arch-dependent uprobe interface implementation for ARM.
 */


#include <linux/init.h>			/* need for asm/traps.h */
#include <linux/sched.h>		/* need for asm/traps.h */

#include <asm/ptrace.h>			/* need for asm/traps.h */
#include <asm/traps.h>

#include <kprobe/swap_slots.h>
#include <kprobe/swap_kprobes.h>
#include <kprobe/swap_kprobes_deps.h>
#include <uprobe/swap_uprobes.h>

#include <swap-asm/swap_kprobes.h>
#include <swap-asm/trampoline_arm.h>

#include "swap_uprobes.h"
#include "trampoline_thumb.h"


/**
 * @def flush_insns
 * @brief Flushes instructions.
 */
#define flush_insns(addr, size)					\
	flush_icache_range((unsigned long)(addr),		\
			   (unsigned long)(addr) + (size))

static inline long branch_t16_dest(kprobe_opcode_t insn, unsigned int insn_addr)
{
	long offset = insn & 0x3ff;
	offset -= insn & 0x400;
	return (insn_addr + 4 + offset * 2);
}

static inline long branch_cond_t16_dest(kprobe_opcode_t insn, unsigned int insn_addr)
{
	long offset = insn & 0x7f;
	offset -= insn & 0x80;
	return (insn_addr + 4 + offset * 2);
}

static inline long branch_t32_dest(kprobe_opcode_t insn, unsigned int insn_addr)
{
	unsigned int poff = insn & 0x3ff;
	unsigned int offset = (insn & 0x07fe0000) >> 17;

	poff -= (insn & 0x400);

	if (insn & (1 << 12))
		return ((insn_addr + 4 + (poff << 12) + offset * 4));
	else
	return ((insn_addr + 4 + (poff << 12) + offset * 4) & ~3);
}

static inline long cbz_t16_dest(kprobe_opcode_t insn, unsigned int insn_addr)
{
	unsigned int i = (insn & 0x200) >> 3;
	unsigned int offset = (insn & 0xf8) >> 2;
	return insn_addr + 4 + i + offset;
}

/* is instruction Thumb2 and NOT a branch, etc... */
static int is_thumb2(kprobe_opcode_t insn)
{
	return ((insn & 0xf800) == 0xe800 ||
		(insn & 0xf800) == 0xf000 ||
		(insn & 0xf800) == 0xf800);
}

static int arch_copy_trampoline_arm_uprobe(struct uprobe *up)
{
	int ret;
	struct kprobe *p = up2kp(up);
	unsigned long insn = p->opcode;
	unsigned long vaddr = (unsigned long)p->addr;
	unsigned long *tramp = up->atramp.tramp_arm;

	ret = arch_make_trampoline_arm(vaddr, insn, tramp);
	p->safe_arm = !!ret;

	return ret;
}

static int arch_check_insn_thumb(unsigned long insn)
{
	int ret = 0;

	/* check instructions that can change PC */
	if (THUMB_INSN_MATCH(UNDEF, insn) ||
	    THUMB_INSN_MATCH(SWI, insn) ||
	    THUMB_INSN_MATCH(BREAK, insn) ||
	    THUMB2_INSN_MATCH(B1, insn) ||
	    THUMB2_INSN_MATCH(B2, insn) ||
	    THUMB2_INSN_MATCH(BXJ, insn) ||
	    (THUMB2_INSN_MATCH(ADR, insn) && THUMB2_INSN_REG_RD(insn) == 15) ||
	    (THUMB2_INSN_MATCH(LDRW, insn) && THUMB2_INSN_REG_RT(insn) == 15) ||
	    (THUMB2_INSN_MATCH(LDRW1, insn) && THUMB2_INSN_REG_RT(insn) == 15) ||
	    (THUMB2_INSN_MATCH(LDRHW, insn) && THUMB2_INSN_REG_RT(insn) == 15) ||
	    (THUMB2_INSN_MATCH(LDRHW1, insn) && THUMB2_INSN_REG_RT(insn) == 15) ||
	    (THUMB2_INSN_MATCH(LDRWL, insn) && THUMB2_INSN_REG_RT(insn) == 15) ||
	    THUMB2_INSN_MATCH(LDMIA, insn) ||
	    THUMB2_INSN_MATCH(LDMDB, insn) ||
	    (THUMB2_INSN_MATCH(DP, insn) && THUMB2_INSN_REG_RD(insn) == 15) ||
	    (THUMB2_INSN_MATCH(RSBW, insn) && THUMB2_INSN_REG_RD(insn) == 15) ||
	    (THUMB2_INSN_MATCH(RORW, insn) && THUMB2_INSN_REG_RD(insn) == 15) ||
	    (THUMB2_INSN_MATCH(ROR, insn) && THUMB2_INSN_REG_RD(insn) == 15) ||
	    (THUMB2_INSN_MATCH(LSLW1, insn) && THUMB2_INSN_REG_RD(insn) == 15) ||
	    (THUMB2_INSN_MATCH(LSLW2, insn) && THUMB2_INSN_REG_RD(insn) == 15) ||
	    (THUMB2_INSN_MATCH(LSRW1, insn) && THUMB2_INSN_REG_RD(insn) == 15) ||
	    (THUMB2_INSN_MATCH(LSRW2, insn) && THUMB2_INSN_REG_RD(insn) == 15) ||
	    /* skip PC, #-imm12 -> SP, #-imm8 and Tegra-hanging instructions */
	    (THUMB2_INSN_MATCH(STRW1, insn) && THUMB2_INSN_REG_RN(insn) == 15) ||
	    (THUMB2_INSN_MATCH(STRBW1, insn) && THUMB2_INSN_REG_RN(insn) == 15) ||
	    (THUMB2_INSN_MATCH(STRHW1, insn) && THUMB2_INSN_REG_RN(insn) == 15) ||
	    (THUMB2_INSN_MATCH(STRW, insn) && THUMB2_INSN_REG_RN(insn) == 15) ||
	    (THUMB2_INSN_MATCH(STRHW, insn) && THUMB2_INSN_REG_RN(insn) == 15) ||
	    (THUMB2_INSN_MATCH(LDRW, insn) && THUMB2_INSN_REG_RN(insn) == 15) ||
	    (THUMB2_INSN_MATCH(LDRBW, insn) && THUMB2_INSN_REG_RN(insn) == 15) ||
	    (THUMB2_INSN_MATCH(LDRHW, insn) && THUMB2_INSN_REG_RN(insn) == 15) ||
	    /* skip STRDx/LDRDx Rt, Rt2, [Rd, ...] */
	    (THUMB2_INSN_MATCH(LDRD, insn) || THUMB2_INSN_MATCH(LDRD1, insn) || THUMB2_INSN_MATCH(STRD, insn))) {
		ret = -EFAULT;
	}

	return ret;
}

static int prep_pc_dep_insn_execbuf_thumb(kprobe_opcode_t * insns, kprobe_opcode_t insn, int uregs)
{
	unsigned char mreg = 0;
	unsigned char reg = 0;

	if (THUMB_INSN_MATCH(APC, insn) || THUMB_INSN_MATCH(LRO3, insn)) {
		reg = ((insn & 0xffff) & uregs) >> 8;
	} else {
		if (THUMB_INSN_MATCH(MOV3, insn)) {
			if (((((unsigned char) insn) & 0xff) >> 3) == 15) {
				reg = (insn & 0xffff) & uregs;
			} else {
				return 0;
			}
		} else {
			if (THUMB2_INSN_MATCH(ADR, insn)) {
				reg = ((insn >> 16) & uregs) >> 8;
				if (reg == 15) {
					return 0;
				}
			} else {
				if (THUMB2_INSN_MATCH(LDRW, insn) || THUMB2_INSN_MATCH(LDRW1, insn) ||
				    THUMB2_INSN_MATCH(LDRHW, insn) || THUMB2_INSN_MATCH(LDRHW1, insn) ||
				    THUMB2_INSN_MATCH(LDRWL, insn)) {
					reg = ((insn >> 16) & uregs) >> 12;
					if (reg == 15) {
						return 0;
					}
				} else {
					// LDRB.W PC, [PC, #immed] => PLD [PC, #immed], so Rt == PC is skipped
					if (THUMB2_INSN_MATCH(LDRBW, insn) || THUMB2_INSN_MATCH(LDRBW1, insn) ||
					    THUMB2_INSN_MATCH(LDREX, insn)) {
						reg = ((insn >> 16) & uregs) >> 12;
					} else {
						if (THUMB2_INSN_MATCH(DP, insn)) {
							reg = ((insn >> 16) & uregs) >> 12;
							if (reg == 15) {
								return 0;
							}
						} else {
							if (THUMB2_INSN_MATCH(RSBW, insn)) {
								reg = ((insn >> 12) & uregs) >> 8;
								if (reg == 15){
									return 0;
								}
							} else {
								if (THUMB2_INSN_MATCH(RORW, insn)) {
									reg = ((insn >> 12) & uregs) >> 8;
									if (reg == 15) {
										return 0;
									}
								} else {
									if (THUMB2_INSN_MATCH(ROR, insn) || THUMB2_INSN_MATCH(LSLW1, insn) ||
									    THUMB2_INSN_MATCH(LSLW2, insn) || THUMB2_INSN_MATCH(LSRW1, insn) ||
									    THUMB2_INSN_MATCH(LSRW2, insn)) {
										reg = ((insn >> 12) & uregs) >> 8;
										if (reg == 15) {
											return 0;
										}
									} else {
										if (THUMB2_INSN_MATCH(TEQ1, insn) || THUMB2_INSN_MATCH(TST1, insn)) {
											reg = 15;
										} else {
											if (THUMB2_INSN_MATCH(TEQ2, insn) || THUMB2_INSN_MATCH(TST2, insn)) {
												reg = THUMB2_INSN_REG_RM(insn);
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if ((THUMB2_INSN_MATCH(STRW, insn) || THUMB2_INSN_MATCH(STRBW, insn) ||
	     THUMB2_INSN_MATCH(STRD, insn) || THUMB2_INSN_MATCH(STRHT, insn) ||
	     THUMB2_INSN_MATCH(STRT, insn) || THUMB2_INSN_MATCH(STRHW1, insn) ||
	     THUMB2_INSN_MATCH(STRHW, insn)) && THUMB2_INSN_REG_RT(insn) == 15) {
		reg = THUMB2_INSN_REG_RT(insn);
	}

	if (reg == 6 || reg == 7) {
		*((unsigned short*)insns + 0) = (*((unsigned short*)insns + 0) & 0x00ff) | ((1 << mreg) | (1 << (mreg + 1)));
		*((unsigned short*)insns + 1) = (*((unsigned short*)insns + 1) & 0xf8ff) | (mreg << 8);
		*((unsigned short*)insns + 2) = (*((unsigned short*)insns + 2) & 0xfff8) | (mreg + 1);
		*((unsigned short*)insns + 3) = (*((unsigned short*)insns + 3) & 0xffc7) | (mreg << 3);
		*((unsigned short*)insns + 7) = (*((unsigned short*)insns + 7) & 0xf8ff) | (mreg << 8);
		*((unsigned short*)insns + 8) = (*((unsigned short*)insns + 8) & 0xffc7) | (mreg << 3);
		*((unsigned short*)insns + 9) = (*((unsigned short*)insns + 9) & 0xffc7) | ((mreg + 1) << 3);
		*((unsigned short*)insns + 10) = (*((unsigned short*)insns + 10) & 0x00ff) | (( 1 << mreg) | (1 << (mreg + 1)));
	}

	if (THUMB_INSN_MATCH(APC, insn)) {
		// ADD Rd, PC, #immed_8*4 -> ADD Rd, SP, #immed_8*4
		*((unsigned short*)insns + 4) = ((insn & 0xffff) | 0x800);				// ADD Rd, SP, #immed_8*4
	} else {
		if (THUMB_INSN_MATCH(LRO3, insn)) {
			// LDR Rd, [PC, #immed_8*4] -> LDR Rd, [SP, #immed_8*4]
			*((unsigned short*)insns + 4) = ((insn & 0xffff) + 0x5000);			// LDR Rd, [SP, #immed_8*4]
		} else {
			if (THUMB_INSN_MATCH(MOV3, insn)) {
				// MOV Rd, PC -> MOV Rd, SP
				*((unsigned short*)insns + 4) = ((insn & 0xffff) ^ 0x10);		// MOV Rd, SP
			} else {
				if (THUMB2_INSN_MATCH(ADR, insn)) {
					// ADDW Rd, PC, #imm -> ADDW Rd, SP, #imm
					insns[2] = (insn & 0xfffffff0) | 0x0d;				// ADDW Rd, SP, #imm
				} else {
					if (THUMB2_INSN_MATCH(LDRW, insn) || THUMB2_INSN_MATCH(LDRBW, insn) ||
					    THUMB2_INSN_MATCH(LDRHW, insn)) {
						// LDR.W Rt, [PC, #-<imm_12>] -> LDR.W Rt, [SP, #-<imm_8>]
						// !!!!!!!!!!!!!!!!!!!!!!!!
						// !!! imm_12 vs. imm_8 !!!
						// !!!!!!!!!!!!!!!!!!!!!!!!
						insns[2] = (insn & 0xf0fffff0) | 0x0c00000d;		// LDR.W Rt, [SP, #-<imm_8>]
					} else {
						if (THUMB2_INSN_MATCH(LDRW1, insn) || THUMB2_INSN_MATCH(LDRBW1, insn) ||
						    THUMB2_INSN_MATCH(LDRHW1, insn) || THUMB2_INSN_MATCH(LDRD, insn) ||
						    THUMB2_INSN_MATCH(LDRD1, insn) || THUMB2_INSN_MATCH(LDREX, insn)) {
							// LDRx.W Rt, [PC, #+<imm_12>] -> LDRx.W Rt, [SP, #+<imm_12>] (+/-imm_8 for LDRD Rt, Rt2, [PC, #<imm_8>]
							insns[2] = (insn & 0xfffffff0) | 0xd;													// LDRx.W Rt, [SP, #+<imm_12>]
						} else {
							if (THUMB2_INSN_MATCH(MUL, insn)) {
								insns[2] = (insn & 0xfff0ffff) | 0x000d0000;											// MUL Rd, Rn, SP
							} else {
								if (THUMB2_INSN_MATCH(DP, insn)) {
									if (THUMB2_INSN_REG_RM(insn) == 15) {
										insns[2] = (insn & 0xfff0ffff) | 0x000d0000;									// DP Rd, Rn, PC
									} else if (THUMB2_INSN_REG_RN(insn) == 15) {
										insns[2] = (insn & 0xfffffff0) | 0xd;										// DP Rd, PC, Rm
									}
								} else {
									if (THUMB2_INSN_MATCH(LDRWL, insn)) {
										// LDRx.W Rt, [PC, #<imm_12>] -> LDRx.W Rt, [SP, #+<imm_12>] (+/-imm_8 for LDRD Rt, Rt2, [PC, #<imm_8>]
										insns[2] = (insn & 0xfffffff0) | 0xd;										// LDRx.W Rt, [SP, #+<imm_12>]
									} else {
										if (THUMB2_INSN_MATCH(RSBW, insn)) {
											insns[2] = (insn & 0xfffffff0) | 0xd;									// RSB{S}.W Rd, PC, #<const> -> RSB{S}.W Rd, SP, #<const>
										} else {
											if (THUMB2_INSN_MATCH(RORW, insn) || THUMB2_INSN_MATCH(LSLW1, insn) || THUMB2_INSN_MATCH(LSRW1, insn)) {
												if ((THUMB2_INSN_REG_RM(insn) == 15) && (THUMB2_INSN_REG_RN(insn) == 15)) {
													insns[2] = (insn & 0xfffdfffd);								// ROR.W Rd, PC, PC
												} else if (THUMB2_INSN_REG_RM(insn) == 15) {
													insns[2] = (insn & 0xfff0ffff) | 0xd0000;						// ROR.W Rd, Rn, PC
												} else if (THUMB2_INSN_REG_RN(insn) == 15) {
													insns[2] = (insn & 0xfffffff0) | 0xd;							// ROR.W Rd, PC, Rm
												}
											} else {
												if (THUMB2_INSN_MATCH(ROR, insn) || THUMB2_INSN_MATCH(LSLW2, insn) || THUMB2_INSN_MATCH(LSRW2, insn)) {
													insns[2] = (insn & 0xfff0ffff) | 0xd0000;						// ROR{S} Rd, PC, #<const> -> ROR{S} Rd, SP, #<const>
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (THUMB2_INSN_MATCH(STRW, insn) || THUMB2_INSN_MATCH(STRBW, insn)) {
		insns[2] = (insn & 0xfff0ffff) | 0x000d0000;								// STRx.W Rt, [Rn, SP]
	} else {
		if (THUMB2_INSN_MATCH(STRD, insn) || THUMB2_INSN_MATCH(STRHT, insn) ||
		    THUMB2_INSN_MATCH(STRT, insn) || THUMB2_INSN_MATCH(STRHW1, insn)) {
			if (THUMB2_INSN_REG_RN(insn) == 15) {
				insns[2] = (insn & 0xfffffff0) | 0xd;							// STRD/T/HT{.W} Rt, [SP, ...]
			} else {
				insns[2] = insn;
			}
		} else {
			if (THUMB2_INSN_MATCH(STRHW, insn) && (THUMB2_INSN_REG_RN(insn) == 15)) {
				if (THUMB2_INSN_REG_RN(insn) == 15) {
					insns[2] = (insn & 0xf0fffff0) | 0x0c00000d;					// STRH.W Rt, [SP, #-<imm_8>]
				} else {
					insns[2] = insn;
				}
			}
		}
	}

	// STRx PC, xxx
	if ((reg == 15) && (THUMB2_INSN_MATCH(STRW, insn)   ||
			    THUMB2_INSN_MATCH(STRBW, insn)  ||
			    THUMB2_INSN_MATCH(STRD, insn)   ||
			    THUMB2_INSN_MATCH(STRHT, insn)  ||
			    THUMB2_INSN_MATCH(STRT, insn)   ||
			    THUMB2_INSN_MATCH(STRHW1, insn) ||
			    THUMB2_INSN_MATCH(STRHW, insn) )) {
		insns[2] = (insns[2] & 0x0fffffff) | 0xd0000000;
	}

	if (THUMB2_INSN_MATCH(TEQ1, insn) || THUMB2_INSN_MATCH(TST1, insn)) {
		insns[2] = (insn & 0xfffffff0) | 0xd;									// TEQ SP, #<const>
	} else {
		if (THUMB2_INSN_MATCH(TEQ2, insn) || THUMB2_INSN_MATCH(TST2, insn)) {
			if ((THUMB2_INSN_REG_RN(insn) == 15) && (THUMB2_INSN_REG_RM(insn) == 15)) {
				insns[2] = (insn & 0xfffdfffd);								// TEQ/TST PC, PC
			} else if (THUMB2_INSN_REG_RM(insn) == 15) {
				insns[2] = (insn & 0xfff0ffff) | 0xd0000;						// TEQ/TST Rn, PC
			} else if (THUMB2_INSN_REG_RN(insn) == 15) {
				insns[2] = (insn & 0xfffffff0) | 0xd;							// TEQ/TST PC, Rm
			}
		}
	}

	return 0;
}

static int arch_copy_trampoline_thumb_uprobe(struct uprobe *up)
{
	int uregs, pc_dep;
	struct kprobe *p = up2kp(up);
	unsigned int addr;
	unsigned long vaddr = (unsigned long)p->addr;
	unsigned long insn = p->opcode;
	unsigned long *tramp = up->atramp.tramp_thumb;
	enum { tramp_len = sizeof(up->atramp.tramp_thumb) };

	p->safe_thumb = 1;
	if (vaddr & 0x01) {
		printk("Error in %s at %d: attempt to register kprobe at an unaligned address\n", __FILE__, __LINE__);
		return -EINVAL;
	}

	if (!arch_check_insn_thumb(insn)) {
		p->safe_thumb = 0;
	}

	uregs = 0;
	pc_dep = 0;

	if (THUMB_INSN_MATCH(APC, insn) || THUMB_INSN_MATCH(LRO3, insn)) {
		uregs = 0x0700;		/* 8-10 */
		pc_dep = 1;
	} else if (THUMB_INSN_MATCH(MOV3, insn) && (((((unsigned char)insn) & 0xff) >> 3) == 15)) {
		/* MOV Rd, PC */
		uregs = 0x07;
		pc_dep = 1;
	} else if THUMB2_INSN_MATCH(ADR, insn) {
		uregs = 0x0f00;		/* Rd 8-11 */
		pc_dep = 1;
	} else if (((THUMB2_INSN_MATCH(LDRW, insn) || THUMB2_INSN_MATCH(LDRW1, insn) ||
		     THUMB2_INSN_MATCH(LDRBW, insn) || THUMB2_INSN_MATCH(LDRBW1, insn) ||
		     THUMB2_INSN_MATCH(LDRHW, insn) || THUMB2_INSN_MATCH(LDRHW1, insn) ||
		     THUMB2_INSN_MATCH(LDRWL, insn)) && THUMB2_INSN_REG_RN(insn) == 15) ||
		     THUMB2_INSN_MATCH(LDREX, insn) ||
		     ((THUMB2_INSN_MATCH(STRW, insn) || THUMB2_INSN_MATCH(STRBW, insn) ||
		       THUMB2_INSN_MATCH(STRHW, insn) || THUMB2_INSN_MATCH(STRHW1, insn)) &&
		      (THUMB2_INSN_REG_RN(insn) == 15 || THUMB2_INSN_REG_RT(insn) == 15)) ||
		     ((THUMB2_INSN_MATCH(STRT, insn) || THUMB2_INSN_MATCH(STRHT, insn)) &&
		       (THUMB2_INSN_REG_RN(insn) == 15 || THUMB2_INSN_REG_RT(insn) == 15))) {
		uregs = 0xf000;		/* Rt 12-15 */
		pc_dep = 1;
	} else if ((THUMB2_INSN_MATCH(LDRD, insn) || THUMB2_INSN_MATCH(LDRD1, insn)) && (THUMB2_INSN_REG_RN(insn) == 15)) {
		uregs = 0xff00;		/* Rt 12-15, Rt2 8-11 */
		pc_dep = 1;
	} else if (THUMB2_INSN_MATCH(MUL, insn) && THUMB2_INSN_REG_RM(insn) == 15) {
		uregs = 0xf;
		pc_dep = 1;
	} else if (THUMB2_INSN_MATCH(DP, insn) && (THUMB2_INSN_REG_RN(insn) == 15 || THUMB2_INSN_REG_RM(insn) == 15)) {
		uregs = 0xf000;		/* Rd 12-15 */
		pc_dep = 1;
	} else if (THUMB2_INSN_MATCH(STRD, insn) && ((THUMB2_INSN_REG_RN(insn) == 15) || (THUMB2_INSN_REG_RT(insn) == 15) || THUMB2_INSN_REG_RT2(insn) == 15)) {
		uregs = 0xff00;		/* Rt 12-15, Rt2 8-11 */
		pc_dep = 1;
	} else if (THUMB2_INSN_MATCH(RSBW, insn) && THUMB2_INSN_REG_RN(insn) == 15) {
		uregs = 0x0f00;		/* Rd 8-11 */
		pc_dep = 1;
	} else if (THUMB2_INSN_MATCH (RORW, insn) && (THUMB2_INSN_REG_RN(insn) == 15 || THUMB2_INSN_REG_RM(insn) == 15)) {
		uregs = 0x0f00;
		pc_dep = 1;
	} else if ((THUMB2_INSN_MATCH(ROR, insn) || THUMB2_INSN_MATCH(LSLW2, insn) || THUMB2_INSN_MATCH(LSRW2, insn)) && THUMB2_INSN_REG_RM(insn) == 15) {
		uregs = 0x0f00;		/* Rd 8-11 */
		pc_dep = 1;
	} else if ((THUMB2_INSN_MATCH(LSLW1, insn) || THUMB2_INSN_MATCH(LSRW1, insn)) && (THUMB2_INSN_REG_RN(insn) == 15 || THUMB2_INSN_REG_RM(insn) == 15)) {
		uregs = 0x0f00;		/* Rd 8-11 */
		pc_dep = 1;
	} else if ((THUMB2_INSN_MATCH(TEQ1, insn) || THUMB2_INSN_MATCH(TST1, insn)) && THUMB2_INSN_REG_RN(insn) == 15) {
		uregs = 0xf0000;	/* Rn 0-3 (16-19) */
		pc_dep = 1;
	} else if ((THUMB2_INSN_MATCH(TEQ2, insn) || THUMB2_INSN_MATCH(TST2, insn)) &&
		   (THUMB2_INSN_REG_RN(insn) == 15 || THUMB2_INSN_REG_RM(insn) == 15)) {
		uregs = 0xf0000;	/* Rn 0-3 (16-19) */
		pc_dep = 1;
	}

	if (unlikely(uregs && pc_dep)) {
		memcpy(tramp, pc_dep_insn_execbuf_thumb, tramp_len);
		if (prep_pc_dep_insn_execbuf_thumb(tramp, insn, uregs) != 0) {
			printk("Error in %s at %d: failed to prepare exec buffer for insn %lx!",
			       __FILE__, __LINE__, insn);
			p->safe_thumb = 1;
		}

		addr = vaddr + 4;
		*((unsigned short*)tramp + 13) = 0xdeff;
		*((unsigned short*)tramp + 14) = addr & 0x0000ffff;
		*((unsigned short*)tramp + 15) = addr >> 16;
		if (!is_thumb2(insn)) {
			addr = vaddr + 2;
			*((unsigned short*)tramp + 16) = (addr & 0x0000ffff) | 0x1;
			*((unsigned short*)tramp + 17) = addr >> 16;
		} else {
			addr = vaddr + 4;
			*((unsigned short*)tramp + 16) = (addr & 0x0000ffff) | 0x1;
			*((unsigned short*)tramp + 17) = addr >> 16;
		}
	} else {
		memcpy(tramp, gen_insn_execbuf_thumb, tramp_len);
		*((unsigned short*)tramp + 13) = 0xdeff;
		if (!is_thumb2(insn)) {
			addr = vaddr + 2;
			*((unsigned short*)tramp + 2) = insn;
			*((unsigned short*)tramp + 16) = (addr & 0x0000ffff) | 0x1;
			*((unsigned short*)tramp + 17) = addr >> 16;
		} else {
			addr = vaddr + 4;
			tramp[1] = insn;
			*((unsigned short*)tramp + 16) = (addr & 0x0000ffff) | 0x1;
			*((unsigned short*)tramp + 17) = addr >> 16;
		}
	}

	if (THUMB_INSN_MATCH(B2, insn)) {
		memcpy(tramp, b_off_insn_execbuf_thumb, tramp_len);
		*((unsigned short*)tramp + 13) = 0xdeff;
		addr = branch_t16_dest(insn, vaddr);
		*((unsigned short*)tramp + 14) = (addr & 0x0000ffff) | 0x1;
		*((unsigned short*)tramp + 15) = addr >> 16;
		*((unsigned short*)tramp + 16) = 0;
		*((unsigned short*)tramp + 17) = 0;

	} else if (THUMB_INSN_MATCH(B1, insn)) {
		memcpy(tramp, b_cond_insn_execbuf_thumb, tramp_len);
		*((unsigned short*)tramp + 13) = 0xdeff;
		*((unsigned short*)tramp + 0) |= (insn & 0xf00);
		addr = branch_cond_t16_dest(insn, vaddr);
		*((unsigned short*)tramp + 14) = (addr & 0x0000ffff) | 0x1;
		*((unsigned short*)tramp + 15) = addr >> 16;
		addr = vaddr + 2;
		*((unsigned short*)tramp + 16) = (addr & 0x0000ffff) | 0x1;
		*((unsigned short*)tramp + 17) = addr >> 16;

	} else if (THUMB_INSN_MATCH(BLX2, insn) ||
		   THUMB_INSN_MATCH(BX, insn)) {
		memcpy(tramp, b_r_insn_execbuf_thumb, tramp_len);
		*((unsigned short*)tramp + 13) = 0xdeff;
		*((unsigned short*)tramp + 4) = insn;
		addr = vaddr + 2;
		*((unsigned short*)tramp + 16) = (addr & 0x0000ffff) | 0x1;
		*((unsigned short*)tramp + 17) = addr >> 16;

	} else if (THUMB2_INSN_MATCH(BLX1, insn) ||
		   THUMB2_INSN_MATCH(BL, insn)) {
		memcpy(tramp, blx_off_insn_execbuf_thumb, tramp_len);
		*((unsigned short*)tramp + 13) = 0xdeff;
		addr = branch_t32_dest(insn, vaddr);
		*((unsigned short*)tramp + 14) = (addr & 0x0000ffff);
		*((unsigned short*)tramp + 15) = addr >> 16;
		addr = vaddr + 4;
		*((unsigned short*)tramp + 16) = (addr & 0x0000ffff) | 0x1;
		*((unsigned short*)tramp + 17) = addr >> 16;

	} else if (THUMB_INSN_MATCH(CBZ, insn)) {
		memcpy(tramp, cbz_insn_execbuf_thumb, tramp_len);
		*((unsigned short*)tramp + 13) = 0xdeff;
		/* zero out original branch displacement (imm5 = 0; i = 0) */
		*((unsigned short*)tramp + 0) = insn & (~0x2f8);
		/* replace it with 8 bytes offset in execbuf (imm5 = 0b00010) */
		*((unsigned short*)tramp + 0) |= 0x20;
		addr = cbz_t16_dest(insn, vaddr);
		*((unsigned short*)tramp + 14) = (addr & 0x0000ffff) | 0x1;
		*((unsigned short*)tramp + 15) = addr >> 16;
		addr = vaddr + 2;
		*((unsigned short*)tramp + 16) = (addr & 0x0000ffff) | 0x1;
		*((unsigned short*)tramp + 17) = addr >> 16;
	}

	return 0;
}

/**
 * @brief Prepares uprobe for ARM.
 *
 * @param up Pointer to the uprobe.
 * @return 0 on success,\n
 * negative error code on error.
 */
int arch_prepare_uprobe(struct uprobe *up)
{
	struct kprobe *p = up2kp(up);
	struct task_struct *task = up->task;
	unsigned long vaddr = (unsigned long)p->addr;
	unsigned long insn;

	if (vaddr & 0x01) {
		printk("Error in %s at %d: attempt to register uprobe "
		       "at an unaligned address\n", __FILE__, __LINE__);
		return -EINVAL;
	}

	if (!read_proc_vm_atomic(task, vaddr, &insn, sizeof(insn)))
		panic("failed to read memory %lx!\n", vaddr);

	p->opcode = insn;

	arch_copy_trampoline_arm_uprobe(up);
	arch_copy_trampoline_thumb_uprobe(up);

	if ((p->safe_arm) && (p->safe_thumb)) {
		printk("Error in %s at %d: failed "
		       "arch_copy_trampoline_*_uprobe() (both) "
		       "[tgid=%u, addr=%lx, data=%lx]\n",
		       __FILE__, __LINE__, task->tgid, vaddr, insn);
		return -EFAULT;
	}

	up->atramp.utramp = swap_slot_alloc(up->sm);
	if (up->atramp.utramp == NULL) {
		printk("Error: swap_slot_alloc failed (%08lx)\n", vaddr);
		return -ENOMEM;
	}

	return 0;
}

/**
 * @brief Analysis opcodes.
 *
 * @param rp Pointer to the uretprobe.
 * @return Void.
 */
void arch_opcode_analysis_uretprobe(struct uretprobe *rp)
{
	/* Remove retprobe if first insn overwrites lr */
	rp->thumb_noret = !!(THUMB2_INSN_MATCH(BL, rp->up.kp.opcode) ||
			     THUMB2_INSN_MATCH(BLX1, rp->up.kp.opcode) ||
			     THUMB_INSN_MATCH(BLX2, rp->up.kp.opcode));

	rp->arm_noret = !!(ARM_INSN_MATCH(BL, rp->up.kp.opcode) ||
			   ARM_INSN_MATCH(BLX1, rp->up.kp.opcode) ||
			   ARM_INSN_MATCH(BLX2, rp->up.kp.opcode));
}

/**
 * @brief Prepates uretprobe for ARM.
 *
 * @param ri Pointer to the uretprobe instance.
 * @param regs Pointer to CPU register data.
 * @return Void.
 */
void arch_prepare_uretprobe(struct uretprobe_instance *ri,
			    struct pt_regs *regs)
{
	ri->ret_addr = (kprobe_opcode_t *)regs->ARM_lr;
	ri->sp = (kprobe_opcode_t *)regs->ARM_sp;

	/* Set flag of current mode */
	ri->sp = (kprobe_opcode_t *)((long)ri->sp | !!thumb_mode(regs));

	if (thumb_mode(regs)) {
		regs->ARM_lr = (unsigned long)(ri->rp->up.kp.ainsn.insn) + 0x1b;
	} else {
		regs->ARM_lr = (unsigned long)(ri->rp->up.kp.ainsn.insn + UPROBES_TRAMP_RET_BREAK_IDX);
	}
}

/**
 * @brief Disarms uretprobe instance.
 *
 * @param ri Pointer to the uretprobe instance
 * @param task Pointer to the task for which the uretprobe instance
 * @return 0 on success,\n
 * negative error code on error.
 */
int arch_disarm_urp_inst(struct uretprobe_instance *ri,
			 struct task_struct *task)
{
	struct pt_regs *uregs = task_pt_regs(ri->task);
	unsigned long ra = swap_get_ret_addr(uregs);
	unsigned long *tramp;
	unsigned long *sp = (unsigned long *)((long)ri->sp & ~1);
	unsigned long *stack = sp - RETPROBE_STACK_DEPTH + 1;
	unsigned long *found = NULL;
	unsigned long *buf[RETPROBE_STACK_DEPTH];
	int i, retval;

	/* Understand function mode */
	if ((long)ri->sp & 1) {
		tramp = (unsigned long *)
			((unsigned long)ri->rp->up.kp.ainsn.insn + 0x1b);
	} else {
		tramp = (unsigned long *)(ri->rp->up.kp.ainsn.insn +
					  UPROBES_TRAMP_RET_BREAK_IDX);
	}

	/* check stack */
	retval = read_proc_vm_atomic(task, (unsigned long)stack,
				     buf, sizeof(buf));
	if (retval != sizeof(buf)) {
		printk("---> %s (%d/%d): failed to read stack from %08lx\n",
		       task->comm, task->tgid, task->pid,
		       (unsigned long)stack);
		retval = -EFAULT;
		goto check_lr;
	}

	/* search the stack from the bottom */
	for (i = RETPROBE_STACK_DEPTH - 1; i >= 0; i--) {
		if (buf[i] == tramp) {
			found = stack + i;
			break;
		}
	}

	if (!found) {
		retval = -ESRCH;
		goto check_lr;
	}

	printk("---> %s (%d/%d): trampoline found at "
	       "%08lx (%08lx /%+d) - %p\n",
	       task->comm, task->tgid, task->pid,
	       (unsigned long)found, (unsigned long)sp,
	       found - sp, ri->rp->up.kp.addr);
	retval = write_proc_vm_atomic(task, (unsigned long)found,
				      &ri->ret_addr,
				      sizeof(ri->ret_addr));
	if (retval != sizeof(ri->ret_addr)) {
		printk("---> %s (%d/%d): failed to write value to %08lx",
		       task->comm, task->tgid, task->pid, (unsigned long)found);
		retval = -EFAULT;
	} else {
		retval = 0;
	}

check_lr: /* check lr anyway */
	if (ra == (unsigned long)tramp) {
		printk("---> %s (%d/%d): trampoline found at "
		       "lr = %08lx - %p\n",
		       task->comm, task->tgid, task->pid,
		       ra, ri->rp->up.kp.addr);
		swap_set_ret_addr(uregs, (unsigned long)ri->ret_addr);
		retval = 0;
	} else if (retval) {
		printk("---> %s (%d/%d): trampoline NOT found at "
		       "sp = %08lx, lr = %08lx - %p\n",
		       task->comm, task->tgid, task->pid,
		       (unsigned long)sp, ra, ri->rp->up.kp.addr);
	}

	return retval;
}

/**
 * @brief Jump pre-handler.
 *
 * @param p Pointer to the kprobe.
 * @param regs Pointer to CPU register data.
 * @return 0.
 */
int setjmp_upre_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct uprobe *up = container_of(p, struct uprobe, kp);
	struct ujprobe *jp = container_of(up, struct ujprobe, up);

	kprobe_pre_entry_handler_t pre_entry = (kprobe_pre_entry_handler_t)jp->pre_entry;
	entry_point_t entry = (entry_point_t)jp->entry;

	if (pre_entry) {
		p->ss_addr[smp_processor_id()] = (kprobe_opcode_t *)
						 pre_entry(jp->priv_arg, regs);
	}

	if (entry) {
		entry(regs->ARM_r0, regs->ARM_r1, regs->ARM_r2,
		      regs->ARM_r3, regs->ARM_r4, regs->ARM_r5);
	} else {
		arch_ujprobe_return();
	}

	return 0;
}

/**
 * @brief Gets trampoline address.
 *
 * @param p Pointer to the kprobe.
 * @param regs Pointer to CPU register data.
 * @return Trampoline address.
 */
unsigned long arch_get_trampoline_addr(struct kprobe *p, struct pt_regs *regs)
{
	return thumb_mode(regs) ?
			(unsigned long)(p->ainsn.insn) + 0x1b :
			(unsigned long)(p->ainsn.insn + UPROBES_TRAMP_RET_BREAK_IDX);
}

/**
 * @brief Restores return address.
 *
 * @param orig_ret_addr Original return address.
 * @param regs Pointer to CPU register data.
 * @return Void.
 */
void arch_set_orig_ret_addr(unsigned long orig_ret_addr, struct pt_regs *regs)
{
	regs->ARM_lr = orig_ret_addr;
	regs->ARM_pc = orig_ret_addr & ~0x1;

	if (regs->ARM_lr & 0x1)
		regs->ARM_cpsr |= PSR_T_BIT;
	else
		regs->ARM_cpsr &= ~PSR_T_BIT;
}

/**
 * @brief Removes uprobe.
 *
 * @param up Pointer to the uprobe.
 * @return Void.
 */
void arch_remove_uprobe(struct uprobe *up)
{
	swap_slot_free(up->sm, up->atramp.utramp);
}

static void restore_opcode_for_thumb(struct kprobe *p, struct pt_regs *regs)
{
	if (thumb_mode(regs) && !is_thumb2(p->opcode)) {
		u16 tmp = p->opcode >> 16;
		write_proc_vm_atomic(current,
				(unsigned long)((u16*)p->addr + 1), &tmp, 2);
		flush_insns(p->addr, 4);
	}
}

static int make_trampoline(struct uprobe *up, struct pt_regs *regs)
{
	unsigned long *tramp, *utramp;
	struct kprobe *p = up2kp(up);
	int sw;

	/*
	 * 0 bit - thumb mode		(0 - arm, 1 - thumb)
	 * 1 bit - arm mode support	(0 - off, 1  on)
	 * 2 bit - thumb mode support	(0 - off, 1  on)`
	 */
	sw = (!!thumb_mode(regs)) |
	     (int)!p->safe_arm << 1 |
	     (int)!p->safe_thumb << 2;

	switch (sw) {
	/* ARM */
	case 0b110:
	case 0b010:
		tramp = up->atramp.tramp_arm;
		break;
	/* THUMB */
	case 0b111:
	case 0b101:
		restore_opcode_for_thumb(p, regs);
		tramp = up->atramp.tramp_thumb;
		break;
	default:
		printk("Error in %s at %d: we are in arm mode "
		       "(!) and check instruction was fail "
		       "(%0lX instruction at %p address)!\n",
		       __FILE__, __LINE__, p->opcode, p->addr);

		disarm_uprobe(p, up->task);

		return 1;
	}

	utramp = up->atramp.utramp;

	if (!write_proc_vm_atomic(up->task, (unsigned long)utramp, tramp,
				  UPROBES_TRAMP_LEN * sizeof(*tramp)))
		panic("failed to write memory %p!\n", utramp);
	flush_insns(utramp, UPROBES_TRAMP_LEN * sizeof(*tramp));

	p->ainsn.insn = utramp;

	return 0;
}

static int uprobe_handler(struct pt_regs *regs)
{
	kprobe_opcode_t *addr = (kprobe_opcode_t *)(regs->ARM_pc);
	struct task_struct *task = current;
	pid_t tgid = task->tgid;
	struct kprobe *p;

	p = get_ukprobe(addr, tgid);
	if (p == NULL) {
		unsigned long offset_bp = thumb_mode(regs) ?
					  0x1a :
					  4 * UPROBES_TRAMP_RET_BREAK_IDX;
		void *tramp_addr = (void *)addr - offset_bp;

		p = get_ukprobe_by_insn_slot(tramp_addr, tgid, regs);
		if (p == NULL) {
			printk("no_uprobe: Not one of ours: let "
			       "kernel handle it %p\n", addr);
			return 1;
		}

		trampoline_uprobe_handler(p, regs);
	} else {
		if (p->ainsn.insn == NULL) {
			struct uprobe *up = kp2up(p);

			if (make_trampoline(up, regs)) {
				printk("no_uprobe live\n");
				return 0;
			}

			/* for uretprobe */
			add_uprobe_table(p);
		}

		if (!p->pre_handler || !p->pre_handler(p, regs)) {
			prepare_singlestep(p, regs);
		}
	}

	return 0;
}

/**
 * @brief Breakpoint instruction handler.
 *
 * @param regs Pointer to CPU register data.
 * @param instr Instruction.
 * @return uprobe_handler results.
 */
int uprobe_trap_handler(struct pt_regs *regs, unsigned int instr)
{
	int ret;
	unsigned long flags;
	local_irq_save(flags);

	preempt_disable();
	ret = uprobe_handler(regs);
	preempt_enable_no_resched();

	local_irq_restore(flags);
	return ret;
}

/* userspace probes hook (arm) */
static struct undef_hook undef_hook_for_us_arm = {
	.instr_mask	= 0xffffffff,
	.instr_val	= BREAKPOINT_INSTRUCTION,
	.cpsr_mask	= MODE_MASK,
	.cpsr_val	= USR_MODE,
	.fn		= uprobe_trap_handler
};

/* userspace probes hook (thumb) */
static struct undef_hook undef_hook_for_us_thumb = {
	.instr_mask	= 0xffffffff,
	.instr_val	= BREAKPOINT_INSTRUCTION & 0x0000ffff,
	.cpsr_mask	= MODE_MASK,
	.cpsr_val	= USR_MODE,
	.fn		= uprobe_trap_handler
};

/**
 * @brief Installs breakpoint hooks.
 *
 * @return 0.
 */
int swap_arch_init_uprobes(void)
{
	swap_register_undef_hook(&undef_hook_for_us_arm);
	swap_register_undef_hook(&undef_hook_for_us_thumb);

	return 0;
}

/**
 * @brief Uninstalls breakpoint hooks.
 *
 * @return Void.
 */
void swap_arch_exit_uprobes(void)
{
	swap_unregister_undef_hook(&undef_hook_for_us_thumb);
	swap_unregister_undef_hook(&undef_hook_for_us_arm);
}
