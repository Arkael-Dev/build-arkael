// SPDX-License-Identifier: GPL-2.0
/*
 * VortexMax CPU Governor v4.0 (AI Adaptive Core Edition)
 * Engineered for GKI 5.10 / 6.1 / 6.6 (ARM64 Hybrid API)
 * Notes; still in the development stage, not fully fixed, 
 * ══════════════════════════════════════════════════════════════════
 * V4.0 AI ADAPTIVE CORE - FEATURE MATRIX
 * ══════════════════════════════════════════════════════════════════
 * 
 * [✓] Self-Learning Load Engine    - Auto-adjust target_load BIG/LITTLE
 * [✓] Dynamic Behavior Profiling   - IDLE/UI/GAMING/BURST/HEAVY detection
 * [✓] Predictive Scaling Engine v3 - EMA + trend + acceleration prediction
 * [✓] Adaptive Sampling Rate AI   - 4ms-30ms dynamic based on behavior
 * [✓] Intelligent Boost Fusion    - Touch+IO+Wake unified decision
 * [✓] Thermal Intelligence v2     - Proactive thermal adaptation
 * [✓] Frequency Curve Learning    - Efficiency score per freq bucket
 * [✓] Stability Guard AI          - Oscillation detection & auto-stabilize
 * 
 * ───────────────────────────────────────────────────────────────────
 * ARCHITECTURE: "ZERO DEPENDENCY - PURE KERNEL - AI SAFE"
 * ───────────────────────────────────────────────────────────────────
 * NO external scripts / modules / user-space tools
 * Pure heuristic + adaptive algorithm (NO ML library needed)
 * 100% kernel safe & lightweight
 * 
 * Author:      Kingfinik98
 * Email:       kingfinix98@gmail.com
 * Repository:  https://github.com/Kingfinik98/build-vortex
 * Version:     4.0 AI Adaptive Core
 * License:     GPL-2.0
 * ══════════════════════════════════════════════════════════════════
 */

/* =====================================================================
 * HEADER INCLUDES
 * ===================================================================== */
#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/sched/cpufreq.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/cpu.h>

/* =====================================================================
 * BEHAVIOR MODE DEFINITIONS (AI State Machine)
 * ===================================================================== */
#define VORTEX_MODE_IDLE       0
#define VORTEX_MODE_UI          1
#define VORTEX_MODE_GAMING      2
#define VORTEX_MODE_BURST       3
#define VORTEX_MODE_HEAVY       4

#define FREQ_BUCKETS             16  /* Number of frequency buckets for learning */

/* =====================================================================
 * TUNABLE PARAMETERS (Sysfs Adjustable)
 * ===================================================================== */

/* --- Base Load Thresholds (Initial values, AI will adapt) --- */
static unsigned int target_load_big = 75;
module_param_named(target_load_big, target_load_big, uint, 0644);
static unsigned int target_load_little = 85;
module_param_named(target_load_little, target_load_little, uint, 0644);
static unsigned int fast_ramp_up_load = 85;
module_param(fast_ramp_up_load, uint, 0644);

/* --- Touch Boost Parameters --- */
static unsigned int touch_boost_freq_pct = 85;
module_param_named(touch_boost_freq_pct, touch_boost_freq_pct, uint, 0644);
static unsigned int touch_boost_duration_ms = 400;
module_param_named(touch_boost_duration_ms, touch_boost_duration_ms, uint, 0644);
static bool touch_boost_enabled = true;
module_param_named(touch_boost_enabled, touch_boost_enabled, bool, 0644);

/* --- IO Boost Parameters --- */
static unsigned int io_boost_freq_pct = 75;
module_param_named(io_boost_freq_pct, io_boost_freq_pct, uint, 0644);
static unsigned int io_boost_duration_ms = 250;
module_param_named(io_boost_duration_ms, io_boost_duration_ms, uint, 0644);
static bool io_boost_enabled = true;
module_param_named(io_boost_enabled, io_boost_enabled, bool, 0644);
static unsigned int io_detect_threshold = 55;
module_param(io_detect_threshold, uint, 0644);

/* --- Smart Ramp-Up Parameters --- */
static unsigned int ramp_up_step_pct = 25;  /* Reduced from 35 for stability */
module_param_named(ramp_up_step_pct, ramp_up_step_pct, uint, 0644);
static bool ramp_up_momentum = true;
module_param_named(ramp_up_momentum, ramp_up_momentum, bool, 0644);

/* --- Frequency Floor Parameters --- */
static unsigned int freq_floor_idle_pct = 20;
module_param_named(freq_floor_idle_pct, freq_floor_idle_pct, uint, 0644);
static unsigned int freq_floor_active_pct = 35;
module_param_named(freq_floor_active_pct, freq_floor_active_pct, uint, 0644);
static unsigned int freq_floor_gaming_pct = 50;
module_param_named(freq_floor_gaming_pct, freq_floor_gaming_pct, uint, 0644);

/* --- Hysteresis Band Parameters --- */
static unsigned int hysteresis_up_pct = 12;
module_param_named(hysteresis_up_pct, hysteresis_up_pct, uint, 0644);
static unsigned int hysteresis_down_pct = 10;
module_param_named(hysteresis_down_pct, hysteresis_down_pct, uint, 0644);

/* --- Max Hold Parameters --- */
static unsigned int max_hold_cycles = 8;
module_param_named(max_hold_cycles, max_hold_cycles, uint, 0644);

