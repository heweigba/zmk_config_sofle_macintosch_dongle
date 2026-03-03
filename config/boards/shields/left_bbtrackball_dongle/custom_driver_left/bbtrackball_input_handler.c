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
// 已删除：改用 input subsystem

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

/* ==== 解发方向键（使用 input subsystem）==== */
static void trigger_arrow_key(uint8_t dir, bool pressed) {
    uint16_t key_code;

    switch (dir) {
        case DIR_LEFT:   key_code = INPUT_KEY_LEFT;  break;
        case DIR_RIGHT:  key_code = INPUT_KEY_RIGHT; break;
        case DIR_UP:     key_code = INPUT_KEY_UP;    break;
        case DIR_DOWN:   key_code = INPUT_KEY_DOWN;  break;
        default: return;
    }

    /* 使用 input subsystem 发送按键事件（同步等待）*/
    input_report_key(trackball_dev_ref, key_code, pressed ? 1 : 0, true, K_MSEC(50));
}

/* ==== 轮询处理（逐步重新启用）==== */
static void poll_handler(struct k_work *work) {
    uint32_t now = k_uptime_get_32();

    for (int i = 0; i < DIR_COUNT; i++) {
        DirState *d = &dir_states[i];
        int current_state = gpio_pin_get(d->gpio_dev, d->pin);

        /* 检测下降沿（按下） */
        if (d->last_state == 1 && current_state == 0) {
            /* 冷却时间检查 */
            if (now - d->last_trigger_time >= COOLDOWN_MS) {
                d->last_trigger_time = now;

                /* 触发按键按下和释放 */
                trigger_arrow_key(i, true);
                k_msleep(5);
                trigger_arrow_key(i, false);

                LOG_INF("Direction %d triggered", i);
            }
        }

        d->last_state = current_state;
    }

    k_work_schedule(&poll_work, K_MSEC(POLL_INTERVAL_MS));
}

/* ==== 轨迹球移动状态查询 ==== */
bool trackball_is_moving(void) {
    uint32_t now = k_uptime_get_32();

    for (int i = 0; i < DIR_COUNT; i++) {
        DirState *d = &dir_states[i];
        /* 如果任意方向在冷却期内，认为正在移动 */
        if (now - d->last_trigger_time < COOLDOWN_MS) {
            return true;
        }
    }
    return false;
}

/* ==== 初始化 ==== */
static int bbtrackball_init(const struct device *dev) {
    LOG_INF("Initializing BBtrackball (polling mode)...");
    LOG_INF("  POLL_INTERVAL: %dms, COOLDOWN: %dms",
            POLL_INTERVAL_MS, COOLDOWN_MS);

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

