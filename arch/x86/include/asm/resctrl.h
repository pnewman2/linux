/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_RESCTRL_H
#define _ASM_X86_RESCTRL_H

#ifdef CONFIG_X86_CPU_RESCTRL

#include <linux/sched.h>
#include <linux/jump_label.h>

/**
 * struct resctrl_pqr_state - State cache for the PQR MSR
 * @cur_rmid:		The cached Resource Monitoring ID
 * @cur_closid:	The cached Class Of Service ID
 * @default_rmid:	The user assigned Resource Monitoring ID
 * @default_closid:	The user assigned cached Class Of Service ID
 * @cpu_rmid:	The permanently-assigned RMID when using soft RMIDs
 *
 * The upper 32 bits of MSR_IA32_PQR_ASSOC contain closid and the
 * lower 10 bits rmid. The update to MSR_IA32_PQR_ASSOC always
 * contains both parts, so we need to cache them. This also
 * stores the user configured per cpu CLOSID and RMID.
 *
 * The cache also helps to avoid pointless updates if the value does
 * not change.
 */
struct resctrl_pqr_state {
	u32			cur_rmid;
	u32			cur_closid;
	u32			default_rmid;
	u32			default_closid;
	u32			cpu_rmid;
};

DECLARE_PER_CPU(struct resctrl_pqr_state, pqr_state);

DECLARE_STATIC_KEY_FALSE(rdt_enable_key);
DECLARE_STATIC_KEY_FALSE(rdt_alloc_enable_key);
DECLARE_STATIC_KEY_FALSE(rdt_mon_enable_key);
DECLARE_STATIC_KEY_FALSE(rdt_soft_rmid_enable_key);

void resctrl_mbm_flush_cpu(void);

/*
 * __resctrl_sched_in() - Writes the task's CLOSid/RMID to IA32_PQR_MSR
 *
 * Following considerations are made so that this has minimal impact
 * on scheduler hot path:
 * - This will stay as no-op unless we are running on an Intel SKU
 *   which supports resource control or monitoring and we enable by
 *   mounting the resctrl file system.
 * - Caches the per cpu CLOSid/RMID values and does the MSR write only
 *   when a task with a different CLOSid/RMID is scheduled in.
 * - We allocate RMIDs/CLOSids globally in order to keep this as
 *   simple as possible.
 * Must be called with preemption disabled.
 */
static inline void __resctrl_sched_in(struct task_struct *tsk)
{
	struct resctrl_pqr_state *state = this_cpu_ptr(&pqr_state);
	u32 closid = state->default_closid;
	u32 rmid = state->default_rmid;
	u32 tmp;

	/*
	 * If this task has a closid/rmid assigned, use it.
	 * Else use the closid/rmid assigned to this cpu.
	 */
	if (static_branch_likely(&rdt_alloc_enable_key)) {
		tmp = READ_ONCE(tsk->closid);
		if (tmp)
			closid = tmp;
	}

	if (static_branch_likely(&rdt_mon_enable_key)) {
		tmp = READ_ONCE(tsk->rmid);
		if (tmp)
			rmid = tmp;
	}

	if (closid != state->cur_closid || rmid != state->cur_rmid) {
		if (static_branch_unlikely(&rdt_soft_rmid_enable_key)) {
			/*
			 * Flush current event counts to outgoing soft RMID
			 * when it changes.
			 */
			if (rmid != state->cur_rmid)
				resctrl_mbm_flush_cpu();

			/*
			 * rmid never changes in this mode, so skip wrmsr if the
			 * closid is not changing.
			 */
			if (closid != state->cur_closid)
				wrmsr(MSR_IA32_PQR_ASSOC, state->cpu_rmid,
				      closid);
		} else
			/* In this case, rmid is an RMID. */
			wrmsr(MSR_IA32_PQR_ASSOC, rmid, closid);

		/*
		 * Record new closid/rmid last so soft RMID case can detect
		 * changes.
		 */
		state->cur_closid = closid;
		state->cur_rmid = rmid;
	}
}

static inline unsigned int resctrl_arch_round_mon_val(unsigned int val)
{
	unsigned int scale = boot_cpu_data.x86_cache_occ_scale;

	/* h/w works in units of "boot_cpu_data.x86_cache_occ_scale" */
	val /= scale;
	return val * scale;
}

static inline void resctrl_sched_in(struct task_struct *tsk)
{
	if (static_branch_likely(&rdt_enable_key))
		__resctrl_sched_in(tsk);
}

void resctrl_cpu_detect(struct cpuinfo_x86 *c);

#else

static inline void resctrl_sched_in(struct task_struct *tsk) {}
static inline void resctrl_cpu_detect(struct cpuinfo_x86 *c) {}

#endif /* CONFIG_X86_CPU_RESCTRL */

#endif /* _ASM_X86_RESCTRL_H */
