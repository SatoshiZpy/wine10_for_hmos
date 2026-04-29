// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_COMMON_H_
#define OHOS_ADAPTER_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define MAX_SEND_FDS 4

struct window_info {
  void* window;
  char *window_id;
  int widget_id;
};

typedef void (*ON_WINDOW_ADD_FUNC)(const struct window_info*);
typedef void (*ON_WINDOW_REMOVE_FUNC)(const struct window_info*);

struct window_status_listener {
  ON_WINDOW_ADD_FUNC window_add;
  ON_WINDOW_REMOVE_FUNC window_remove;
};

enum ability_type
{
  ENTRY_ABILITY,
  EMBEDDED_ABILITY
};

enum window_type
{
  MAIN_WINDOW,
  POPUP_WINDOW,
  MENU,
  TOOLTIP,
  DRAG_WINDOW,
  BUBBLE_WINDOW
};

enum launch_process_type
{
  MAIN_PROCESS,
  SERVER_PROCESS,
  CHILD_PROCESS
};

enum global_descriptors {
  SERVER_IPC_CHANNEL = 100,
  FILE_DIR_DESCRIPTOR,
  IPC_DESCRIPTOR_MAX
};

struct window_bounds {
  int left;
  int top;
  int width;
  int height;
};

struct window_features {
  int title_bar;
  int close_button;
  int minimize_button;
  int maximize_button;
  int resizable;
  int touchable;
  int shadow;
  int prevents_app_activation;
  int use_floating_window;
  int use_dark_mode;
};

struct window_state {
  int  disabled;
  int  no_foreground;
  int  top_most;
  int  excluded_by_control;
  int  minimized;
  int  minimized_valid;
  int  maximized;
  int  full_screen;
  int  split_mode;
  int  visiable;
};

struct window_param
{
  enum ability_type container;
  enum window_type type;
  int parent_id;
  int background_color;

  struct window_bounds bounds;
  struct window_features features;
  struct window_state state;

  int hwnd;
  int pid;
};

struct launch_options
{
  enum launch_process_type type;
  char *entry_point;
  char *extra_env;

  int fds[MAX_SEND_FDS];
  int id[MAX_SEND_FDS];
};

typedef void (*screenlock_event_callback)(const char* event_type);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  // OHOS_ADAPTER_API_H_
