/*
 * bb_wheel_to_arrow.c - Custom input processor
 * Converts wheel events to arrow keys
 * Directly sends INPUT_KEY events without behaviors API
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_bb_wheel_to_arrow

#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
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
    /* Only process RELATIVE (wheel) events */
    if (event->type != INPUT_EV_REL) {
        return 0;
    }

    uint32_t arrow_key = 0;

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

    LOG_INF("Converting wheel %d (value %d) to arrow key %u",
         event->code, event->value, arrow_key);

    /* Send key press event (sync=false) */
    int ret = input_report_key(dev, arrow_key, 1, false, K_NO_WAIT);
    if (ret < 0) {
        LOG_ERR("Failed to send key press: %d", ret);
        return ret;
    }

    /* Send key release event (sync=true to complete the sequence) */
    ret = input_report_key(dev, arrow_key, 0, true, K_NO_WAIT);
    if (ret < 0) {
        LOG_ERR("Failed to send key release: %d", ret);
        return ret;
    }

    LOG_INF("Arrow key %u triggered successfully", arrow_key);
    return 0;
}

/* Device tree binding */
#define BB_WHEEL_TO_ARROW_DEFINE(inst)                                               \
    static struct bb_wheel_to_arrow_cfg bb_wheel_to_arrow_cfg_##inst;         \
    DEVICE_DT_INST_DEFINE(inst, bb_wheel_to_arrow_init, NULL,              \
                          &bb_wheel_to_arrow_cfg_##inst, NULL,              \
                          POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(BB_WHEEL_TO_ARROW_DEFINE);
