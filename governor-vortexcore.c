// SPDX-License-Identifier: GPL-2.0
/*
 * VortexCore CPU Governor v2
 * 
 * Engineered for GKI 5.10, 6.1, and 6.6
 * Philosophy: Dynamic equilibrium between peak gaming throughput and daily battery endurance.
 * Implements thermal-aware, big.LITTLE-aware, and non-linear decay algorithms.
 *
 * Author: Kingfinik98
 */

#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/sched/cpufreq.h>
#include <linux/workqueue.h>

/* Fallback for strict GKI 5.10 environments where these macros are stripped from headers */
#ifndef CPUFREQ_GOV_START
#define CPUFREQ_GOV_START  1
#define CPUFREQ_GOV_STOP   2
#define CPUFREQ_GOV_LIMITS 3
#endif

/* VortexCore Heuristic Parameters */
static unsigned int target_load_big = 80;
module_param_named(target_load_big, target_load_big, uint, 0644);

static unsigned int target_load_little = 90;
module_param_named(target_load_little, target_load_little, uint, 0644);

static unsigned int fast_ramp_up_load = 90;
module_param(fast_ramp_up_load, uint, 0644);

struct vortex_cpu_info {
    u64 prev_cpu_idle;
    u64 prev_cpu_wall;
    unsigned int target_freq;
};

static DEFINE_PER_CPU(struct vortex_cpu_info, vortex_info);

/* ========================================================================
 * VORTEXCORE v2 CORE LOGIC (Shared logic)
 * ======================================================================== */
static void vortex_eval_freq(struct cpufreq_policy *policy)
{
    struct vortex_cpu_info *info = &per_cpu(vortex_info, policy->cpu);
    u64 now, idle_time, delta_wall, delta_idle;
    unsigned int load, freq_target;
    unsigned int current_freq = policy->cur;

    /* 1 & 2. Precise timing using local_clock instead of jiffies */
    now = local_clock();
    idle_time = get_cpu_idle_time(policy->cpu, &delta_wall, 0);

    if (info->prev_cpu_wall == 0) {
        info->prev_cpu_wall = now;
        info->prev_cpu_idle = idle_time;
        info->target_freq = current_freq;
        return;
    }

    delta_wall = now - info->prev_cpu_wall;
    delta_idle = idle_time - info->prev_cpu_idle;

    info->prev_cpu_wall = now;
    info->prev_cpu_idle = idle_time;

    if (delta_wall == 0 || delta_idle > delta_wall)
        load = 0;
    else
        load = div64_u64(100 * (delta_wall - delta_idle), delta_wall);

    /* 6. CPU Topology Awareness */
    bool is_big = (policy->cpuinfo.max_freq > (policy->cpuinfo.min_freq * 2));
    unsigned int dyn_target_load = is_big ? target_load_big : target_load_little;

    /* 5. Thermal Awareness */
    unsigned int thermal_max = policy->max;

    /* VortexCore Decision Matrix */
    if (load >= fast_ramp_up_load) {
        freq_target = thermal_max;
    } else if (load > dyn_target_load) {
        unsigned int freq_adj = thermal_max * load / 100;
        freq_target = max(freq_adj, current_freq);
    } else {
        /* 4. Non-Linear Adaptive Ramp-Down */
        if (current_freq > policy->min) {
            unsigned int freq_diff = current_freq - policy->min;
            unsigned int decay_step = max(policy->min / 100, freq_diff / 10);
            
            if (current_freq > policy->min + decay_step)
                freq_target = current_freq - decay_step;
            else
                freq_target = policy->min;
        } else {
            freq_target = policy->min;
        }
    }

    /* 3. Safer Frequency Scaling (Jitter reduction) */
    if (freq_target != info->target_freq) {
        info->target_freq = freq_target;
        __cpufreq_driver_target(policy, freq_target, CPUFREQ_RELATION_L);
    }
}

/* ========================================================================
 * LEGACY API WRAPPER (For GKI 5.10 strict environments)
 * ======================================================================== */
struct vortex_policy_info {
    struct delayed_work work;
    struct cpufreq_policy *policy;
};

static void vortex_work_handler(struct work_struct *work)
{
    struct vortex_policy_info *vpinfo = container_of(work, struct vortex_policy_info, work.work);
    vortex_eval_freq(vpinfo->policy);
    schedule_delayed_work_on(vpinfo->policy->cpu, &vpinfo->work, msecs_to_jiffies(10));
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
        schedule_delayed_work_on(policy->cpu, &vpinfo->work, msecs_to_jiffies(10));
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
MODULE_DESCRIPTION("VortexCore v2 - Thermal & Topology Aware Governor for GKI");
MODULE_LICENSE("GPL");
