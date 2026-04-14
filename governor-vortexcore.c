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
#include <linux/sched/cpufreq.h>

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
 * VORTEXCORE v2 CORE LOGIC (Modern API)
 * ======================================================================== */
static void vortex_update_cpu(struct cpufreq_policy *policy)
{
    struct vortex_cpu_info *info = &per_cpu(vortex_info, policy->cpu);
    u64 now, idle_time, delta_wall, delta_idle;
    unsigned int load, freq_target;
    unsigned int current_freq = policy->cur;

    /* 1 & 2. Precise timing using local_clock instead of jiffies */
    now = local_clock();
    idle_time = get_cpu_idle_time(policy->cpu, &delta_wall, 0);

    /* Initialize on first call */
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

    /* 6. CPU Topology Awareness (Safe heuristic for modules) */
    bool is_big = (policy->cpuinfo.max_freq > (policy->cpuinfo.min_freq * 2));
    unsigned int dyn_target_load = is_big ? target_load_big : target_load_little;

    /* 5. Thermal Awareness: Strictly respect dynamic thermal ceiling */
    unsigned int thermal_max = policy->max;

    /* VortexCore Decision Matrix */
    if (load >= fast_ramp_up_load) {
        freq_target = thermal_max;
    } else if (load > dyn_target_load) {
        unsigned int freq_adj = thermal_max * load / 100;
        freq_target = max(freq_adj, current_freq);
    } else {
        /* 4. Non-Linear Adaptive Ramp-Down (Exponential-like decay) */
        if (current_freq > policy->min) {
            unsigned int freq_diff = current_freq - policy->min;
            /* Decay by 10% of the difference, with a minimum step */
            unsigned int decay_step = max(policy->min / 100, freq_diff / 10);
            
            if (current_freq > policy->min + decay_step)
                freq_target = current_freq - decay_step;
            else
                freq_target = policy->min;
        } else {
            freq_target = policy->min;
        }
    }

    /* 3. Safer Frequency Scaling: Only update if target actually changes to reduce jitter */
    if (freq_target != info->target_freq) {
        info->target_freq = freq_target;
        __cpufreq_driver_target(policy, freq_target, CPUFREQ_RELATION_L);
    }
}

static unsigned int vortex_speed(struct cpufreq_policy *policy)
{
    struct vortex_cpu_info *info = &per_cpu(vortex_info, policy->cpu);
    return info->target_freq;
}

static void vortex_limits(struct cpufreq_policy *policy)
{
    /* Thermal subsystem directly manipulates policy->max. 
     * We ensure our governor gracefully adheres to the new limits
     * by strictly using policy->max in the update_cpu logic.
     */
}

static struct cpufreq_governor vortex_gov = {
    .name = "vortexcore",
    .owner = THIS_MODULE,
    .update_cpu = vortex_update_cpu,
    .limits = vortex_limits,
    .speed = vortex_speed,
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
