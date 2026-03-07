/*
 * bb_wheel_to_arrow.c - Custom input processor
 * Converts wheel events to arrow keys
 * Bypasses buggy input-processor-behaviors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_bb_wheel_to_arrow

#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zmk/behavior.h>
#include <zmk/keymap.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(bb_wheel_to_arrow, LOG_LEVEL_INF);

struct bb_wheel_to_arrow_cfg {
    const struct device *dev;
};

static int bb_wheel_to_arrow_init(const struct device *dev) {
    LOG_INF("BB wheel-to-arrow processor initialized");
    return 0;
}

/* Process input events */
static int bb_wheel_to_arrow_handle(const struct device *dev,
                                        struct input_event *event) {
    const struct bb_wheel_to_arrow_cfg *cfg = dev->config;

    /* Only process RELATIVE (wheel) events */
    if (event->type != INPUT_EV_REL) {
        return 0;
    }

    uint16_t arrow_key = 0;

    /* Map wheel events to arrow keys */
    switch (event->code) {
        case INPUT_REL_WHEEL:
            if (event->value > 0) {
                arrow_key = INPUT_KEY_UP;
            } else if (event->value < 0) {
                arrow_key = INPUT_KEY_DOWN;
            }
            break;

        case INPUT_REL_HWHEEL:
            if (event->value > 0) {
                arrow_key = INPUT_KEY_RIGHT;
            } else if (event->value < 0) {
                arrow_key = INPUT_KEY_LEFT;
            }
            break;

        default:
            return 0;
    }

    if (arrow_key == 0) {
        return 0;
    }

    LOG_INF("Converting wheel %d (value %d) to arrow key %d",
         event->code, event->value, arrow_key);

    /* Find the zmk,behaviors-key-press behavior */
    static const struct device *kp_dev =
        DEVICE_DT_GET(DT_NODELABEL(kp));

    if (!device_is_ready(kp_dev)) {
        LOG_ERR("kp behavior device not ready");
        return -ENODEV;
    }

    /* Invoke the key press behavior directly */
    struct zmk_behavior_binding binding = {
        .behavior_dev = kp_dev,
        .param1 = arrow_key,
        .param2 = 0,
    };

    /* Trigger key press */
    int ret = zmk_behavior_invoke_binding(&binding, NULL, true);
    if (ret < 0) {
        LOG_ERR("Failed to invoke behavior: %d", ret);
        return ret;
    }

    /* Small delay then release */
    k_msleep(30);

    /* Trigger key release */
    ret = zmk_behavior_invoke_binding(&binding, NULL, false);
    if (ret < 0) {
        LOG_ERR("Failed to release behavior: %d", ret);
        return ret;
    }

    LOG_INF("Arrow key %d triggered successfully", arrow_key);
    return 0;
}

/* Device tree binding */
#define BB_WHEEL_TO_ARROW_DEFINE(inst)                                               \
    static struct bb_wheel_to_arrow_cfg bb_wheel_to_arrow_cfg_##inst;         \
    DEVICE_DT_INST_DEFINE(inst, bb_wheel_to_arrow_init, NULL,              \
                          &bb_wheel_to_arrow_cfg_##inst, NULL,              \
                          POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(BB_WHEEL_TO_ARROW_DEFINE);
