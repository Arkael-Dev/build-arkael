// SPDX-License-Identifier: GPL-2.0
/*

VortexMax CPU Governor (Stable Aggressive)

Engineered for GKI 5.10, 6.1, 6.6 (Hybrid API)

Philosophy: Peak performance without thermal suicide.

Waits for actual load before striking, drops to MIN to allow cooldown.

Author: kingfinix98@gmail.com
*/


#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/sched/cpufreq.h>
#include <linux/tick.h>
#include <linux/workqueue.h>

/*

Threshold: CPU load percentage to trigger MAX frequency.

5% is enough to detect gaming/heavy tasks without triggering on background noise.
*/
static unsigned int max_threshold = 5;
module_param_named(max_threshold, max_threshold, uint, 0644);


/*

Down Delay: How many sampling loops to hold MAX before dropping to MIN.

Prevents oscillation when CPU load fluctuates around the threshold.
*/
static unsigned int down_delay = 2;
module_param_named(down_delay, down_delay, uint, 0644);


struct vortex_cpu_info {
    u64 prev_idle;
    u64 prev_wall;
};

static DEFINE_PER_CPU(struct vortex_cpu_info, vortex_info);

struct vortex_policy_info {
    struct delayed_work work;
    struct cpufreq_policy *policy;
    unsigned int hold_counter;
};

/*

Binary scaling governor:

Instant jump to max saat load naik


Hold max untuk sementara (anti oscillation saat load fluktuatif)


Turun ke min untuk cooldown


Fokus: low latency & fast response (gaming oriented)
*/
static void vortex_eval_freq(struct cpufreq_policy *policy)
{
    /* CRITICAL FIX: Prevent crash from uninitialized or race-condition pointers */
    if (!policy || !policy->governor_data)
        return;

    struct vortex_policy_info *vp = policy->governor_data;
    struct vortex_cpu_info *info = &per_cpu(vortex_info, policy->cpu);
    u64 idle, wall;
    u64 delta_idle, delta_wall;
    unsigned int load, target;

    idle = get_cpu_idle_time(policy->cpu, &wall, 0);

    if (!info->prev_wall) {
        info->prev_wall = wall;
        info->prev_idle = idle;
        return;
    }

    delta_wall = wall - info->prev_wall;
    delta_idle = idle - info->prev_idle;

    info->prev_wall = wall;
    info->prev_idle = idle;

    if (delta_wall == 0 || delta_idle > delta_wall)
        load = 0;
    else
        load = div64_u64(100 * (delta_wall - delta_idle), delta_wall);

    /* VortexMax Logic with Anti-Oscillation Hold */
    if (load > max_threshold) {
        target = policy->max;
        vp->hold_counter = down_delay; /* Activate hold */
    } else {
        if (vp->hold_counter > 0) {
            vp->hold_counter--; /* Keep holding MAX */
            target = policy->max;
        } else {
            target = policy->min; /* Cooldown: Instant Rest */
        }
    }

    if (target != policy->cur) {
        __cpufreq_driver_target(policy, target, CPUFREQ_RELATION_H);
    }
}


static void vortex_work(struct work_struct *work)
{
    struct vortex_policy_info *vp =
        container_of(work, struct vortex_policy_info, work.work);

    if (!vp || !vp->policy)
        return;

    vortex_eval_freq(vp->policy);

    /* 10ms is aggressive enough for gaming, but safe for CPU overhead */
    schedule_delayed_work_on(
        vp->policy->cpu,
        &vp->work,
        msecs_to_jiffies(10)
    );
}

static int vortex_init(struct cpufreq_policy *policy)
{
    struct vortex_policy_info *vp;

    vp = kzalloc(sizeof(*vp), GFP_KERNEL);
    if (!vp)
        return -ENOMEM;

    vp->policy = policy;
    vp->hold_counter = 0; /* Init hold state */
    policy->governor_data = vp;

    INIT_DEFERRABLE_WORK(&vp->work, vortex_work);

    return 0;
}

static void vortex_exit(struct cpufreq_policy *policy)
{
    struct vortex_policy_info *vp = policy->governor_data;

    if (!vp)
        return;

    cancel_delayed_work_sync(&vp->work);
    kfree(vp);
    policy->governor_data = NULL;
}

static int vortex_start(struct cpufreq_policy *policy)
{
    struct vortex_policy_info *vp = policy->governor_data;

    if (!vp)
        return -EINVAL;

    schedule_delayed_work_on(policy->cpu, &vp->work, 0);
    return 0;
}

static void vortex_stop(struct cpufreq_policy *policy)
{
    struct vortex_policy_info *vp = policy->governor_data;

    if (vp)
        cancel_delayed_work_sync(&vp->work);
}

static void vortex_limits(struct cpufreq_policy *policy)
{
    vortex_eval_freq(policy);
}

static struct cpufreq_governor vortex_gov = {
    .name		= "vortexmax",
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

MODULE_AUTHOR("kingfinix98@gmail.com");
MODULE_DESCRIPTION("VortexMax - Stable Aggressive Peak Performance Governor");
MODULE_LICENSE("GPL");
