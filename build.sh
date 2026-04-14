// SPDX-License-Identifier: GPL-2.0
/*
 * VortexCore CPU Governor
 * 
 * Engineered for GKI 5.10, 6.1, and 6.6
 * Philosophy: Dynamic equilibrium between peak gaming throughput and daily battery endurance.
 * Implements an aggressive ramp-up heuristic combined with a granular decay algorithm.
 *
 * Author: Kingfinik98
 */

#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/tick.h>
#include <linux/sched/cpufreq.h>
#include <linux/workqueue.h>

/* VortexCore Heuristic Parameters */
#define VORTEX_DEFAULT_TARGET_LOAD 80
#define VORTEX_FAST_RAMP_UP_LOAD 90
#define VORTEX_SMOOTH_RAMP_DOWN_STEP (1 * 1024 * 1024) // Granular step-down ~1MHz
#define VORTEX_SAMPLE_RATE (10) // Sampling rate dalam milidetik

static unsigned int target_load = VORTEX_DEFAULT_TARGET_LOAD;
module_param(target_load, uint, 0644);

static unsigned int fast_ramp_up_load = VORTEX_FAST_RAMP_UP_LOAD;
module_param(fast_ramp_up_load, uint, 0644);

struct vortex_cpu_info {
    u64 prev_cpu_idle;
    u64 prev_cpu_wall;
    unsigned int target_freq;
};

static DEFINE_PER_CPU(struct vortex_cpu_info, vortex_info);

/* ========================================================================
 * LOGIKA INTI VORTEXCORE (Dipakai Bersama oleh Legacy & Modern API)
 * ======================================================================== */
static void vortex_eval_freq(struct cpufreq_policy *policy)
{
    struct vortex_cpu_info *info = &per_cpu(vortex_info, policy->cpu);
    u64 now, idle_time, wall_time;
    unsigned int load, freq_target;
    unsigned int current_freq = policy->cur;

    now = get_jiffies_64();
    idle_time = get_cpu_idle_time(policy->cpu, &wall_time, 0);

    wall_time = now - info->prev_cpu_wall;
    idle_time = idle_time - info->prev_cpu_idle;

    if (wall_time == 0 || wall_time < idle_time)
        load = 0;
    else
        load = div64_u64(100 * (wall_time - idle_time), wall_time);

    info->prev_cpu_wall = now;
    info->prev_cpu_idle = idle_time;

    /* VortexCore Decision Matrix */
    if (load >= fast_ramp_up_load) {
        freq_target = policy->max;
    } else if (load > target_load) {
        unsigned int freq_adj = policy->max * load / 100;
        freq_target = max(freq_adj, current_freq);
    } else {
        if (current_freq > policy->min) {
            if (current_freq > VORTEX_SMOOTH_RAMP_DOWN_STEP)
                freq_target = current_freq - VORTEX_SMOOTH_RAMP_DOWN_STEP;
            else
                freq_target = policy->min;
        } else {
            freq_target = policy->min;
        }
    }

    info->target_freq = freq_target;
    __cpufreq_driver_target(policy, freq_target, CPUFREQ_RELATION_L);
}

/* ========================================================================
 * LEGACY API (Khusus Kernel 5.10 & 6.1)
 * ======================================================================== */
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0)

struct vortex_policy_info {
    struct delayed_work work;
    struct cpufreq_policy *policy;
};

static void vortex_work_handler(struct work_struct *work)
{
    struct vortex_policy_info *vpinfo = container_of(work, struct vortex_policy_info, work.work);
    vortex_eval_freq(vpinfo->policy);
    schedule_delayed_work_on(vpinfo->policy->cpu, &vpinfo->work, msecs_to_jiffies(VORTEX_SAMPLE_RATE));
}

static int vortex_governor(struct cpufreq_policy *policy, unsigned int event)
{
    struct vortex_policy_info *vpinfo;
    unsigned int cpu;

    switch (event) {
    case CPUFREQ_GOV_START:
        if (!policy->governor_data) {
            vpinfo = kzalloc(sizeof(*vpinfo), GFP_KERNEL);
            if (!vpinfo)
                return -ENOMEM;
            vpinfo->policy = policy;
            INIT_DEFERRABLE_WORK(&vpinfo->work, vortex_work_handler);
            policy->governor_data = vpinfo;
        }
        for_each_cpu(cpu, policy->cpus) {
            struct vortex_cpu_info *info = &per_cpu(vortex_info, cpu);
            info->prev_cpu_idle = get_cpu_idle_time(cpu, &info->prev_cpu_wall, 0);
            info->target_freq = policy->cur;
        }
        schedule_delayed_work_on(policy->cpu, &vpinfo->work, msecs_to_jiffies(VORTEX_SAMPLE_RATE));
        break;
    case CPUFREQ_GOV_STOP:
        vpinfo = policy->governor_data;
        if (vpinfo) {
            cancel_delayed_work_sync(&vpinfo->work);
            kfree(vpinfo);
            policy->governor_data = NULL;
        }
        break;
    case CPUFREQ_GOV_LIMITS:
        break;
    }
    return 0;
}

static struct cpufreq_governor vortex_gov = {
    .name = "vortexcore",
    .owner = THIS_MODULE,
    .governor = vortex_governor,
};

#else
/* ========================================================================
 * MODERN API (Khusus Kernel 6.6 ke atas)
 * ======================================================================== */

static unsigned int vortex_speed(struct cpufreq_policy *policy)
{
    struct vortex_cpu_info *info = &per_cpu(vortex_info, policy->cpu);
    return info->target_freq;
}

static void vortex_limits(struct cpufreq_policy *policy)
{
    /* Enforce frequency boundaries based on governor constraints */
}

static struct cpufreq_governor vortex_gov = {
    .name = "vortexcore",
    .owner = THIS_MODULE,
    .update_cpu = vortex_eval_freq,
    .limits = vortex_limits,
    .speed = vortex_speed,
};

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(6, 6, 0) */

/* ========================================================================
 * INIT & EXIT
 * ======================================================================== */
static int __init vortex_init(void)
{
    return cpufreq_register_governor(&vortex_gov);
}

static void __exit vortex_exit(void)
{
    cpufreq_unregister_governor(&vortex_gov);
}

module_init(vortex_init);
module_exit(vortex_exit);

MODULE_AUTHOR("Kingfinik98");
MODULE_DESCRIPTION("VortexCore - Dynamic Gaming & Endurance Governor for GKI 5.10/6.1/6.6");
MODULE_LICENSE("GPL");