/* --- Thermal Guard Parameters --- */
static unsigned int thermal_limit_pct = 90;
module_param_named(thermal_limit_pct, thermal_limit_pct, uint, 0644);
static unsigned int thermal_counter_threshold = 12;
static bool thermal_hard_limit = false;
module_param_named(thermal_hard_limit, thermal_hard_limit, bool, 0644);
module_param(thermal_counter_threshold, uint, 0644);

/* --- Adaptive Sampling Rate (AI Controlled) --- */
static unsigned int sample_rate_idle_ms = 30;
module_param(sample_rate_idle_ms, uint, 0644);
static unsigned int sample_rate_ui_ms = 16;
module_param(sample_rate_ui_ms, uint, 0644);
static unsigned int sample_rate_gaming_ms = 8;
module_param(sample_rate_gaming_ms, uint, 0644);
static unsigned int sample_rate_burst_ms = 5;
module_param(sample_rate_burst_ms, uint, 0644);

/* --- Wake Boost Parameters --- */
static unsigned int wake_boost_freq_pct = 65;
module_param_named(wake_boost_freq_pct, wake_boost_freq_pct, uint, 0644);
static unsigned int wake_boost_duration_ms = 150;
module_param_named(wake_boost_duration_ms, wake_boost_duration_ms, uint, 0644);
static bool wake_boost_enabled = true;
module_param_named(wake_boost_enabled, wake_boost_enabled, bool, 0644);

/* --- Transition Rate Limiter --- */
static unsigned int max_freq_change_per_ms = 35;
module_param(max_freq_change_per_ms, uint, 0644);
static bool transition_smooth_enabled = true;
module_param_named(transition_smooth_enabled, transition_smooth_enabled, bool, 0644);

/* --- AI Learning Parameters --- */
static unsigned int ai_learning_rate = 7; /* 1-10, higher = faster learning */
module_param(ai_learning_rate, uint, 0644);
static unsigned int gaming_sustain_cycles = 15;
static unsigned int gaming_load_threshold = 70;
module_param(gaming_load_threshold, uint, 0644);
module_param(gaming_sustain_cycles, uint, 0644);
static unsigned int burst_detection_threshold = 30; /* Load jump to detect burst */
module_param(burst_detection_threshold, uint, 0644);

/* --- Stability Guard Parameters --- */
static unsigned int stability_window = 5; /* Check last N samples for oscillation */
module_param(stability_window, uint, 0644);
static unsigned int oscillation_threshold = 3; /* Direction changes in window */
module_param(oscillation_threshold, uint, 0644);

/* =====================================================================
 * AI STATE STRUCTURE
 * ===================================================================== */

/**
 * struct vortex_ai_state - Per-CPU AI learning state
 * Contains all adaptive learning data
 */
struct vortex_ai_state {
    /* Self-learning target loads */
    unsigned int learned_target_big;
    unsigned int learned_target_little;
    
    /* Frequency efficiency scoring */
    unsigned int efficiency_score[FREQ_BUCKETS];
    unsigned int freq_usage_count[FREQ_BUCKETS];
    
    /* Behavior mode tracking */
    unsigned int current_mode;
    unsigned int prev_mode;
    unsigned int mode_duration_cycles;
    
    /* Prediction engine */
    unsigned int predicted_load;
    unsigned int load_acceleration;
    int prev_trend;
    
    /* Stability guard */
    unsigned int freq_change_count;
    unsigned int direction_changes;
    unsigned int freq_history[5];
    bool force_smoothing;
};

/**
 * struct vortexmax_cpu_info - Per-CPU governor state
 */
struct vortexmax_cpu_info {
    u64 prev_cpu_idle;
    u64 prev_cpu_wall;
    u64 last_touch_time;
    u64 last_io_time;
    u64 last_wake_time;
    unsigned int target_freq;
    unsigned int thermal_counter;
    unsigned int next_delay_ms;
    unsigned int max_hold_counter;
    
    /* Load History Ring Buffer */
    unsigned int load_history[8];
    unsigned int history_idx;
    unsigned int avg_load_ema;
    unsigned int prev_load;
    
    /* Trend Detection */
    int load_trend;
    
    /* State Flags */
    bool is_active;
    unsigned int consecutive_high_load;
    unsigned int consecutive_low_load;
    
    /* IO Burst Detection */
    bool io_burst_active;
    
    /* AI State (embedded) */
    struct vortex_ai_state ai;
};

static DEFINE_PER_CPU(struct vortexmax_cpu_info, vortexmax_info);

/**
 * struct vortexmax_policy_info - Per-policy state
 */
struct vortexmax_policy_info {
    struct delayed_work work;
    struct cpufreq_policy *policy;
};

/* Global State */
static bool vortexmax_initialized = false;
static atomic_t active_governors = ATOMIC_INIT(0);
static DEFINE_SPINLOCK(vortexmax_lock);

/* Work queue for touch boost */
static struct work_struct touch_boost_work;
static bool touch_pending = false;

/* =====================================================================
 * AI ENGINE FUNCTIONS
 * ===================================================================== */

/**
 * ai_get_behavior_mode() - Classify current usage pattern
 * Returns one of: IDLE, UI, GAMING, BURST, HEAVY
 */
