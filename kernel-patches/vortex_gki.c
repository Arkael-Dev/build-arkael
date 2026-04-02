#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/sched/sysctl.h>
#include <linux/mm.h>
#include <linux/sysctl.h>
#include <linux/tcp.h>
#include <net/tcp.h>
#include <net/sock.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/string.h>
#include <linux/err.h>

extern struct net init_net;

// ==========================================
// 1. DIRECT KERNEL MEMORY PATCH (Instant)
// ==========================================

extern int panic_timeout;
extern int panic_on_oops;
extern int panic_on_warn;
extern int console_loglevel;
extern enum sched_tunable_scaling sysctl_sched_tunable_scaling;

static int __init vortex_direct_init(void) {
    pr_info("[VorteX] Applying safe Direct Kernel Patches...\n");

    panic_timeout = 0;
    panic_on_oops = 0;
    panic_on_warn = 0;
    console_loglevel = CONSOLE_LOGLEVEL_SILENT;
    sysctl_sched_tunable_scaling = 0;

    return 0;
}

// ==========================================
// 2. ULTRA-SAFE SYSFS ENGINE
// ==========================================

// Function: Safe Write with strict validation
static bool vortex_write_sysfs(const char *path, const char *val) {
    struct file *file;
    loff_t pos = 0;
    ssize_t ret;

    if (!path || !val) return false;

    file = filp_open(path, O_WRONLY, 0);
    if (IS_ERR_OR_NULL(file)) {
        return false; // Path doesn't exist or no permission
    }
    
    ret = kernel_write(file, val, strlen(val), &pos);
    filp_close(file, NULL);
    
    if (ret < 0) {
        pr_warn("[VorteX] REJECTED: %s (Value '%s' is invalid)\n", path, val);
        return false;
    }
    return true;
}

// Function: Safe Read
static bool vortex_read_sysfs(const char *path, char *buf, size_t buflen) {
    struct file *file;
    loff_t pos = 0;
    ssize_t ret;

    if (!path || !buf || buflen == 0) return false;
    buf[0] = '\0';

    file = filp_open(path, O_RDONLY, 0);
    if (IS_ERR_OR_NULL(file)) return false;

    ret = kernel_read(file, buf, buflen - 1, &pos);
    filp_close(file, NULL);

    if (ret > 0) {
        buf[ret] = '\0';
        char *newline = strchr(buf, '\n');
        if (newline) *newline = '\0';
        return true;
    }
    return false;
}

// Function: Safe TCP Setter
static void vortex_set_tcp_congestion(const char *name) {
    struct tcp_congestion_ops *ops;
    rcu_read_lock();
    ops = tcp_ca_find(name);
    if (ops && try_module_get(ops->owner)) {
        tcp_set_default_congestion_control(&init_net, name);
        pr_info("[VorteX] TCP: Successfully forced to %s\n", name);
        module_put(ops->owner);
    } else {
        pr_warn("[VorteX] TCP: Failed to set %s (Module not compiled in defconfig?)\n", name);
    }
    rcu_read_unlock();
}

// ==========================================
// 3. SAFE SYSFS THREAD (Dynamic Discovery)
// ==========================================

