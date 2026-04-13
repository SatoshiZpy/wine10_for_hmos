// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IME_EVENT_CALLBACK_H
#define IME_EVENT_CALLBACK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

enum ime_text_input_type
{
  IME_TEXT_INPUT_NONE = -1,
  IME_TEXT_INPUT_TEXT = 0,
  IME_TEXT_INPUT_MULTILINE,
  IME_TEXT_INPUT_NUMBER,
  IME_TEXT_INPUT_PHONE,
  IME_TEXT_INPUT_DATETIME,
  IME_TEXT_INPUT_EMAIL_ADDRESS,
  IME_TEXT_INPUT_URL,
  IME_TEXT_INPUT_VISIBLE_PASSWORD,
  IME_TEXT_INPUT_NUMBER_PASSWORD,
  IME_TEXT_INPUT_SCREEN_LOCK_PASSWORD,
  IME_TEXT_INPUT_USER_NAME,
  IME_TEXT_INPUT_NEW_PASSWORD,
  IME_TEXT_INPUT_NUMBER_DECIMAL,
  IME_TEXT_INPUT_ONE_TIME_CODE
};

enum ime_enter_key_type
{
  IME_ENTER_KEY_UNSPECIFIED = 0,
  IME_ENTER_KEY_NONE,
  IME_ENTER_KEY_GO,
  IME_ENTER_KEY_SEARCH,
  IME_ENTER_KEY_SEND,
  IME_ENTER_KEY_NEXT,
  IME_ENTER_KEY_DONE,
  IME_ENTER_KEY_PREVIOUS,
  IME_ENTER_KEY_NEWLINE
};

enum ime_request_keyboard_reason
{
  IME_REQUEST_KEYBOARD_NONE = 0,
  IME_REQUEST_KEYBOARD_MOUSE = 1,
  IME_REQUEST_KEYBOARD_TOUCH = 2,
  IME_REQUEST_KEYBOARD_OTHER = 20,
};

enum ime_move_direction {
  IME_MOVE_NONE = 0,
  IME_MOVE_UP = 1,
  IME_MOVE_DOWN,
  IME_MOVE_LEFT,
  IME_MOVE_RIGHT,
};

struct ime_cursor_info
{
  int left;
  int top;
  int width;
  int height;
};

struct ime_input_attribute
{
  int input_pattern;
  int enter_key_type;
};

struct ime_selection_range
{
  int start;
  int end;
};

struct ime_text_config
{
  struct ime_input_attribute input_attribute;
  struct ime_cursor_info cursor_info;
  struct ime_selection_range range;
  uint32_t windowId;
};

typedef enum {
    OHOS_IME_UNKNOWN = -1,
    OHOS_IME_INSERT_TEXT = 0,
    OHOS_IME_DELETE_BACKWARD,
    OHOS_IME_DELETE_FORWARD,
    OHOS_IME_SEND_FUNCTION_KEY,
    OHOS_IME_MOVE_CURSOR,
    OHOS_IME_EXIT_FULLSCREEN,
} ime_event_type;

typedef struct {
    int input_text_len;
    char* input_text;
    int delete_text_len;
    int move_direction;
    int enter_key_type;
} ime_event_data;

typedef struct {
    ime_event_type type;
    ime_event_data data;
} ime_event;

typedef void (*ime_event_callback)(const ime_event* event);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  // IME_EVENT_CALLBACK_H