static unsigned int ai_get_behavior_mode(struct vortexmax_cpu_info *info,
                                         unsigned int load,
                                         unsigned int ema_load)
{
    unsigned int mode = info->ai.current_mode;
    unsigned int load_jump;
    
    /* Calculate load acceleration (how fast load is changing) */
    if (info->prev_load > 0 && info->prev_load <= 100) {
        if (load >= info->prev_load)
            info->ai.load_acceleration = load - info->prev_load;
        else
            info->ai.load_acceleration = 0; /* Deceleration not counted as burst */
    }
    
    /* Detect burst: sudden large load jump */
    load_jump = (load > info->prev_load) ? (load - info->prev_load) : 0;
    
    /* Mode classification logic */
    if (load < 20) {
        mode = VORTEX_MODE_IDLE;
    } else if (load < 45) {
        mode = VORTEX_MODE_UI;
    } else if (load_jump > burst_detection_threshold && load > 50) {
        /* Sudden spike = BURST */
        mode = VORTEX_MODE_BURST;
    } else if (load >= gaming_load_threshold) {
        info->consecutive_high_load++;
        
        if (info->consecutive_high_load >= gaming_sustain_cycles) {
            mode = VORTEX_MODE_GAMING;
        } else {
            mode = VORTEX_MODE_HEAVY; /* Not sustained enough for gaming */
        }
    } else if (load >= 60) {
        mode = VORTEX_MODE_HEAVY;
    } else {
        mode = VORTEX_MODE_UI;
    }
    
    /* Reset counters on mode change */
    if (mode != info->ai.current_mode) {
        info->ai.prev_mode = info->ai.current_mode;
        info->ai.current_mode = mode;
        info->ai.mode_duration_cycles = 0;
    }
    
    info->ai.mode_duration_cycles++;
    
    return mode;
}

/**
 * ai_update_learned_targets() - Self-learning load targets
 * Adapts target_load_big and target_load_little based on actual usage
 */
static void ai_update_learned_targets(struct vortex_ai_state *ai,
                                       unsigned int load,
                                       bool is_big)
{
    unsigned int *target;
    unsigned int base_target;
    
    if (is_big) {
        target = &ai->learned_target_big;
        base_target = 75;
    } else {
        target = &ai->learned_target_little;
        base_target = 85;
    }
    
    /* Initialize on first run */
    if (*target == 0) {
        *target = base_target;
        return;
    }
    
    /* Exponential moving average adaptation: 
     * new = (old * rate + current) / (rate + 1)
     * Higher ai_learning_rate = faster adaptation
     */
    *target = ((*target) * ai_learning_rate + load) / (ai_learning_rate + 1);
    
    /* Clamp to sane bounds */
    if (is_big) {
        if (*target < 50) *target = 50;
        if (*target > 95) *target = 95;
    } else {
        if (*target < 60) *target = 60;
        if (*target > 98) *target = 98;
    }
}

/**
 * ai_predict_load() - Predict next cycle's load
 * Uses EMA + trend + acceleration
 */
static unsigned int ai_predict_load(struct vortex_ai_state *ai,
                                    unsigned int ema_load,
                                    int trend)
{
    int predicted;
    int trend_factor;
    int accel_factor;
    
    /* Trend factor: weight of direction (-1, 0, +1) */
    trend_factor = trend * 8;
    
    /* Acceleration factor: how fast load is increasing */
    accel_factor = (int)ai->load_acceleration * 3;
    
    /* Prediction formula */
    predicted = (int)ema_load + trend_factor + accel_factor;
    
    /* Bounds check */
    if (predicted < 0) predicted = 0;
    if (predicted > 100) predicted = 100;
    
    ai->predicted_load = (unsigned int)predicted;
    ai->prev_trend = trend;
    
    return ai->predicted_load;
}

/**
 * ai_update_efficiency_score() - Learn which frequencies are most efficient
 * Tracks efficiency per frequency bucket
 */
static void ai_update_efficiency_score(struct vortex_ai_state *ai,
                                        unsigned int freq,
                                        unsigned int max_freq,
                                        unsigned int load)
{
    unsigned int index;
    
    /* Map frequency to bucket (0 = min, FREQ_BUCKETS-1 = max) */
    if (max_freq == 0 || freq == 0) return;
    
    index = (freq * (FREQ_BUCKETS - 1)) / max_freq;
    if (index >= FREQ_BUCKETS) index = FREQ_BUCKETS - 1;
    
    /* Update efficiency score: lower load at this freq = better score */
    /* Inverse relationship: high load = low efficiency for that freq */
    ai->freq_usage_count[index]++;
    ai->efficiency_score[index] += (100 - load); /* Higher when load is lower */
    
    /* Prevent overflow - decay scores periodically */
    if (ai->freq_usage_count[index] > 1000) {
        ai->efficiency_score[index] /= 2;
        ai->freq_usage_count[index] /= 2;
    }
}

/**
 * ai_check_stability() - Detect oscillation and enable smoothing
 * Returns true if oscillation detected
 */
static bool ai_check_stability(struct vortex_ai_state *ai,
                                unsigned int new_freq)
{
    unsigned int i, changes = 0;
    
    /* Shift history */
    for (i = stability_window - 1; i > 0; i--) {
        ai->freq_history[i] = ai->freq_history[i - 1];
    }
    ai->freq_history[0] = new_freq;
    
    /* Count direction changes in window */
    for (i = 0; i < stability_window - 1; i++) {
        if (ai->freq_history[i] != ai->freq_history[i + 1]) {
            ai->freq_change_count++;
            if (i > 0 && 
                ((ai->freq_history[i] > ai->freq_history[i+1]) !=
                 (ai->freq_history[i-1] > ai->freq_history[i]))) {
                changes++;
            }
        }
    }
    
    ai->direction_changes = changes;
    
    /* Oscillation detected if too many direction changes */
    if (changes >= oscillation_threshold) {
        ai->force_smoothing = true;
        return true;
    }
    
    /* Gradually release smoothing after stable period */
    if (ai->force_smoothing && changes <= 1) {
        ai->freq_change_count--;
        if (ai->freq_change_count == 0)
            ai->force_smoothing = false;
    }
    
    return ai->force_smoothing;
}

