/*
 * Simple host-side tests for mouse mapper math. Not auto-run; call mouse_mapper_test_run() from a harness.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include "adapter/mouse_mapper.h"

static bool test_pointer_linear(void) {
    struct mouse_mapper_cfg cfg;
    struct mouse_mapper_state st;
    mouse_mapper_default_cfg(&cfg);
    cfg.pointer_mode = true;
    cfg.sensitivity = 1.0f;
    cfg.accel = 1.0f;
    cfg.smoothing_alpha = 1.0f;
    cfg.carry_enabled = false;
    cfg.poll_interval_s = 0.001f;
    mouse_mapper_init(&st, &cfg);

    int32_t ox = 0;
    int32_t oy = 0;
    mouse_mapper_process(&cfg, &st, 1, 2, 1000, &ox, &oy);
    assert(ox == 1);
    assert(oy == 2);
    return true;
}

static bool test_pointer_carry(void) {
    struct mouse_mapper_cfg cfg;
    struct mouse_mapper_state st;
    mouse_mapper_default_cfg(&cfg);
    cfg.pointer_mode = true;
    cfg.sensitivity = 0.1f;
    cfg.accel = 1.0f;
    cfg.smoothing_alpha = 1.0f;
    cfg.carry_enabled = true;
    cfg.poll_interval_s = 0.001f;
    mouse_mapper_init(&st, &cfg);

    int32_t ox = 0;
    int32_t oy = 0;
    int32_t sum = 0;
    for (int i = 0; i < 10; i++) {
        mouse_mapper_process(&cfg, &st, 1, 0, (uint64_t)(1000 * (i + 1)), &ox, &oy);
        sum += ox;
    }
    assert(sum == 1);
    assert(oy == 0);
    return true;
}

static bool test_legacy_scaling(void) {
    struct mouse_mapper_cfg cfg;
    struct mouse_mapper_state st;
    mouse_mapper_default_cfg(&cfg);
    cfg.pointer_mode = false;
    cfg.sensitivity = 2.0f;
    cfg.max_per_frame = 127;
    mouse_mapper_init(&st, &cfg);

    int32_t ox = 0;
    int32_t oy = 0;
    mouse_mapper_process(&cfg, &st, 3, -2, 1000, &ox, &oy);
    assert(ox == 6);
    assert(oy == -4);
    return true;
}

bool mouse_mapper_test_run(void) {
    return test_pointer_linear() && test_pointer_carry() && test_legacy_scaling();
}

void mouse_mapper_test_log(void) {
    if (mouse_mapper_test_run()) {
        printf("# mouse_mapper_test: ok\n");
    }
}
