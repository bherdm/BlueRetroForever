#include "mouse_mapper.h"
#include <math.h>
#include <esp_timer.h>

#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

void mouse_mapper_default_cfg(struct mouse_mapper_cfg *cfg) {
    cfg->sensitivity = 1.2f;
    cfg->accel = 1.05f;
    cfg->smoothing_alpha = 0.2f;
    cfg->max_per_frame = 127;
    cfg->pointer_mode = true;
    cfg->carry_enabled = true;
    cfg->poll_interval_s = 1.0f / 60.0f;
}

void mouse_mapper_init(struct mouse_mapper_state *st, const struct mouse_mapper_cfg *cfg) {
    (void)cfg;
    st->carry_x = 0.0f;
    st->carry_y = 0.0f;
    st->ema_vx = 0.0f;
    st->ema_vy = 0.0f;
    st->last_ts_us = 0;
}

void mouse_mapper_reset(struct mouse_mapper_state *st) {
    st->carry_x = 0.0f;
    st->carry_y = 0.0f;
    st->ema_vx = 0.0f;
    st->ema_vy = 0.0f;
    st->last_ts_us = 0;
}

static inline float apply_accel(float v, float accel) {
    float s = (v >= 0.0f) ? 1.0f : -1.0f;
    float mag = fabsf(v);
    return s * powf(mag, accel);
}

void mouse_mapper_process(const struct mouse_mapper_cfg *cfg, struct mouse_mapper_state *st,
                          int32_t dx, int32_t dy, uint64_t ts_us,
                          int32_t *out_x, int32_t *out_y) {
    if (!cfg->pointer_mode) {
        /* Legacy path: just scale and clamp. */
        float sx = dx * cfg->sensitivity;
        float sy = dy * cfg->sensitivity;
        int32_t ox = (int32_t)lroundf(sx);
        int32_t oy = (int32_t)lroundf(sy);
        if (ox == 0 && dx != 0) ox = (dx > 0) ? 1 : -1;
        if (oy == 0 && dy != 0) oy = (dy > 0) ? 1 : -1;
        *out_x = CLAMP(ox, -cfg->max_per_frame, cfg->max_per_frame);
        *out_y = CLAMP(oy, -cfg->max_per_frame, cfg->max_per_frame);
        return;
    }

    uint64_t now_us = ts_us ? ts_us : (uint64_t)esp_timer_get_time();
    float dt = (st->last_ts_us == 0) ? 0.001f : ((float)(now_us - st->last_ts_us) / 1000000.0f);
    if (dt < 0.001f) dt = 0.001f; /* guard */
    st->last_ts_us = now_us;

    float vx = (float)dx / dt;
    float vy = (float)dy / dt;

    /* Isotropic gain/accel so diagonals aren't favored. */
    float speed = hypotf(vx, vy);
    float ux = (speed > 0.0f) ? (vx / speed) : 0.0f;
    float uy = (speed > 0.0f) ? (vy / speed) : 0.0f;

    speed *= cfg->sensitivity;

    if (cfg->accel != 1.0f && speed > 0.0f) {
        speed = powf(speed, cfg->accel);
    }

    vx = ux * speed;
    vy = uy * speed;

    float alpha = CLAMP(cfg->smoothing_alpha, 0.01f, 1.0f);
    st->ema_vx = alpha * vx + (1.0f - alpha) * st->ema_vx;
    st->ema_vy = alpha * vy + (1.0f - alpha) * st->ema_vy;

    float frame_dt = cfg->poll_interval_s > 0.0f ? cfg->poll_interval_s : 1.0f / 60.0f;
    float dx_frame = st->ema_vx * frame_dt;
    float dy_frame = st->ema_vy * frame_dt;

    float acc_x = cfg->carry_enabled ? st->carry_x + dx_frame : dx_frame;
    float acc_y = cfg->carry_enabled ? st->carry_y + dy_frame : dy_frame;

    int32_t ox = (cfg->carry_enabled) ? (int32_t)truncf(acc_x) : (int32_t)lroundf(acc_x);
    int32_t oy = (cfg->carry_enabled) ? (int32_t)truncf(acc_y) : (int32_t)lroundf(acc_y);

    if (ox == 0 && dx != 0) ox = (dx > 0) ? 1 : -1;
    if (oy == 0 && dy != 0) oy = (dy > 0) ? 1 : -1;

    if (cfg->carry_enabled) {
        st->carry_x = acc_x - (float)ox;
        st->carry_y = acc_y - (float)oy;
    }

    /* Limit diagonal magnitude so diagonals are not faster than straight axes. */
    float mag = hypotf((float)ox, (float)oy);
    if (mag > (float)cfg->max_per_frame && mag > 0.0f) {
        float scale = ((float)cfg->max_per_frame) / mag;
        int32_t sx = (int32_t)lroundf(ox * scale);
        int32_t sy = (int32_t)lroundf(oy * scale);
        if (sx == 0 && ox != 0) {
            sx = (ox > 0) ? 1 : -1;
        }
        if (sy == 0 && oy != 0) {
            sy = (oy > 0) ? 1 : -1;
        }
        ox = sx;
        oy = sy;
    }

    ox = CLAMP(ox, -cfg->max_per_frame, cfg->max_per_frame);
    oy = CLAMP(oy, -cfg->max_per_frame, cfg->max_per_frame);

    *out_x = ox;
    *out_y = oy;
}