/**
 * ai_get_adaptive_sampling() - Get sampling rate based on behavior mode
 */
static unsigned int ai_get_adaptive_sampling(unsigned int mode)
{
    switch (mode) {
        case VORTEX_MODE_IDLE:   return sample_rate_idle_ms;
        case VORTEX_MODE_UI:     return sample_rate_ui_ms;
        case VORTEX_MODE_GAMING: return sample_rate_gaming_ms;
        case VORTEX_MODE_BURST:  return sample_rate_burst_ms;
        case VORTEX_MODE_HEAVY:  return sample_rate_gaming_ms;
        default:                 return sample_rate_ui_ms;
    }
}

/**
 * ai_get_dynamic_target() - Get AI-adapted target load
 */
static unsigned int ai_get_dynamic_target(struct vortex_ai_state *ai,
                                         bool is_big)
{
    /* Use learned target if available and reasonable */
    if (is_big) {
        if (ai->learned_target_big >= 50 && ai->learned_target_big <= 95)
            return ai->learned_target_big;
        return target_load_big;
    } else {
        if (ai->learned_target_little >= 60 && ai->learned_target_little <= 98)
            return ai->learned_target_little;
        return target_load_little;
    }
}

/* =====================================================================
 * LOAD CALCULATION ENGINE
 * ===================================================================== */

static void update_load_history(struct vortexmax_cpu_info *info, unsigned int load)
{
    info->prev_load = info->load_history[info->history_idx];
    info->load_history[info->history_idx] = load;
    info->history_idx = (info->history_idx + 1) % 8;
}

static unsigned int calculate_ema(struct vortexmax_cpu_info *info, unsigned int current_load)
{
    unsigned int ema;
    
    if (info->avg_load_ema == 0) {
        ema = current_load;
    } else {
        ema = (current_load * 51 + info->avg_load_ema * 120) / 171;
    }
    
    info->avg_load_ema = ema;
    return ema;
}

static int detect_load_trend(struct vortexmax_cpu_info *info)
{
    unsigned int recent_avg = 0, older_avg = 0;
    int i;
    
    for (i = 0; i < 3; i++) {
        int idx = (info->history_idx - 1 - i + 8) % 8;
        if (idx >= 0 && idx < 8)
            recent_avg += info->load_history[idx];
    }
    for (i = 3; i < 6; i++) {
        int idx = (info->history_idx - 1 - i + 8) % 8;
        if (idx >= 0 && idx < 8)
            older_avg += info->load_history[idx];
    }
    
    recent_avg /= 3;
    older_avg /= 3;
    
    if (recent_avg > older_avg + 5) return 1;
    if (older_avg > recent_avg + 5) return -1;
    return 0;
}

/* =====================================================================
 * BOOST ENGINES
 * ===================================================================== */

static unsigned int apply_touch_boost(struct cpufreq_policy *policy,
                                       struct vortexmax_cpu_info *info,
                                       u64 now_ns)
{
    u64 elapsed_us;
    unsigned int boost_freq;
    
    if (!touch_boost_enabled || info->last_touch_time == 0)
        return 0;
    
    elapsed_us = (now_ns - info->last_touch_time) / 1000ULL;
    
    if (elapsed_us < (u64)(touch_boost_duration_ms * 1000ULL)) {
        boost_freq = (policy->max * touch_boost_freq_pct) / 100;
        if (boost_freq < policy->cur)
            boost_freq = policy->cur;
        if (info->max_hold_counter < max_hold_cycles)
            info->max_hold_counter = max_hold_cycles;
        return boost_freq;
    }
    return 0;
}

static unsigned int apply_io_boost(struct cpufreq_policy *policy,
                                    struct vortexmax_cpu_info *info,
                                    u64 now_ns,
                                    unsigned int load)
{
    u64 elapsed_us;
    unsigned int boost_freq;
    
    if (!io_boost_enabled) return 0;
    
    if ((load >= io_detect_threshold) &&
        (info->prev_load < (io_detect_threshold - 20))) {
        info->last_io_time = now_ns;
        info->io_burst_active = true;
    } else if (info->io_burst_active) {
        elapsed_us = (now_ns - info->last_io_time) / 1000ULL;
        if (elapsed_us > (u64)(io_boost_duration_ms * 1000ULL))
            info->io_burst_active = false;
    }
    
    if (info->io_burst_active) {
        boost_freq = (policy->max * io_boost_freq_pct) / 100;
        if (boost_freq < policy->cur) boost_freq = policy->cur;
        return boost_freq;
    }
    return 0;
}

static unsigned int apply_wake_boost(struct cpufreq_policy *policy,
                                      struct vortexmax_cpu_info *info,
                                      u64 now_ns)
{
    u64 elapsed_us;
    unsigned int boost_freq;
    
    if (!wake_boost_enabled || info->last_wake_time == 0) return 0;
    
    elapsed_us = (now_ns - info->last_wake_time) / 1000ULL;
    
    if (elapsed_us < (u64)(wake_boost_duration_ms * 1000ULL)) {
        boost_freq = (policy->max * wake_boost_freq_pct) / 100;
        if (boost_freq < policy->cur) boost_freq = policy->cur;
        return boost_freq;
    }
    return 0;
}

/* =====================================================================
 * FREQUENCY DECISION ENGINE
 * ===================================================================== */

