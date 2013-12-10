#ifndef _ASM_PROBES_H
#define _ASM_PROBES_H

typedef u32 probes_opcode_t;

struct arch_probes_insn;
typedef void (probes_insn_handler_t)(probes_opcode_t,
				     struct arch_probes_insn *,
				     struct pt_regs *);
typedef unsigned long (probes_check_cc)(unsigned long);
typedef void (probes_insn_singlestep_t)(probes_opcode_t,
					struct arch_probes_insn *,
					struct pt_regs *);
typedef void (probes_insn_fn_t)(void);

/* Architecture specific copy of original instruction. */
struct arch_probes_insn {
	probes_opcode_t			*insn;
	probes_insn_handler_t		*insn_handler;
	probes_check_cc			*insn_check_cc;
	probes_insn_singlestep_t	*insn_singlestep;
	probes_insn_fn_t		*insn_fn;
};

#endif
