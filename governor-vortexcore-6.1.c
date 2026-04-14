// SPDX-License-Identifier: GPL-2.0
/*
 * VortexCore CPU Governor v3.2 (Anti Parachute Fix)
 * Engineered for GKI 6.1 (Modern API)
 * Features: Adaptive Sampling, Proactive Thermal, Refined big.LITTLE, Max Hold
 * Author: Kingfinik98
 */

#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched/clock.h>
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
    unsigned int thermal_counter;
    unsigned int max_hold_counter; /* Anti Parachute State */
};

static DEFINE_PER_CPU(struct vortex_cpu_info, vortex_info);

/* ========================================================================
 * VORTEXCORE v3.2 CORE LOGIC (100% Unchanged)
 * ======================================================================== */
static void vortex_update_cpu(struct cpufreq_policy *policy)
{
    struct vortex_cpu_info *info = &per_cpu(vortex_info, policy->cpu);
    u64 now, idle_time, delta_wall, delta_idle;
    unsigned int load, freq_target, current_freq = policy->cur;
    unsigned int thermal_max = policy->max;

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

    /* Refined big.LITTLE Awareness */
    bool is_big = (policy->cpuinfo.max_freq > (policy->cpuinfo.min_freq * 2));
    unsigned int dyn_target_load = is_big ? target_load_big : target_load_little;

    /* Decision Matrix */
    if (load >= fast_ramp_up_load) {
        freq_target = thermal_max;
        if (info->max_hold_counter < 5)
            info->max_hold_counter++;
    } else if (load > dyn_target_load) {
        unsigned int freq_adj = thermal_max * load / 100;
        freq_target = max(freq_adj, current_freq);
    } else {
        /* Max Frequency Hold (Anti Terjun Payung) */
        if (info->max_hold_counter > 0) {
            freq_target = current_freq;
            info->max_hold_counter--;
        } else {
            /* V3.2 Decay Logic (freq_diff / 20) */
            if (current_freq > policy->min) {
                unsigned int freq_diff = current_freq - policy->min;
                unsigned int decay_step = max(policy->min / 100, freq_diff / 20);
                freq_target = (current_freq > policy->min + decay_step) ? current_freq - decay_step : policy->min;
            } else {
                freq_target = policy->min;
            }
        }
    }

    /* Proactive Thermal (> 20) */
    if (freq_target >= thermal_max) {
        info->thermal_counter++;
        if (info->thermal_counter > 20) {
            freq_target = (thermal_max * 95) / 100;
        }
    } else {
        if (info->thermal_counter > 0)
            info->thermal_counter--;
    }

    /* Jitter Prevention */
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
    vortex_update_cpu(policy);
}

/* ========================================================================
 * GKI 6.1 MODERN API STRUCT (Clean & Native)
 * ======================================================================== */
static struct cpufreq_governor vortex_gov = {
    .name		= "vortexcore", /* Nama di HP tetap VortexCore */
    .owner		= THIS_MODULE,
    .update_cpu	= vortex_update_cpu,
    .limits		= vortex_limits,
    .speed		= vortex_speed,
};

static int __init vortex_module_init(void)
{
    return cpufreq_register_governor(&vortex_gov);
}

static void __exit vortex_module_exit(void)
{
    cpufreq_unregister_governor(&vortex_gov);
}

module_init(vortex_module_init);
module_exit(vortex_module_exit);

MODULE_AUTHOR("Kingfinik98");
MODULE_DESCRIPTION("VortexCore v3.2 - Modern API GKI 6.1");
MODULE_LICENSE("GPL");