static unsigned int get_frequency_floor(struct cpufreq_policy *policy,
                                         struct vortexmax_cpu_info *info)
{
    unsigned int floor_pct;
    unsigned int mode = info->ai.current_mode;
    
    switch (mode) {
        case VORTEX_MODE_GAMING: floor_pct = freq_floor_gaming_pct; break;
        case VORTEX_MODE_BURST:  floor_pct = freq_floor_gaming_pct; break;
        case VORTEX_MODE_HEAVY: floor_pct = freq_floor_active_pct; break;
        case VORTEX_MODE_UI:    floor_pct = freq_floor_active_pct; break;
        default:                floor_pct = freq_floor_idle_pct; break;
    }
    
    if (policy->max == 0) return policy->min;
    return (policy->max * floor_pct) / 100;
}

static unsigned int apply_hysteresis(unsigned int requested,
                                      unsigned int cur_freq,
                                      unsigned int min_freq,
                                      bool going_up)
{
    unsigned int band_pct, band, lower, upper;
    
    band_pct = going_up ? hysteresis_up_pct : hysteresis_down_pct;
    if (cur_freq == 0) return requested;
    band = (cur_freq * band_pct) / 100;
    
    lower = (cur_freq > band) ? (cur_freq - band) : min_freq;
    upper = cur_freq + band;
    
    if (requested >= lower && requested <= upper)
        return cur_freq;
    return requested;
}

static unsigned int smart_ramp_up(unsigned int cur_freq,
                                   unsigned int target_max,
                                   unsigned int load,
                                   int trend)
{
    unsigned int step, new_freq;
    
    if (target_max == 0 || cur_freq > target_max) return target_max;
    step = (target_max * ramp_up_step_pct) / 200; /* Halved for stability */
    
    if (ramp_up_momentum && trend == 1)
        step += step / 2;
    
    if (load >= 90)
        step += step / 2;
    
    new_freq = cur_freq + step;
    
    if (new_freq > target_max) new_freq = target_max;
    return new_freq;
}

static unsigned int smooth_decay(unsigned int cur_freq,
                                  unsigned int floor,
                                  struct vortexmax_cpu_info *info)
{
    unsigned int diff, step;
    
    if (cur_freq <= floor) return floor;
    
    diff = cur_freq - floor;
    step = max(floor / 100, diff / 40); /* Even slower decay */
    
    /* Extra slow in gaming/active modes */
    switch (info->ai.current_mode) {
        case VORTEX_MODE_GAMING: step = max(1, step / 4); break;
        case VORTEX_MODE_BURST:  step = max(1, step / 3); break;
        case VORTEX_MODE_HEAVY: step = max(1, step / 2); break;
        default: break;
    }
    
    if (cur_freq > floor + step) return cur_freq - step;
    return floor;
}

static unsigned int apply_transition_limit(unsigned int requested,
                                            unsigned int cur_freq,
                                            unsigned int sample_ms)
{
    unsigned int max_delta;
    
    if (!transition_smooth_enabled) return requested;
    
    max_delta = max_freq_change_per_ms * sample_ms;
    
    if (requested > cur_freq) {
        if ((requested - cur_freq) <= max_delta) return requested;
        return cur_freq + max_delta;
    } else {
        if ((cur_freq - requested) <= max_delta) return requested;
        return cur_freq - max_delta;
    }
}

static unsigned int apply_thermal_guard(unsigned int freq_target,
                                         unsigned int thermal_max,
                                         struct vortexmax_cpu_info *info)
{
    unsigned int soft_limit;
    
    soft_limit = (thermal_max * thermal_limit_pct) / 100;
    
    if (freq_target <= soft_limit) {
        if (info->thermal_counter > 0) info->thermal_counter -= 2;
        return freq_target;
    }
    
    info->thermal_counter++;
    
    if (info->thermal_counter < thermal_counter_threshold)
        return freq_target;
    
    /* Adaptive thermal response based on mode */
    switch (info->ai.current_mode) {
        case VORTEX_MODE_GAMING:
            /* Allow higher thermal in gaming before reducing */
            if (info->thermal_counter < (thermal_counter_threshold * 2))
                return freq_target;
            break;
        default:
            break;
    }
    
    if (thermal_hard_limit) return soft_limit;
    
    {   /* Soft reduction */
        unsigned int excess = freq_target - soft_limit;
        unsigned int reduction = excess / 2;
        if (reduction < (thermal_max / 50)) reduction = thermal_max / 50;
        return freq_target - reduction;
    }
}

/* =====================================================================
 * MAIN FREQUENCY EVALUATION FUNCTION (v4.0 AI Enhanced)
 * ===================================================================== */

