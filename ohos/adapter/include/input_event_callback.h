// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_INPUT_EVENT_CALLBACK_H_
#define OHOS_INPUT_EVENT_CALLBACK_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
    UNKNOWN_INPUT_EVENT = 0,
    OHOS_MOUSE_EVENT,
    OHOS_KEY_EVENT,
    OHOS_WINDOW_RECT_CHANGE_EVENT,
} input_event_type;

typedef enum {
    OHOS_MOUSE_ACTION_SCROLL = 99,
} ohos_mouse_action;

typedef struct {
    float x;
    float y;
    float screen_x;
    float screen_y;
    int64_t timestamp;
    int action;
    int button;
    float wheel_x;
    float wheel_y;
} mouse_event_param;

typedef struct {
    bool is_hover;
} mouse_hover_event_param;

typedef struct {
    int action;
    int code;
    int source_type;
    int64_t device_id;
    int64_t timestamp;
    uint64_t modifier_key_states;
    bool is_num_lockon;
    bool is_caps_lockon;
    bool is_scroll_lockon;
} key_event_param;

typedef struct {
    input_event_type type;
    uint64_t window;
    int32_t left;
    int32_t top;
    int32_t width;
    int32_t height;
} window_rect_change_event_param;

typedef union {
    mouse_event_param mouse_event;
    mouse_hover_event_param mouse_hover_event;
    key_event_param key_event;
    window_rect_change_event_param window_rect_change_event;
} event_data;

typedef struct {
    input_event_type type;
    int widget_id;
    event_data data;
} input_event;

typedef void (*input_event_callback)(const input_event* event);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  // OHOS_INPUT_EVENT_CALLBACK_H_
