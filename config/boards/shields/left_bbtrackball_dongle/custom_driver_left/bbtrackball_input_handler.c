/*
 * bbtrackball_input_handler.c - BB Trackball (节流状态机)
 *
 * 算法：节流状态机
 * - IDLE: 等待首次脉冲
 * - FIRST_OUTPUT: 输出 1 格，进入冷却
 * - COOLDOWN: 300ms 冷却期，累计脉冲
 * - UNIFORM: 200ms 固定间隔匀速输出
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_bbtrackball

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

LOG_MODULE_REGISTER(bbtrackball_input_handler, LOG_LEVEL_INF);

/* ==== GPIO Pins ==== */
#define DOWN_GPIO_PIN 9
#define LEFT_GPIO_PIN 12
#define UP_GPIO_PIN 5
#define RIGHT_GPIO_PIN 27

#define GPIO0_DEV DT_NODELABEL(gpio0)
#define GPIO1_DEV DT_NODELABEL(gpio1)


/* ==== 滚轮节流控制参数（增加冷却时间）==== */
#define COOLDOWN_MS 300            /* 首次触发后的冷却期（从 200ms 增加到 300ms）*/
#define UNIFORM_INTERVAL_MS 200        /* 匢速输出间隔（从 150ms 增加到 200ms）*/
#define RESET_IDLE_MS 500          /* 停止移动后重置状态的时间 */
#define DEBOUNCE_MS 10              /* 防抖时间：10ms 内的重复脉冲忽略 */

/* 节流状态机 */
enum throttle_state {
    THROTTLE_IDLE = 0,
    THROTTLE_FIRST_OUTPUT,
    THROTTLE_COOLDOWN,
    THROTTLE_UNIFORM
};

/* ==== 方向定义 ==== */
enum {
    DIR_LEFT = 0,
    DIR_RIGHT,
    DIR_UP,
    DIR_DOWN,
    DIR_COUNT
};

/* ==== 状态 ==== */
static const struct device *trackball_dev_ref = NULL;

/* ==== 每个方向的状态 ==== */
typedef struct {
    const struct device *gpio_dev;
    int pin;
    int last_state;
    int pending_steps;
    int accumulated_steps;
    enum throttle_state throttle;
    uint32_t last_output_time;
    uint32_t last_pulse_time;
} DirState;

/* 方向修正 */
static DirState dir_states[DIR_COUNT] = {
    [DIR_LEFT]  = {DEVICE_DT_GET(GPIO0_DEV), RIGHT_GPIO_PIN, 1, 0, 0, THROTTLE_IDLE, 0, 0},
    [DIR_RIGHT] = {DEVICE_DT_GET(GPIO0_DEV), LEFT_GPIO_PIN, 1, 0, 0, THROTTLE_IDLE, 0, 0},
    [DIR_UP]    = {DEVICE_DT_GET(GPIO1_DEV), DOWN_GPIO_PIN, 1, 0, 0, THROTTLE_IDLE, 0, 0},
    [DIR_DOWN]  = {DEVICE_DT_GET(GPIO0_DEV), UP_GPIO_PIN, 1, 0, 0, THROTTLE_IDLE, 0, 0},
};

static struct gpio_callback gpio_cbs[DIR_COUNT];
static struct k_work_delayable process_work;

/* ==== Device Config/Data ==== */
struct bbtrackball_dev_config {
    uint16_t x_input_code;
    uint16_t y_input_code;
};

struct bbtrackball_data {
    const struct device *dev;
};

/* ==== 外部接口 ==== */
bool trackball_is_moving(void) {
    uint32_t now = k_uptime_get_32();
    for (int i = 0; i < DIR_COUNT; i++) {
        DirState *d = &dir_states[i];
        if (d->pending_steps > 0 || d->accumulated_steps > 0) {
            return true;
        }
        if (now - d->last_pulse_time < 100) {
            return true;
        }
    }
    return false;
}