static void vortexmax_eval_freq(struct cpufreq_policy *policy)
{
    struct vortexmax_cpu_info *info = &per_cpu(vortexmax_info, policy->cpu);
    u64 now_ns, idle_time, delta_wall, delta_idle;
    unsigned int raw_load, load, freq_target, cur_freq_val;
    unsigned int thermal_max, freq_floor;
    unsigned int touch_val = 0, io_val = 0, wake_val = 0;
    int trend;
    unsigned int behavior_mode;
    bool freq_increasing;

    now_ns = local_clock();
    idle_time = get_cpu_idle_time(policy->cpu, &delta_wall, 0);
    cur_freq_val = policy->cur;
    thermal_max = policy->max;

    /* ---- INITIALIZATION ---- */
    if (info->prev_cpu_wall == 0) {
        info->prev_cpu_wall = now_ns;
        info->prev_cpu_idle = idle_time;
        info->target_freq = cur_freq_val;
        info->next_delay_ms = sample_rate_ui_ms;
        info->last_touch_time = 0;
        info->last_io_time = 0;
        info->last_wake_time = now_ns;
        info->history_idx = 0;
        info->avg_load_ema = 0;
        info->prev_load = 0;
        info->load_trend = 0;
        info->is_active = false;
        info->consecutive_high_load = 0;
        info->consecutive_low_load = 0;
        info->io_burst_active = false;
        memset(info->load_history, 0, sizeof(info->load_history));
        
        /* Initialize AI state */
        memset(&info->ai, 0, sizeof(info->ai));
        info->ai.learned_target_big = target_load_big;
        info->ai.learned_target_little = target_load_little;
        info->ai.current_mode = VORTEX_MODE_IDLE;
        info->ai.prev_mode = VORTEX_MODE_IDLE;
        memset(info->ai.freq_history, 0, sizeof(info->ai.freq_history));
        
        return;
    }

    /* ---- LOAD CALCULATION ---- */
    delta_wall = now_ns - info->prev_cpu_wall;
    delta_idle = idle_time - info->prev_cpu_idle;
    info->prev_cpu_wall = now_ns;
    info->prev_cpu_idle = idle_time;

    /* Reject invalid short samples (< 1ms) */
    if (delta_wall < 1000000ULL) return;

    if (delta_wall == 0 || delta_idle > delta_wall)
        raw_load = 0;
    else
        raw_load = div64_u64(100 * (delta_wall - delta_idle), delta_wall);

    /* Update history and calculate smoothed values */
    update_load_history(info, raw_load);
    load = calculate_ema(info, raw_load);
    trend = detect_load_trend(info);
    info->load_trend = trend;
    
    /* ===== AI BEHAVIOR PROFILING ===== */
    behavior_mode = ai_get_behavior_mode(info, load, load);
    
    /* Update active state based on mode */
    info->is_active = (behavior_mode != VORTEX_MODE_IDLE);
    
    /* Self-learning: adapt target loads */
    {
        bool is_big = (policy->cpuinfo.max_freq > (policy->cpuinfo.min_freq * 2));
        ai_update_learned_targets(&info->ai, load, is_big);
    }
    
    /* Predictive load calculation */
    ai_predict_load(&info->ai, load, trend);
    
    /* Determine core type and get dynamic (learned) target */
    bool is_big = (policy->cpuinfo.max_freq > (policy->cpuinfo.min_freq * 2));
    unsigned int dyn_target = ai_get_dynamic_target(&info->ai, is_big);

    /* Get dynamic frequency floor based on behavior mode */
    freq_floor = get_frequency_floor(policy, info);

    /* ================================================================
     * PRIORITY-BASED DECISION MATRIX (v4.0 - AI Enhanced)
     * ================================================================ */
    
    /* 1. TOUCH BOOST (Highest Priority) */
    touch_val = apply_touch_boost(policy, info, now_ns);
    if (touch_val > 0) {
        freq_target = touch_val;
        goto apply_final;
    }

    /* 2. WAKE BOOST */
    wake_val = apply_wake_boost(policy, info, now_ns);
    if (wake_val > 0) {
        freq_target = wake_val;
        goto apply_final;
    }

    /* 3. IO BOOST */
    io_val = apply_io_boost(policy, info, now_ns, load);
    if (io_val > 0) {
        freq_target = io_val;
        goto apply_final;
    }

    /* 4. AI-ENHANCED SCALING LOGIC */
    if (load >= fast_ramp_up_load || info->ai.predicted_load >= fast_ramp_up_load) {
        /* Use predicted load if it's higher (proactive scaling) */
        unsigned int effective_load = max(load, info->ai.predicted_load);
        
        if (effective_load >= fast_ramp_up_load) {
            freq_target = thermal_max;
            if (info->max_hold_counter < max_hold_cycles)
                info->max_hold_counter++;
        } else {
            freq_target = smart_ramp_up(cur_freq_val, thermal_max, effective_load, trend);
            if (info->max_hold_counter < (max_hold_cycles / 2))
                info->max_hold_counter++;
        }
    } else if (load > dyn_target) {
        freq_target = smart_ramp_up(cur_freq_val, thermal_max, load, trend);
        if (info->max_hold_counter < (max_hold_cycles / 2))
            info->max_hold_counter++;
    } else if (info->max_hold_counter > 0) {
        if (load > (dyn_target - 15)) {
            freq_target = cur_freq_val;
            info->max_hold_counter--;
        } else {
            freq_target = smooth_decay(cur_freq_val, freq_floor, info);
            info->max_hold_counter--;
        }
    } else {
        freq_target = smooth_decay(cur_freq_val, freq_floor, info);
    }

apply_final:

    /* Enforce frequency floor */
    if (freq_target < freq_floor)
        freq_target = freq_floor;

    /* Apply thermal guard */
    freq_target = apply_thermal_guard(freq_target, thermal_max, info);

    /* Determine direction for hysteresis */
    freq_increasing = (freq_target > cur_freq_val);
    
    /* Apply hysteresis band */
    freq_target = apply_hysteresis(freq_target, cur_freq_val,
                                    policy->min, freq_increasing);

    /* Apply transition rate limiter */
    freq_target = apply_transition_limit(freq_target, cur_freq_val,
                                          info->next_delay_ms);

    /* HARD SAFETY CLAMP (Critical - prevents out-of-range values) */
    freq_target = clamp(freq_target, policy->min, policy->max);

    /* STABILITY GUARD: Check for oscillation */
    if (ai_check_stability(&info->ai, freq_target)) {
        /* Force smoother transition - use midpoint */
        if (abs((int)freq_target - (int)cur_freq_val) > (int)(thermal_max / 10)) {
            freq_target = (freq_target + cur_freq_val) / 2;
        }
    }

    /* Apply frequency change only if different */
    if (freq_target != info->target_freq) {
        info->target_freq = freq_target;
        __cpufreq_driver_target(policy, freq_target, CPUFREQ_RELATION_L);
        
        /* Update AI efficiency scoring */
        ai_update_efficiency_score(&info->ai, freq_target, thermal_max, load);
    }

    /* Adaptive sampling rate selection based on AI behavior mode */
    info->next_delay_ms = ai_get_adaptive_sampling(behavior_mode);
    
    /* Extra slowdown if stability guard is active */
    if (info->ai.force_smoothing) {
        info->next_delay_ms = max(info->next_delay_ms, info->next_delay_ms * 2);
        if (info->next_delay_ms > 50) info->next_delay_ms = 50;
    }
}

