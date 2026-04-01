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

// External kernel variables to patch
extern int panic_timeout;
extern int panic_on_oops;
extern int panic_on_rcu_stall;
extern int panic_on_warn;
extern int console_loglevel;
extern int sysctl_sched_tunable_scaling;

// Array for LMK minfree (in pages, typically 1 page = 4KB)
// Original script: 2560,5120,11520,25600,35840,38400 (in KB)
// Converted to pages (divided by 4): 640, 1280, 2880, 6400, 8960, 9600
static int vortex_minfree[6] = {640, 1280, 2880, 6400, 8960, 9600};

// Helper function to change TCP Congestion Control internally
static void vortex_set_tcp_congestion(const char *name) {
    struct tcp_congestion_ops *ops;
    rcu_read_lock();
    ops = tcp_ca_find(name);
    if (ops && try_module_get(ops->owner)) {
        tcp_set_default_congestion_control(ops);
        pr_info("VorteX: TCP Congestion set to %s\n", name);
        module_put(ops->owner);
    } else {
        pr_warn("VorteX: TCP Congestion '%s' not found/compiled in kernel\n", name);
    }
    rcu_read_unlock();
}

static int __init vortex_kernel_init(void) {
    pr_info("VorteX Esport: Initializing Kernel Space Optimizations\n");

    // ========== PANIC & PRINTK ==========
    panic_timeout = 0;
    panic_on_oops = 0;
    panic_on_rcu_stall = 0;
    panic_on_warn = 0;
    
    // Mute console loglevel (equivalent to echo "0 0 0 0")
    console_loglevel = CONSOLE_LOGLEVEL_SILENT; // = 0

    // ========== SCHEDULER INTERNALS ==========
    // Equivalent to echo "0" > sched_tunable_scaling
    sysctl_sched_tunable_scaling = 0;

    // Modify LMK Minfree sysctl directly in kernel memory
    // (No need to write to /sys/module/lowmemorykiller/parameters/minfree)
    #ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER
    extern int lowmem_minfree_size;
    if (lowmem_minfree_size >= 6) {
        memcpy(lowmem_minfree, vortex_minfree, sizeof(vortex_minfree));
        pr_info("VorteX: LMK minfree patched directly in kernel memory\n");
    }
    #endif

    // ========== TCP CONGESTION ==========
    // Make sure CONFIG_TCP_CONG_WESTWOOD=y is set in your kernel defconfig
    vortex_set_tcp_congestion("westwood");

    pr_info("VorteX: Kernel Patching Complete\n");
    return 0;
}

static void __exit vortex_kernel_exit(void) {
    pr_info("VorteX Esport: Unloaded\n");
}

module_init(vortex_kernel_init);
module_exit(vortex_kernel_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("VorteX Esport");
MODULE_DESCRIPTION("GKI Kernel Boot Optimizations");
// Signed-off-by: kingfinix98@gmail.com
