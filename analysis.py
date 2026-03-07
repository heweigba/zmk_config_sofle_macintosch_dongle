#!/usr/bin/env3

# -*- coding: utf-8 -*-

"""
死机根因分析:
关键发现:
"""

import sys

print("=== 根本原因 ===")
print()
print("问题: INPUT_KEY_0/1/2/3 与键盘扫描器冲突!")
print()
print("分析:")
print("1. KEY_0/1/2/3 是键盘数字键的扫描码")
print("2. 接收器同时有键盘扫描器在工作")
print("3. 当轨迹球发送KEY_0时:")
print("   - 键盘扫描器可能同时发送数字0")
print("   - input-processor-behaviors尝试转换为方向键")
print("   - 两个系统冲突 → 崩溃!")
print()
print("证据:")
print("- TrackPoint用REL_X/Y(鼠标移动) - 不冲突 ✓")
print("- BTN方案用BTN事件 - 不冲突 ✓")
print("- KEY方案用KEY_0-3 - 与键盘冲突 ✗")
print()
print("结论:")
print("必须使用**不与键盘扫描冲突的事件类型**:")
print("- 鼠标按钮 (BTN) - 之前卡住")
print("- 鼠标移动 (REL) - TrackPoint在用")
print("- 功能键 (KEY_F1-f4) - 未被键盘扫描器使用")
print()
print("解决方案: 改用 **INPUT_KEY_F1/F2/F3/F4** (功能键)")
