/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_RESCTRL_H
#define _ASM_X86_RESCTRL_H

#ifdef CONFIG_X86_CPU_RESCTRL

#include <linux/sched.h>
#include <linux/jump_label.h>

/*
 * This value can never be a valid CLOSID, and is used when mapping a
 * (closid, rmid) pair to an index and back. On x86 only the RMID is
 * needed. The index is a software defined value.
 */
#define X86_RESCTRL_EMPTY_CLOSID         ((u32)~0)

extern bool rdt_alloc_capable;
extern bool rdt_mon_capable;

DECLARE_STATIC_KEY_FALSE(rdt_enable_key);
DECLARE_STATIC_KEY_FALSE(rdt_alloc_enable_key);
DECLARE_STATIC_KEY_FALSE(rdt_mon_enable_key);

static inline bool resctrl_arch_alloc_capable(void)
{
	return rdt_alloc_capable;
}

static inline void resctrl_arch_enable_alloc(void)
{
	static_branch_enable_cpuslocked(&rdt_alloc_enable_key);
	static_branch_inc_cpuslocked(&rdt_enable_key);
}

static inline void resctrl_arch_disable_alloc(void)
{
	static_branch_disable_cpuslocked(&rdt_alloc_enable_key);
	static_branch_dec_cpuslocked(&rdt_enable_key);
}

static inline bool resctrl_arch_mon_capable(void)
{
	return rdt_mon_capable;
}

static inline void resctrl_arch_enable_mon(void)
{
	static_branch_enable_cpuslocked(&rdt_mon_enable_key);
	static_branch_inc_cpuslocked(&rdt_enable_key);
}

static inline void resctrl_arch_disable_mon(void)
{
	static_branch_disable_cpuslocked(&rdt_mon_enable_key);
	static_branch_dec_cpuslocked(&rdt_enable_key);
}

static inline unsigned int resctrl_arch_round_mon_val(unsigned int val)
{
	unsigned int scale = boot_cpu_data.x86_cache_occ_scale;

	/* h/w works in units of "boot_cpu_data.x86_cache_occ_scale" */
	val /= scale;
	return val * scale;
}

static inline u32 resctrl_arch_system_num_rmid_idx(void)
{
	/* RMID are independent numbers for x86. num_rmid_idx == num_rmid */
	return boot_cpu_data.x86_cache_max_rmid + 1;
}

static inline void resctrl_arch_rmid_idx_decode(u32 idx, u32 *closid, u32 *rmid)
{
	*rmid = idx;
	*closid = X86_RESCTRL_EMPTY_CLOSID;
}

static inline u32 resctrl_arch_rmid_idx_encode(u32 ignored, u32 rmid)
{
	return rmid;
}

/* x86 can always read an rmid, nothing needs allocating */
struct rdt_resource;
static inline void *resctrl_arch_mon_ctx_alloc(struct rdt_resource *r, int evtid)
{
	might_sleep();
	return NULL;
};

static inline void resctrl_arch_mon_ctx_free(struct rdt_resource *r, int evtid,
					     void *ctx) { };

void resctrl_cpu_detect(struct cpuinfo_x86 *c);

#else

static inline void resctrl_cpu_detect(struct cpuinfo_x86 *c) {}

#endif /* CONFIG_X86_CPU_RESCTRL */

#endif /* _ASM_X86_RESCTRL_H */
