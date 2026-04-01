#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/string.h>
#include <linux/err.h>

// Fungsi untuk menulis ke sysfs (Sama seperti echo di terminal)
static void vortex_write_sysfs(const char *path, const char *val) {
    struct file *file;
    loff_t pos = 0;
    
    file = filp_open(path, O_WRONLY, 0);
    if (!IS_ERR(file)) {
        kernel_write(file, val, strlen(val), &pos);
        filp_close(file, NULL);
    }
}

// Thread yang dijalankan saat booting
static int vortex_gpu_thread(void *data) {
    // Tunggu 5 detik agar node sysfs KGSL/devfreq benar-benar siap
    ssleep(5);

    pr_info("[VorteX] Starting GPU Performance Tuning...\n");

    // ==========================================
    // 1. FREKUENSI LOCK (940 MHz Puncak Aman)
    // ==========================================
    vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/min_freq", "940000000");
    vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/max_freq", "940000000");
    vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/devfreq/governor", "performance");
    vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/max_gpuclk", "940000");

    // ==========================================
    // 2. CUSTOM BUS & CACHE OPTIMIZATION
    // ==========================================
    vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/force_bus_on", "1");
    vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/gpu_llc_slice_enable", "1");
    vortex_write_sysfs("/sys/class/kgsl/kgsl-3d0/l3_vote", "1");

    pr_info("[VorteX] GPU Tuning Applied!\n");
    return 0;
}

// Dipanggil otomatis oleh kernel saat booting
static int __init vortex_gpu_init(void) {
    kthread_run(vortex_gpu_thread, NULL, "vortex_gpu");
    return 0;
}

late_initcall(vortex_gpu_init);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("VorteX");
MODULE_DESCRIPTION("VorteX GPU Performance Tuning");