/* ==== 发送方向键（使用 INPUT_KEY 事件）==== */
static void send_arrow_key(uint8_t dir, bool pressed) {
    uint16_t key_code;
    switch (dir) {
        case DIR_LEFT:  key_code = INPUT_KEY_LEFT; break;
        case DIR_RIGHT: key_code = INPUT_KEY_RIGHT; break;
        case DIR_UP:    key_code = INPUT_KEY_UP; break;
        case DIR_DOWN:  key_code = INPUT_KEY_DOWN; break;
        default: return;
    }

    /* 使用阻塞等待（10ms 超时）*/
    input_report_key(trackball_dev_ref, key_code, pressed ? 1 : 0, true, K_MSEC(10));
}

/* ==== 触发一次方向键（按下 + 释放）==== */
static void trigger_arrow_key(uint8_t dir) {
    /* 按下 */
    send_arrow_key(dir, true);
    /* 释放 */
    send_arrow_key(dir, false);
}

/* ==== GPIO 中断回调 ==== */
static void dir_edge_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    uint32_t now = k_uptime_get_32();

    for (int i = 0; i < DIR_COUNT; i++) {
        DirState *d = &dir_states[i];
        if ((dev == d->gpio_dev) && (pins & BIT(d->pin))) {
            int val = gpio_pin_get(dev, d->pin);
            if (val != d->last_state) {
                d->last_state = val;
                if (val == 0) {  /* 下降沿 */
                    if (now - d->last_pulse_time < DEBOUNCE_MS) {
                        break;
                    }
                    d->last_pulse_time = now;

                    switch (d->throttle) {
                        case THROTTLE_IDLE:
                            d->pending_steps = 1;
                            d->accumulated_steps = 0;
                            d->throttle = THROTTLE_FIRST_OUTPUT;
                            LOG_INF("Dir %d: IDLE -> FIRST_OUTPUT", i);
                            break;

                        case THROTTLE_FIRST_OUTPUT:
                            d->accumulated_steps++;
                            LOG_DBG("Dir %d: FIRST_OUTPUT pulse accumulated", i);
                            break;

                        case THROTTLE_COOLDOWN:
                            d->accumulated_steps++;
                            LOG_DBG("Dir %d: COOLDOWN pulse accumulated, total=%d", i, d->accumulated_steps);
                            break;

                        case THROTTLE_UNIFORM:
                            d->accumulated_steps++;
                            LOG_DBG("Dir %d: UNIFORM pulse accumulated, total=%d", i, d->accumulated_steps);
                            break;
                    }
                }
            }
            break;
        }
    }
}

/* ==== 处理单个方向的节流状态机 ==== */
static void process_dir_throttle(DirState *d, int dir_id, uint32_t now) {
    switch (d->throttle) {
        case THROTTLE_IDLE:
            break;

        case THROTTLE_FIRST_OUTPUT:
            if (d->pending_steps > 0) {
                trigger_arrow_key(dir_id);
                d->pending_steps = 0;
                d->last_output_time = now;
                d->throttle = THROTTLE_COOLDOWN;
                LOG_INF("Dir %d: FIRST_OUTPUT -> COOLDOWN", dir_id);
            }
            break;

        case THROTTLE_COOLDOWN:
            if (now - d->last_output_time >= COOLDOWN_MS) {
                d->pending_steps = d->accumulated_steps;
                d->accumulated_steps = 0;
                d->throttle = THROTTLE_UNIFORM;
                d->last_output_time = now;
                LOG_INF("Dir %d: COOLDOWN -> UNIFORM (pending=%d)", dir_id, d->pending_steps);

                if (d->pending_steps > 0) {
                    trigger_arrow_key(dir_id);
                    d->pending_steps--;
                    d->last_output_time = now;
                }
            }
            break;

        case THROTTLE_UNIFORM:
            if (now - d->last_output_time >= UNIFORM_INTERVAL_MS) {
                if (d->pending_steps > 0) {
                    trigger_arrow_key(dir_id);
                    d->pending_steps--;
                    d->last_output_time = now;
                } else if (d->accumulated_steps > 0) {
                    d->pending_steps = d->accumulated_steps;
                    d->accumulated_steps = 0;
                    trigger_arrow_key(dir_id);
                    d->pending_steps--;
                    d->last_output_time = now;
                }
            }
            break;
    }

    if (d->throttle != THROTTLE_IDLE && (now - d->last_pulse_time > RESET_IDLE_MS)) {
        LOG_INF("Dir %d: timeout reset to IDLE", dir_id);
        d->throttle = THROTTLE_IDLE;
        d->pending_steps = 0;
        d->accumulated_steps = 0;
    }
}