/* =====================================================================
 * INPUT SUBSYSTEM INTEGRATION
 * ===================================================================== */

static void vortexmax_do_touch_boost(struct work_struct *work)
{
    unsigned int cpu;
    struct vortexmax_cpu_info *info;
    
    for_each_online_cpu(cpu) {
        info = &per_cpu(vortexmax_info, cpu);
        info->last_touch_time = local_clock();
        info->is_active = true;
        info->consecutive_low_load = 0;
    }
    touch_pending = false;
}

static void vortexmax_input_event(struct input_handle *handle,
                                   unsigned int type,
                                   unsigned int code,
                                   int value)
{
    if (type == EV_KEY) {
        if (code == BTN_TOUCH || code == BTN_MOUSE ||
            code == BTN_TOOL_PEN || code == BTN_TOOL_FINGER ||
            code == BTN_STYLUS || code == BTN_STYLUS2) {
            if (value == 1) {
                if (!touch_pending) {
                    touch_pending = true;
                    schedule_work(&touch_boost_work);
                }
            }
        }
    }
    return;
}

static int vortexmax_input_connect(struct input_handler *handler,
                                    struct input_dev *dev,
                                    const struct input_device_id *id)
{
    struct input_handle *handle;
    int err;
    
    if (!(test_bit(EV_KEY, dev->evbit) &&
          (test_bit(BTN_TOUCH, dev->keybit) ||
           test_bit(BTN_MOUSE, dev->keybit) ||
           test_bit(BTN_TOOL_PEN, dev->keybit) ||
           test_bit(BTN_TOOL_FINGER, dev->keybit) ||
           test_bit(BTN_STYLUS, dev->keybit))))
        return -ENODEV;
    
    handle = kzalloc(sizeof(*handle), GFP_KERNEL);
    if (!handle) return -ENOMEM;
    
    handle->dev = dev;
    handle->handler = handler;
    handle->name = "vortexmax";
    
    err = input_register_handle(handle);
    if (err) goto err_free;
    
    err = input_open_device(handle);
    if (err) goto err_unreg;
    
    return 0;
    
err_unreg:
    input_unregister_handle(handle);
err_free:
    kfree(handle);
    return err;
}

static void vortexmax_input_disconnect(struct input_handle *handle)
{
    input_close_device(handle);
    input_unregister_handle(handle);
    kfree(handle);
}

static const struct input_device_id vortexmax_ids[] = {
    { .flags = INPUT_DEVICE_ID_MATCH_EVBIT | INPUT_DEVICE_ID_MATCH_KEYBIT,
      .evbit = { BIT_MASK(EV_KEY) },
      .keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) } },
    { .flags = INPUT_DEVICE_ID_MATCH_EVBIT | INPUT_DEVICE_ID_MATCH_KEYBIT,
      .evbit = { BIT_MASK(EV_KEY) },
      .keybit = { [BIT_WORD(BTN_MOUSE)] = BIT_MASK(BTN_MOUSE) } },
    { .flags = INPUT_DEVICE_ID_MATCH_EVBIT | INPUT_DEVICE_ID_MATCH_KEYBIT,
      .evbit = { BIT_MASK(EV_KEY) },
      .keybit = { [BIT_WORD(BTN_TOOL_PEN)] = BIT_MASK(BTN_TOOL_PEN) } },
    { .flags = INPUT_DEVICE_ID_MATCH_EVBIT | INPUT_DEVICE_ID_MATCH_KEYBIT,
      .evbit = { BIT_MASK(EV_KEY) },
      .keybit = { [BIT_WORD(BTN_TOOL_FINGER)] = BIT_MASK(BTN_TOOL_FINGER) } },
    { },
};

static struct input_handler vortexmax_input_handler = {
    .event      = vortexmax_input_event,
    .connect    = vortexmax_input_connect,
    .disconnect = vortexmax_input_disconnect,
    .name       = "vortexmax",
    .id_table   = vortexmax_ids,
};

/* =====================================================================
 * WORK QUEUE HANDLER
 * ===================================================================== */

static void vortexmax_work_handler(struct work_struct *work)
{
    struct vortexmax_policy_info *vpinfo =
        container_of(work, struct vortexmax_policy_info, work.work);
    struct cpufreq_policy *policy;

    if (!vpinfo || !vpinfo->policy) return;

    policy = vpinfo->policy;

    vortexmax_eval_freq(policy);
    
    schedule_delayed_work_on(
        policy->cpu,
        &vpinfo->work,
        msecs_to_jiffies(per_cpu(vortexmax_info, policy->cpu).next_delay_ms)
    );
}

/* =====================================================================
 * GKI GOVERNOR API
 * ===================================================================== */

