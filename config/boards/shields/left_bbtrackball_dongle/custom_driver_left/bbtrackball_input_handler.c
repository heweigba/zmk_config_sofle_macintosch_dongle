/*
 * bbtrackball_input_handler.c - BB Trackball (轮询模式， *
 * 功能:禁用中断，每 100ms 轮询一次，避免事件洪泛
 *
 * 算法：
* - 每 100ms 读取一次所有方向的 GPIO 状态
* - 如果检测到移动，发送 1 个方向键事件
* - 冷却时间: 1000ms
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_bbtrackball

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zmk/behavior.h>

LOG_MODULE_REGISTER(bbtrackball_input_handler, LOG_LEVEL_INF);

/* ==== GPIO Pins ==== */
#define DOWN_GPIO_PIN 9
#define LEFT_GPIO_PIN 12
#define UP_GPIO_PIN 5
#define RIGHT_GPIO_PIN 27

#define GPIO0_DEV DT_NODELABEL(gpio0)
#define GPIO1_DEV DT_NODELABEL(gpio1)

/* ==== 节流控制参数（轮询模式）==== */
#define POLL_INTERVAL_MS 100       /* 轮询间隔：100ms */
#define COOLDOWN_MS 1000            /* 冷却时间:1000ms */

/* ==== 方向定义 ==== */
enum {
    DIR_LEFT = 0,
    DIR_RIGHT,
    DIR_UP,
    DIR_DOWN,
    DIR_COUNT
};

/* ==== Behavior 设备引用 ==== */
static const struct device *kp_behavior_dev = NULL;

/* ==== 每个方向的状态 ==== */
typedef struct {
    const struct device *gpio_dev;
    int pin;
    int last_state;
    uint32_t last_trigger_time;  /* 上次触发时间 */
} DirState;

/* 方向修正（引脚映射反转）*/
static DirState dir_states[DIR_COUNT] = {
    [DIR_LEFT]  = {DEVICE_DT_GET(GPIO0_DEV), RIGHT_GPIO_PIN, 1, 0},
    [DIR_RIGHT] = {DEVICE_DT_GET(GPIO0_DEV), LEFT_GPIO_PIN, 1, 0},
    [DIR_UP]    = {DEVICE_DT_GET(GPIO1_DEV), DOWN_GPIO_PIN, 1, 0},
    [DIR_DOWN]  = {DEVICE_DT_GET(GPIO0_DEV), UP_GPIO_PIN, 1, 0},
};

static struct k_work_delayable poll_work;
static const struct device *trackball_dev_ref = NULL;

/* ==== 解发方向键 behavior ==== */
static void trigger_arrow_behavior(uint8_t dir) {
    struct zmk_behavior_binding binding = {
        .behavior_dev = kp_behavior_dev,
        .param1 = 0,
        .param2 = 0,
    };

    switch (dir) {
        case DIR_LEFT:   binding.param1 = INPUT_KEY_LEFT;  break;
        case DIR_RIGHT:  binding.param1 = INPUT_KEY_RIGHT; break;
        case DIR_UP:     binding.param1 = INPUT_KEY_UP;    break;
        case DIR_DOWN:   binding.param1 = INPUT_KEY_DOWN;  break;
        default: return;
    }

    /* 触发按键按下 */
    zmk_behavior_trigger_binding(&binding, true);
    /* 立即触发按键释放 */
    zmk_behavior_trigger_binding(&binding, false);
}

/* ==== 轮询处理（完全禁用，用于测试）==== */
static void poll_handler(struct k_work *work) {
    /* 定时测试：暂时不触发任何事件 */
    k_work_schedule(&poll_work, K_MSEC(POLL_INTERVAL_MS));
}

/* ==== 轨迹球移动状态查询（测试版始终返回 false）==== */
bool trackball_is_moving(void) {
    /* 测试版：完全禁用轨迹球，始终返回未移动 */
    return false;
}

/* ==== 初始化 ==== */
static int bbtrackball_init(const struct device *dev) {
    LOG_INF("Initializing BBtrackball (polling mode)...");
    LOG_INF("  POLL_INTERVAL: %dms, COOLDOWN: %dms",
            POLL_INTERVAL_MS, COOLDOWN_MS);

    /* 获取 KP behavior 设备引用 */
    kp_behavior_dev = DEVICE_DT_GET(DT_NODELABEL(kp));

    if (kp_behavior_dev == NULL) {
        LOG_ERR("Failed to get KP behavior device");
        return -EINVAL;
    }

    /* 初始化 GPIO（不使用中断） */
    for (int i = 0; i < DIR_COUNT; i++) {
        DirState *d = &dir_states[i];
        gpio_pin_configure(d->gpio_dev, d->pin, GPIO_INPUT | GPIO_PULL_UP);
        d->last_state = gpio_pin_get(d->gpio_dev, d->pin);
        d->last_trigger_time = 0;
    }

    trackball_dev_ref = dev;

    k_work_init_delayable(&poll_work, poll_handler);
    k_work_schedule(&poll_work, K_MSEC(POLL_INTERVAL_MS));

    return 0;
}