/* ==== 处理步进 ==== */
static void process_handler(struct k_work *work) {
    uint32_t now = k_uptime_get_32();

    /* 处理 X 轴 */
    DirState *d_left = &dir_states[DIR_LEFT];
    DirState *d_right = &dir_states[DIR_RIGHT];


    if ((d_left->pending_steps > 0 || d_left->accumulated_steps > 0) &&
        (d_right->pending_steps > 0 || d_right->accumulated_steps > 0)) {
        int left_total = d_left->pending_steps + d_left->accumulated_steps;
        int right_total = d_right->pending_steps + d_right->accumulated_steps;
        if (left_total > right_total) {
            process_dir_throttle(d_left, DIR_LEFT, now);
        } else {
            process_dir_throttle(d_right, DIR_RIGHT, now);
        }
    } else {
        process_dir_throttle(d_left, DIR_LEFT, now);
        process_dir_throttle(d_right, DIR_RIGHT, now);
    }



    /* 处理 Y 轴 */
    DirState *d_up = &dir_states[DIR_UP];
    DirState *d_down = &dir_states[DIR_DOWN];


    if ((d_up->pending_steps > 0 || d_up->accumulated_steps > 0) &&
        (d_down->pending_steps > 0 || d_down->accumulated_steps > 0)) {
        int up_total = d_up->pending_steps + d_up->accumulated_steps;
        int down_total = d_down->pending_steps + d_down->accumulated_steps;
        if (up_total > down_total) {
            process_dir_throttle(d_up, DIR_UP, now);
        } else {
            process_dir_throttle(d_down, DIR_DOWN, now);
        }
    } else {
        process_dir_throttle(d_up, DIR_UP, now);
        process_dir_throttle(d_down, DIR_DOWN, now);
    }


    k_work_schedule(&process_work, K_MSEC(50));
}

/* ==== 初始化 ==== */
static int bbtrackball_init(const struct device *dev) {
    struct bbtrackball_data *data = dev->data;



    LOG_INF("Initializing BBtrackball (throttle state machine)...");
    LOG_INF("  COOLDOWN: %dms, UNIFORM: %dms, RESET: %dms",
            COOLDOWN_MS, UNIFORM_INTERVAL_MS, RESET_IDLE_MS);

    for (int i = 0; i < DIR_COUNT; i++) {
        DirState *d = &dir_states[i];
        gpio_pin_configure(d->gpio_dev, d->pin,
                          GPIO_INPUT | GPIO_PULL_UP | GPIO_INT_EDGE_BOTH);
        d->last_state = gpio_pin_get(d->gpio_dev, d->pin);


        gpio_init_callback(&gpio_cbs[i], dir_edge_cb, BIT(d->pin));
        gpio_add_callback(d->gpio_dev, &gpio_cbs[i]);
        gpio_pin_interrupt_configure(d->gpio_dev, d->pin, GPIO_INT_EDGE_BOTH);
    }


    data->dev = dev;
    trackball_dev_ref = dev;

    k_work_init_delayable(&process_work, process_handler);
    k_work_schedule(&process_work, K_MSEC(50));

    return 0;
}
