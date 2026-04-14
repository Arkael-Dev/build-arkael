// SPDX-License-Identifier: GPL-2.0
#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

struct vortex_policy_info {
    struct delayed_work work;
    struct cpufreq_policy *policy;
};

static void vortex_work_handler(struct work_struct *work)
{
    struct vortex_policy_info *vpinfo = container_of(work, struct vortex_policy_info, work.work);
    struct cpufreq_policy *policy = vpinfo->policy;
    unsigned int freq = policy->cur;
    
    if (freq < policy->max)
        freq += 100000;
        
    __cpufreq_driver_target(policy, freq, CPUFREQ_RELATION_L);
    schedule_delayed_work_on(vpinfo->policy->cpu, &vpinfo->work, msecs_to_jiffies(20));
}

static int vortex_governor(struct cpufreq_policy *policy, unsigned int event)
{
    struct vortex_policy_info *vpinfo;

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
        schedule_delayed_work_on(policy->cpu, &vpinfo->work, msecs_to_jiffies(20));
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

static struct cpufreq_governor vortex_gov_struct = {
    .name		= "vortexcore",
    .owner		= THIS_MODULE,
    .governor	= vortex_governor,
};

static int __init vortex_init(void)
{
    return cpufreq_register_governor(&vortex_gov_struct);
}

static void __exit vortex_exit(void)
{
    cpufreq_unregister_governor(&vortex_gov_struct);
}

module_init(vortex_init);
module_exit(vortex_exit);

MODULE_AUTHOR("Kingfinik98");
MODULE_DESCRIPTION("VortexCore Governor");
MODULE_LICENSE("GPL");
