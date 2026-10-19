// SPDX-License-Identifier: GPL-2.0
/*
 * VortexCore CPU Governor v3.3 (Boot Smooth)
 * Engineered for GKI 5.10 (Hybrid API)
 * Features: Adaptive Sampling, Proactive Thermal, Refined big.LITTLE, Max Hold,
 *           Cluster-wide Load, Boot Boost, High-priority Sampling
 * Author: Kingfinik98
 */

#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/sched/cpufreq.h>
#include <linux/workqueue.h>

/* VortexCore Heuristic Parameters */
static unsigned int target_load_big = 80;
module_param_named(target_load_big, target_load_big, uint, 0644);
static unsigned int target_load_little = 90;
module_param_named(target_load_little, target_load_little, uint, 0644);
static unsigned int fast_ramp_up_load = 90;
module_param(fast_ramp_up_load, uint, 0644);

/*
 * Boot boost: for boot_boost_ms after the governor starts, never drop below
 * boot_floor_pct % of the policy max and sample every 10 ms. This hides the
 * ramp-up latency while the system is still busy right after boot.
 * Set boot_boost_ms = 0 to disable.
 */
static unsigned int boot_boost_ms = 20000;
module_param(boot_boost_ms, uint, 0644);
static unsigned int boot_floor_pct = 60;
module_param(boot_floor_pct, uint, 0644);

struct vortex_cpu_info {
    u64 prev_cpu_idle;
    u64 prev_cpu_wall;
    unsigned int last_load;
    unsigned int target_freq;
    unsigned int thermal_counter;
    unsigned int next_delay_ms;
    unsigned int max_hold_counter; /* Anti Parachute State */
};

static DEFINE_PER_CPU(struct vortex_cpu_info, vortex_info);

struct vortex_policy_info {
    struct delayed_work work;
    struct cpufreq_policy *policy;
    unsigned long boot_until; /* jiffies, end of boot boost window */
};

/* ========================================================================
 * LOAD: highest busy % across every CPU of the policy.
 * get_cpu_idle_time() returns idle AND wall time in usec, so both deltas
 * use the same unit (the old code mixed local_clock() nsec with usec idle,
 * which made load read ~100% all the time).
 * ======================================================================== */
static unsigned int vortex_get_max_load(struct cpufreq_policy *policy)
{
    unsigned int cpu, max_load = 0;

    for_each_cpu(cpu, policy->cpus) {
        struct vortex_cpu_info *ci = &per_cpu(vortex_info, cpu);
        u64 wall, idle, dwall, didle;

        idle = get_cpu_idle_time(cpu, &wall, 0);

        if (!ci->prev_cpu_wall) {
            ci->prev_cpu_wall = wall;
            ci->prev_cpu_idle = idle;
            continue;
        }

        dwall = wall - ci->prev_cpu_wall;

        /* Window < 5 ms (e.g. back-to-back limits() call): reuse last load */
        if (dwall < 5000) {
            max_load = max(max_load, ci->last_load);
            continue;
        }

        didle = idle - ci->prev_cpu_idle;
        ci->prev_cpu_wall = wall;
        ci->prev_cpu_idle = idle;

        if (didle >= dwall)
            ci->last_load = 0;
        else
            ci->last_load = (unsigned int)div64_u64(100 * (dwall - didle), dwall);

        max_load = max(max_load, ci->last_load);
    }

    return max_load;
}

/* ========================================================================
 * VORTEXCORE v3.3 CORE LOGIC
 * ======================================================================== */
