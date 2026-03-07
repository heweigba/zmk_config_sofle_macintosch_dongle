/* ==== 触发方向键（使用behavior直接触发，绕过input系统） ==== */
static void trigger_arrow_behavior(uint8_t dir) {
    if (dir >= DIR_COUNT) return;
    if (!arrow_dev || !device_is_ready(arrow_dev)) {
        LOG_ERR("Arrow behavior device not ready");
        return;
    }
    if (!arrow_keycodes) {
        LOG_ERR("Invalid direction: %d", dir);
        return;
    }
    struct zmk_behavior_binding binding = {
        .behavior_dev = arrow_dev,
        .param1 = arrow_keycodes[dir],
        .param2 = 0,
    };
    struct zmk_behavior_binding_event event = {
        .position = 0,
        .layer = 0,
        .param1 = 0,
        .param2 = 0,
    };
    LOG_INF("Triggering behavior for direction %d", dir);
    zmk_behavior_trigger_binding(&binding, &event, true);
    zmk_behavior_trigger_binding(&binding, &event, false);
}
