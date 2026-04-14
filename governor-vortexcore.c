// SPDX-License-Identifier: GPL-2.0
/*
 * VortexCore CPU Governor v3.1 (Gaming Stability Fix)
 * Engineered for GKI 5.10 (Hybrid API)
 * Features: Adaptive Sampling, Proactive Thermal, Refined big.LITTLE
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
    unsigned int next_delay_ms;
};

static DEFINE_PER_CPU(struct vortex_cpu_info, vortex_info);

struct vortex_policy_info {
    struct delayed_work work;
    struct cpufreq_policy *policy;
};

/* ========================================================================
 * VORTEXCORE v3.1 CORE LOGIC
 * ======================================================================== */
static void vortex_eval_freq(struct cpufreq_policy *policy)
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
        info->next_delay_ms = 20;
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

    /* 4. Refined big.LITTLE Awareness */
    bool is_big = (policy->cpuinfo.max_freq > (policy->cpuinfo.min_freq * 2));
    unsigned int dyn_target_load = is_big ? target_load_big : target_load_little;

    /* Decision Matrix */
    if (load >= fast_ramp_up_load) {
        freq_target = thermal_max;
    } else if (load > dyn_target_load) {
        unsigned int freq_adj = thermal_max * load / 100;
        freq_target = max(freq_adj, current_freq);
    } else {
        if (current_freq > policy->min) {
            unsigned int freq_diff = current_freq - policy->min;
            /* FIX 1: Less aggressive ramp-down (Changed /10 to /20 = 5% decay) */
            unsigned int decay_step = max(policy->min / 100, freq_diff / 20);
            freq_target = (current_freq > policy->min + decay_step) ? current_freq - decay_step : policy->min;
        } else {
            freq_target = policy->min;
        }
    }

    /* FIX 3: Proactive Thermal (Increased threshold from 10 to 20) */
    if (freq_target >= thermal_max) {
        info->thermal_counter++;
        if (info->thermal_counter > 20) {
            freq_target = (thermal_max * 95) / 100;
        }
    } else {
        if (info->thermal_counter > 0)
            info->thermal_counter--;
    }

    /* Safer Frequency Call (Strict Jitter Prevention) */
    if (freq_target != info->target_freq) {
        info->target_freq = freq_target;
        __cpufreq_driver_target(policy, freq_target, CPUFREQ_RELATION_L);
    }

    /* FIX 2: Faster ramp-up response (Max delay reduced from 40ms to 20ms) */
    if (load >= fast_ramp_up_load || load > dyn_target_load) {
        info->next_delay_ms = 10; /* 10ms for gaming/load */
    } else if (current_freq == policy->min) {
        info->next_delay_ms = 20; /* Fixed: was 40ms, now 20ms for faster spike response */
    } else {
        info->next_delay_ms = 20; /* 20ms for daily use */
    }
}

static void vortex_work_handler(struct work_struct *work)
{
    struct vortex_policy_info *vpinfo = container_of(work, struct vortex_policy_info, work.work);
    struct cpufreq_policy *policy = vpinfo->policy;
    struct vortex_cpu_info *info = &per_cpu(vortex_info, policy->cpu);

    vortex_eval_freq(policy);
    schedule_delayed_work_on(policy->cpu, &vpinfo->work, msecs_to_jiffies(info->next_delay_ms));
}

/* ========================================================================
 * GKI 5.10 HYBRID API STRUCT (Strictly matched)
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
    unsigned int cpu;
    for_each_cpu(cpu, policy->cpus) {
        struct vortex_cpu_info *info = &per_cpu(vortex_info, cpu);
        info->prev_cpu_idle = get_cpu_idle_time(cpu, &info->prev_cpu_wall, 0);
        info->target_freq = policy->cur;
        info->thermal_counter = 0;
        info->next_delay_ms = 20;
    }
    schedule_delayed_work_on(policy->cpu, &((struct vortex_policy_info *)policy->governor_data)->work, msecs_to_jiffies(20));
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
MODULE_DESCRIPTION("VortexCore v3.1 - Gaming Stability Fix");
MODULE_LICENSE("GPL");