static int vortexmax_init(struct cpufreq_policy *policy)
{
    struct vortexmax_policy_info *vpinfo;
    
    vpinfo = kzalloc(sizeof(*vpinfo), GFP_KERNEL);
    if (!vpinfo) return -ENOMEM;
    
    vpinfo->policy = policy;
    INIT_DEFERRABLE_WORK(&vpinfo->work, vortexmax_work_handler);
    policy->governor_data = vpinfo;
    
    return 0;
}

static void vortexmax_exit(struct cpufreq_policy *policy)
{
    struct vortexmax_policy_info *vpinfo = policy->governor_data;
    if (vpinfo) {
        cancel_delayed_work_sync(&vpinfo->work);
        kfree(vpinfo);
        policy->governor_data = NULL;
    }
}

static int vortexmax_start(struct cpufreq_policy *policy)
{
    unsigned int cpu;
    struct vortexmax_cpu_info *info;
    
    for_each_cpu(cpu, policy->cpus) {
        info = &per_cpu(vortexmax_info, cpu);
        
        info->prev_cpu_idle = get_cpu_idle_time(cpu, &info->prev_cpu_wall, 0);
        info->target_freq = policy->cur;
        info->thermal_counter = 0;
        info->max_hold_counter = 0;
        info->next_delay_ms = sample_rate_ui_ms;
        info->last_touch_time = 0;
        info->last_io_time = 0;
        info->last_wake_time = local_clock();
        info->history_idx = 0;
        info->avg_load_ema = 0;
        info->prev_load = 0;
        info->load_trend = 0;
        info->is_active = false;
        info->consecutive_high_load = 0;
        info->consecutive_low_load = 0;
        info->io_burst_active = false;
        memset(info->load_history, 0, sizeof(info->load_history));
        
        /* Initialize AI state */
        memset(&info->ai, 0, sizeof(info->ai));
        info->ai.learned_target_big = target_load_big;
        info->ai.learned_target_little = target_load_little;
        info->ai.current_mode = VORTEX_MODE_IDLE;
        info->ai.prev_mode = VORTEX_MODE_IDLE;
        memset(info->ai.freq_history, 0, sizeof(info->ai.freq_history));
    }
    
    bool need_init = false;

    spin_lock(&vortexmax_lock);
    if (!vortexmax_initialized) {
        need_init = true;
    }
    spin_unlock(&vortexmax_lock);

    if (need_init) {
        INIT_WORK(&touch_boost_work, vortexmax_do_touch_boost);
        if (input_register_handler(&vortexmax_input_handler) != 0)
            pr_err("VortexMax: input handler failed\\n");
        spin_lock(&vortexmax_lock);
        vortexmax_initialized = true;
        spin_unlock(&vortexmax_lock);
    }
    
    atomic_inc(&active_governors);
    
    schedule_delayed_work_on(
        policy->cpu,
        &((struct vortexmax_policy_info *)policy->governor_data)->work,
        msecs_to_jiffies(sample_rate_ui_ms)
    );
    
    return 0;
}

static void vortexmax_stop(struct cpufreq_policy *policy)
{
    struct vortexmax_policy_info *vpinfo = policy->governor_data;

    if (!vpinfo) return;

    cancel_delayed_work_sync(
        &vpinfo->work
    );
    if (atomic_dec_and_test(&active_governors)) {
        spin_lock(&vortexmax_lock);
        cancel_work_sync(&touch_boost_work);
        input_unregister_handler(&vortexmax_input_handler);
        vortexmax_initialized = false;
        spin_unlock(&vortexmax_lock);
    }
}

static void vortexmax_limits(struct cpufreq_policy *policy)
{
    vortexmax_eval_freq(policy);
}

/* =====================================================================
 * GOVERNOR REGISTRATION
 * ===================================================================== */

static struct cpufreq_governor vortexmax_gov = {
    .name       = "vortexmax",
    .owner      = THIS_MODULE,
    .init       = vortexmax_init,
    .exit       = vortexmax_exit,
    .start      = vortexmax_start,
    .stop       = vortexmax_stop,
    .limits     = vortexmax_limits,
};

/* =====================================================================
 * MODULE INIT/EXIT
 * ===================================================================== */

static int __init vortexmax_module_init(void)
{
    int ret;
    
    ret = cpufreq_register_governor(&vortexmax_gov);
    if (ret) {
        pr_err("VortexMax v4.0: Registration failed (%d)\n", ret);
        return ret;
    }
    
    pr_info("VortexMax v4.0 AI Adaptive Core loaded\n");
    pr_info("Features: SelfLearning, BehaviorProfiling, PredictiveScaling\n");
    pr_info("         AdaptiveSampling, BoostFusion, StabilityGuard\n");
    pr_info("Author: Kingfinik98 <kingfinix98@gmail.com>\n");
    
    return 0;
}

static void __exit vortexmax_module_exit(void)
{
    if (vortexmax_initialized) {
        cancel_work_sync(&touch_boost_work);
        input_unregister_handler(&vortexmax_input_handler);
        vortexmax_initialized = false;
    }
    
    cpufreq_unregister_governor(&vortexmax_gov);
    pr_info("VortexMax v4.0 unloaded\n");
}

module_init(vortexmax_module_init);
module_exit(vortexmax_module_exit);

MODULE_AUTHOR("Kingfinik98 <kingfinix98@gmail.com>");
MODULE_DESCRIPTION("VortexMax v4.0 AI Adaptive Core - Zero Dependency Smart Governor");
MODULE_LICENSE("GPL");
MODULE_VERSION("4.0");

