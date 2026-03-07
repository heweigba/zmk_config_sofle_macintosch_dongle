/*
 * bb_btn_to_arrow.c - 自定义input processor
 * 把BTN_0/1/2/3事件转换成方向键
 * 绕过有bug的input-processor-behaviors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_bb_btn_to_arrow

#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bb_btn_to_arrow, LOG_LEVEL_INF);

struct bb_btn_to_arrow_cfg {
    const struct device *dev;
};

static int bb_btn_to_arrow_init(const struct device *dev) {
    LOG_INF("BB btn-to-arrow processor initialized");
    return 0;
}

/* 处理input事件 */
static int bb_btn_to_arrow_handle(const struct device *dev,
                                   struct input_event *event) {
    /* 只处理KEY类型事件 */
    if (event->type != INPUT_EV_KEY) {
        return 0;
    }

    uint32_t arrow_key = 0;

    /* 映射BTN事件到方向键 */
    switch (event->code) {
        case INPUT_BTN_0:
            arrow_key = INPUT_KEY_LEFT;
            break;
        case INPUT_BTN_1:
            arrow_key = INPUT_KEY_RIGHT;
            break;
        case INPUT_BTN_2:
            arrow_key = INPUT_KEY_UP;
            break;
        case INPUT_BTN_3:
            arrow_key = INPUT_KEY_DOWN;
            break;
        default:
            return 0;
    }

    LOG_INF("Converting BTN %u to arrow key %u (value %d)",
            event->code, arrow_key, event->value);

    /* 发送方向键事件 */
    int ret = input_report_key(dev, arrow_key, event->value, true, K_NO_WAIT);
    if (ret < 0) {
        LOG_ERR("Failed to send arrow key: %d", ret);
        return ret;
    }

    LOG_INF("Arrow key %u triggered successfully", arrow_key);
    return 0;
}

/* Device tree binding */
#define BB_BTN_TO_ARROW_DEFINE(inst)                                               \
    static struct bb_btn_to_arrow_cfg bb_btn_to_arrow_cfg_##inst;                  \
    DEVICE_DT_INST_DEFINE(inst, bb_btn_to_arrow_init, NULL,                        \
                          &bb_btn_to_arrow_cfg_##inst, NULL,                       \
                          POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(BB_BTN_TO_ARROW_DEFINE);