static void vortex_eval_freq(struct cpufreq_policy *policy)
{
    struct vortex_cpu_info *info = &per_cpu(vortex_info, policy->cpu);
    struct vortex_policy_info *vpinfo = policy->governor_data;
    unsigned int load, freq_target, current_freq = policy->cur;
    unsigned int thermal_max = policy->max;
    unsigned int dyn_target_load;
    bool is_big, boosting;

    load = vortex_get_max_load(policy);

    /* Refined big.LITTLE Awareness (UNCHANGED) */
    is_big = (policy->cpuinfo.max_freq > (policy->cpuinfo.min_freq * 2));
    dyn_target_load = is_big ? target_load_big : target_load_little;

    boosting = vpinfo && boot_boost_ms &&
               time_before(jiffies, vpinfo->boot_until);

    /* Decision Matrix (STRUCTURE UNCHANGED) */
    if (load >= fast_ramp_up_load) {
        freq_target = thermal_max;
        if (info->max_hold_counter < 5)
            info->max_hold_counter++; /* Activate max hold */
    } else if (load > dyn_target_load) {
        unsigned int freq_adj = thermal_max * load / 100;
        freq_target = max(freq_adj, current_freq);
    } else {
        /* Max Frequency Hold (Anti-Parachuting) */
        if (info->max_hold_counter > 0) {
            freq_target = current_freq; /* Hold frequency, block decay */
            info->max_hold_counter--;
        } else {
            /* Decay Logic (UNCHANGED: freq_diff / 20) */
            if (current_freq > policy->min) {
                unsigned int freq_diff = current_freq - policy->min;
                unsigned int decay_step = max(policy->min / 100, freq_diff / 20);
                freq_target = (current_freq > policy->min + decay_step) ? current_freq - decay_step : policy->min;
            } else {
                freq_target = policy->min;
            }
        }
    }

    /* Boot Boost: hold a frequency floor right after boot */
    if (boosting) {
        unsigned int pct = min_t(unsigned int, boot_floor_pct, 100);
        unsigned int floor_freq = (thermal_max * pct) / 100;

        floor_freq = max(floor_freq, policy->min);
        if (freq_target < floor_freq)
            freq_target = floor_freq;
    }

    /* Proactive Thermal (UNCHANGED: > 20) */
    if (freq_target >= thermal_max) {
        info->thermal_counter++;
        if (info->thermal_counter > 20) {
            freq_target = (thermal_max * 95) / 100;
        }
    } else {
        if (info->thermal_counter > 0)
            info->thermal_counter--;
    }

    /* Jitter Prevention (UNCHANGED) */
    if (freq_target != info->target_freq) {
        info->target_freq = freq_target;
        __cpufreq_driver_target(policy, freq_target, CPUFREQ_RELATION_L);
    }

    /* Adaptive Sampling (10ms busy/boost, 20ms idle) */
    if (boosting || load >= fast_ramp_up_load || load > dyn_target_load)
        info->next_delay_ms = 10;
    else
        info->next_delay_ms = 20;
}

static void vortex_work_handler(struct work_struct *work)
{
    struct vortex_policy_info *vpinfo = container_of(work, struct vortex_policy_info, work.work);
    struct cpufreq_policy *policy = vpinfo->policy;
    struct vortex_cpu_info *info = &per_cpu(vortex_info, policy->cpu);

    vortex_eval_freq(policy);
    /* High-priority wq: sampling must not starve behind boot-time kworkers */
    queue_delayed_work_on(policy->cpu, system_highpri_wq, &vpinfo->work,
                          msecs_to_jiffies(info->next_delay_ms));
}

/* ========================================================================
 * GKI 5.10 HYBRID API STRUCT (Strictly UNCHANGED)
 * ======================================================================== */
static int vortex_init(struct cpufreq_policy *policy)
{
    struct vortex_policy_info *vpinfo = kzalloc(sizeof(*vpinfo), GFP_KERNEL);
    if (!vpinfo)
        return -ENOMEM;
    vpinfo->policy = policy;
    INIT_DEFERRABLE_WORK(&vpinfo->work, vortex_work_handler);
    policy->governor_data = vpinfo;
    return 0;
}

static void vortex_exit(struct cpufreq_policy *policy)
{
    struct vortex_policy_info *vpinfo = policy->governor_data;
    if (vpinfo) {
        cancel_delayed_work_sync(&vpinfo->work);
        kfree(vpinfo);
        policy->governor_data = NULL;
    }
}

static int vortex_start(struct cpufreq_policy *policy)
{
    struct vortex_policy_info *vpinfo = policy->governor_data;
    unsigned int cpu;

    for_each_cpu(cpu, policy->cpus) {
        struct vortex_cpu_info *info = &per_cpu(vortex_info, cpu);
        info->prev_cpu_idle = get_cpu_idle_time(cpu, &info->prev_cpu_wall, 0);
        info->last_load = 0;
        info->target_freq = policy->cur;
        info->thermal_counter = 0;
        info->max_hold_counter = 0; /* Init hold state */
        info->next_delay_ms = 10;
    }

    vpinfo->boot_until = jiffies + msecs_to_jiffies(boot_boost_ms);
    queue_delayed_work_on(policy->cpu, system_highpri_wq, &vpinfo->work,
                          msecs_to_jiffies(10));
    return 0;
}

static void vortex_stop(struct cpufreq_policy *policy)
{
    cancel_delayed_work_sync(&((struct vortex_policy_info *)policy->governor_data)->work);
}

static void vortex_limits(struct cpufreq_policy *policy)
{
    vortex_eval_freq(policy);
}

static struct cpufreq_governor vortex_gov = {
    .name		= "vortexcore",
    .owner		= THIS_MODULE,
    .init		= vortex_init,
    .exit		= vortex_exit,
    .start		= vortex_start,
    .stop		= vortex_stop,
    .limits		= vortex_limits,
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
MODULE_DESCRIPTION("VortexCore v3.3 - Boot Smooth + Anti Parachute Max Hold");
MODULE_LICENSE("GPL");
