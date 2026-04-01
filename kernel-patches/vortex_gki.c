#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/panic.h>
#include <linux/sched/sysctl.h>
#include <linux/mm.h>
#include <linux/sysctl.h>
#include <linux/tcp.h>
#include <net/sock.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/string.h>
#include <linux/err.h>

// ==========================================
// 1. DIRECT KERNEL MEMORY PATCH (Instant)
// ==========================================

extern int panic_timeout;
extern int panic_on_oops;
extern int panic_on_rcu_stall;
extern int panic_on_warn;
extern int console_loglevel;
extern int sysctl_sched_tunable_scaling;

// Helper to set TCP Congestion internally
static void vortex_set_tcp_congestion(const char *name) {
    struct tcp_congestion_ops *ops;
    rcu_read_lock();
    ops = tcp_ca_find(name);
    if (ops && try_module_get(ops->owner)) {
        tcp_set_default_congestion_control(ops);
        pr_info("[VorteX] TCP Congestion set to %s\n", name);
        module_put(ops->owner);
    } else {
        pr_warn("[VorteX] TCP Congestion '%s' not found\n", name);
    }
    rcu_read_unlock();
}

static int __init vortex_direct_init(void) {
    pr_info("[VorteX] Applying Direct Kernel Patches...\n");

    // Panic & Printk
    panic_timeout = 0;
    panic_on_oops = 0;
    panic_on_rcu_stall = 0;
    panic_on_warn = 0;
    console_loglevel = CONSOLE_LOGLEVEL_SILENT; // 0

    // Scheduler
    sysctl_sched_tunable_scaling = 0;

    // TCP
    vortex_set_tcp_congestion("westwood");

    return 0;
}

// ==========================================
// 2. SYSFS PATCHER (GPU, I/O, LMK via Thread)
// ==========================================

static void vortex_write_sysfs(const char *path, const char *val) {
    struct file *file;
    loff_t pos = 0;
    
    file = filp_open(path, O_WRONLY, 0);
    if (!IS_ERR(file)) {
        kernel_write(file, val, strlen(val), &pos);
        filp_close(file, NULL);
    }
}

static int vortex_sysfs_thread(void *data) {
    // Wait until Android system mounts all partitions
    ssleep(8);

    pr_info("[VorteX] Starting Sysfs Tuning...\n");

    // --- GPU TUNING (Lock 940MHz + Custom Bus) ---
    vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/min_freq", "1010000000");
    vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/max_freq", "1010000000");
    vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/governor", "performance");
    vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/max_gpuclk", "1010000000");
    
    vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/force_bus_on", "1");
    vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/gpu_llc_slice_enable", "1");
    vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/l3_vote", "1");

    // --- LMK MINFREE (Bug Fix: No longer using memcpy) ---
    vortex_write_sysfs("/sys/module/lowmemorykiller/parameters/minfree", "2560,5120,11520,25600,35840,38400");

    // --- I/O TUNING (Anti-stutter during loading) ---
    vortex_write_sysfs("/sys/block/sda/queue/scheduler", "deadline");
    vortex_write_sysfs("/sys/block/sda/queue/read_ahead_kb", "128");
    vortex_write_sysfs("/sys/block/sda/queue/iostats", "0");
    
    vortex_write_sysfs("/sys/block/dm-0/queue/scheduler", "deadline");
    vortex_write_sysfs("/sys/block/dm-0/queue/read_ahead_kb", "128");
    vortex_write_sysfs("/sys/block/dm-0/queue/iostats", "0");

    pr_info("[VorteX] All Tuning Applied Successfully!\n");
    return 0;
}

static int __init vortex_sysfs_init(void) {
    kthread_run(vortex_sysfs_thread, NULL, "vortex_sysfs");
    return 0;
}

// ==========================================
// 3. MODULE REGISTRATION
// ==========================================

// Direct patch executed first
pure_initcall(vortex_direct_init);

// Sysfs patch executed after device tree is ready
late_initcall(vortex_sysfs_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("VorteX Esport");
MODULE_DESCRIPTION("GKI 5.10 Ultimate Kernel Patch");
// Signed-off-by: kingfinix98@gmail.com
