#ifndef _ASM_PROBES_H
#define _ASM_PROBES_H

struct kprobe;

struct arch_specific_insn;
typedef void (kprobe_insn_handler_t)(kprobe_opcode_t,
				     struct arch_specific_insn *,
				     struct pt_regs *);
typedef unsigned long (kprobe_check_cc)(unsigned long);
typedef void (kprobe_insn_singlestep_t)(kprobe_opcode_t,
					struct arch_specific_insn *,
					struct pt_regs *);
typedef void (kprobe_insn_fn_t)(void);

/* Architecture specific copy of original instruction. */
struct arch_specific_insn {
	kprobe_opcode_t			*insn;
	kprobe_insn_handler_t		*insn_handler;
	kprobe_check_cc			*insn_check_cc;
	kprobe_insn_singlestep_t	*insn_singlestep;
	kprobe_insn_fn_t		*insn_fn;
};

#endif