static int vortex_sysfs_thread(void *data) {
    char path[128];
    char max_freq_val[32] = {0};
    char current_gov[32] = {0};
    int i;

    // Wait until init.rc and mount points are fully settled
    ssleep(15); 

    pr_info("[VorteX] Initializing Ultra-Safe Sysfs Tuning...\n");

    // --- 1. TCP ---
    vortex_set_tcp_congestion("westwood");

    // --- 2. CPU GOVERNOR (Universal for Snapdragon & MediaTek GKI) ---
    pr_info("[VorteX] Forcing CPU Governor to performance...\n");
    for (i = 0; i <= 15; i++) {
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpufreq/policy%d/scaling_governor", i);
        if (vortex_write_sysfs(path, "performance")) {
            pr_info("[VorteX] CPU: Policy %d set to performance\n", i);
        }
    }

    // --- 3. DYNAMIC I/O SCHEDULER (Smart Scan) ---
    pr_info("[VorteX] Scanning block devices for I/O tuning...\n");
    // Scan physical partitions (sda - sdz)
    for (i = 'a'; i <= 'z'; i++) {
        snprintf(path, sizeof(path), "/sys/block/sd%c/queue/scheduler", i);
        if (vortex_write_sysfs(path, "mq-deadline")) {
            pr_info("[VorteX] I/O: Patched sd%c to mq-deadline\n", i);
        }
        snprintf(path, sizeof(path), "/sys/block/sd%c/queue/read_ahead_kb", i);
        vortex_write_sysfs(path, "128");
        snprintf(path, sizeof(path), "/sys/block/sd%c/queue/iostats", i);
        vortex_write_sysfs(path, "0");
    }
    // Scan logical partitions (dm-0 to dm-15)
    for (i = 0; i <= 15; i++) {
        snprintf(path, sizeof(path), "/sys/block/dm-%d/queue/scheduler", i);
        if (vortex_write_sysfs(path, "mq-deadline")) {
            pr_info("[VorteX] I/O: Patched dm-%d to mq-deadline\n", i);
        }
        snprintf(path, sizeof(path), "/sys/block/dm-%d/queue/read_ahead_kb", i);
        vortex_write_sysfs(path, "128");
        snprintf(path, sizeof(path), "/sys/block/dm-%d/queue/iostats", i);
        vortex_write_sysfs(path, "0");
    }

    // --- 4. GPU TUNING (Read actual hardware limits first) ---
    if (vortex_read_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/max_freq", max_freq_val, sizeof(max_freq_val))) {
        pr_info("[VorteX] GPU: Detected hardware max frequency: %s Hz\n", max_freq_val);
        
        // Inject the actual hardware max into min_freq to lock it
        if (vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/min_freq", max_freq_val)) {
            pr_info("[VorteX] GPU: Min/Max frequency LOCKED to %s\n", max_freq_val);
        }
        vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/max_gpuclk", max_freq_val);
        
        if (vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/governor", "performance")) {
            pr_info("[VorteX] GPU: Governor set to performance\n");
        } else {
            // Fallback logic if "performance" was rejected by thermal config
            vortex_read_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/governor", current_gov, sizeof(current_gov));
            pr_warn("[VorteX] GPU: Performance governor blocked by system. Current: %s\n", current_gov);
        }

        // Bus & Cache tweaks
        vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/force_bus_on", "1");
        vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/gpu_llc_slice_enable", "1");
        vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/l3_vote", "1");
    } else {
        pr_warn("[VorteX] GPU: KGSL node not found. Is this a non-Qualcomm device or KGSL disabled?\n");
    }

    // --- 5. LMK MINFREE ---
    if (vortex_write_sysfs("/sys/module/lowmemorykiller/parameters/minfree", "2560,5120,11520,25600,35840,38400")) {
        pr_info("[VorteX] LMK: Minfree values updated successfully\n");
    } else {
        pr_warn("[VorteX] LMK: Node not found. LMK might be replaced by lmkd in userspace.\n");
    }

    pr_info("[VorteX] =======================================\n");
    pr_info("[VorteX] Safe Tuning Sequence Completed.\n");
    pr_info("[VorteX] Check warnings above for blocked items.\n");
    pr_info("[VorteX] =======================================\n");
    
    return 0;
}

static int __init vortex_sysfs_init(void) {
    struct task_struct *thread;
    thread = kthread_run(vortex_sysfs_thread, NULL, "vortex_sysfs");
    if (IS_ERR(thread)) {
        pr_err("[VorteX] FATAL: Failed to create tuning thread!\n");
    }
    return 0;
}

// ==========================================
// 4. MODULE REGISTRATION
// ==========================================

pure_initcall(vortex_direct_init);
late_initcall(vortex_sysfs_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("VorteX Esport");
MODULE_DESCRIPTION("GKI 5.10 Ultra-Safe Kernel Patch");
// Signed-off-by: kingfinix98@gmail.com
