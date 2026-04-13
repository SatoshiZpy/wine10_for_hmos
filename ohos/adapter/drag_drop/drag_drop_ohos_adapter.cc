// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/drag_drop/drag_drop_ohos_adapter.h"

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/file_manager/file_manager_adapter.h"
#include "ohos/adapter/xcomponent/xcomponent_manager.h"

using namespace ohos::adapter::xcomponent;

namespace ohos {
namespace adapter {

namespace {
const int IMAGE_PIXEL_MAP = 4;
}  // namespace

DragDropOhosAdapter::DragDropOhosAdapter() {}
DragDropOhosAdapter& DragDropOhosAdapter::GetInstance() {
  static DragDropOhosAdapter adapter;
  return adapter;
}

void DragDropOhosAdapter::ExecuteDrag(int32_t width,
                                      int32_t height,
                                      std::shared_ptr<char[]> buff,
                                      int32_t widget_id) {
  if (auto func = ohos::adapter::GetJSFunction("DragDropAdapter.StartDrag")) {
    aki::ArrayBuffer pixel_buffer((uint8_t*)buff.get(),
                                   width * height * IMAGE_PIXEL_MAP);
    aki::ArrayBuffer bookmark_buffer(drag_info_.bookmarkData.data(),
                                     drag_info_.bookmarkData.size());
    aki::ArrayBuffer web_custom_buffer(drag_info_.webCustomData.data(),
                                       drag_info_.webCustomData.size());
    OhosDragParamToJs drag_param;
    drag_param.basicData.text = drag_info_.basicData.text;
    drag_param.basicData.url = drag_info_.basicData.url;
    drag_param.basicData.urlTitle = drag_info_.basicData.urlTitle;
    drag_param.basicData.html = drag_info_.basicData.html;
    drag_param.imageTempUri = drag_info_.imageTempUri;
    drag_param.bookmarkBuffer = std::move(bookmark_buffer);
    drag_param.webCustomBuffer = std::move(web_custom_buffer);
    drag_param.pixelBuffer = std::move(pixel_buffer);
    drag_param.height = height;
    drag_param.width = width;
    drag_param.widgetId = widget_id;
    drag_param.touchX = drag_info_.touchX;
    drag_param.touchY = drag_info_.touchY;
    func->Invoke<void>(drag_param);
  }
}

void HandleDragInfo(const aki::Value drag_info_value,
                    const std::vector<std::string>& file_paths,
                    OhosDragInfo& drag_info) {
  aki::Value basic_data = drag_info_value["basicData"];
  drag_info.basicData.text = basic_data["text"].As<std::string>();
  drag_info.basicData.url = basic_data["url"].As<std::string>();
  drag_info.basicData.urlTitle = basic_data["urlTitle"].As<std::string>();
  drag_info.basicData.html = basic_data["html"].As<std::string>();
  drag_info.filePaths = file_paths;
  drag_info.bookmarkData = std::vector<uint8_t>(
      drag_info_value["bookmarkBuffer"].As<aki::ArrayBuffer>().GetData(),
      drag_info_value["bookmarkBuffer"].As<aki::ArrayBuffer>().GetData() +
          drag_info_value["bookmarkBuffer"].As<aki::ArrayBuffer>().GetLength());
  drag_info.webCustomData = std::vector<uint8_t>(
      drag_info_value["webCustomBuffer"].As<aki::ArrayBuffer>().GetData(),
      drag_info_value["webCustomBuffer"].As<aki::ArrayBuffer>().GetData() +
          drag_info_value["webCustomBuffer"].As<aki::ArrayBuffer>().GetLength());
}

void OnDragEnterCB(int32_t widget_id,
                   const aki::Value drag_info_value,
                   const std::vector<std::string>& file_paths) {
  auto render = XComponentManager::GetInstance()->GetXComponent(widget_id);
  if (render == nullptr) {
    LOGE("[OhosDrag] [drag-enter-event]can not get render: %{public}d", widget_id);
    return;
  }
  OhosDragInfo drag_info;
  HandleDragInfo(drag_info_value, file_paths, drag_info);
  render->OnDragEnterEvent(drag_info);
}

void OnDropCB(int32_t widget_id,
              const aki::Value drag_info_value,
              const std::vector<std::string>& file_uris) {
  auto render = XComponentManager::GetInstance()->GetXComponent(widget_id);
  if (render == nullptr) {
    LOGE("[OhosDrag] [drag-drop-event]can not get render: %{public}d", widget_id);
    return;
  }
  std::vector<std::string> file_paths;
  for (const auto& uri : file_uris) {
    std::string file_path;
    ohos::adapter::FileManagerAdapter::GetInstance().GetPathForUri(uri.c_str(),
                                                                   file_path);
    if (file_path.empty()) {
        LOGE("[OhosDrag] GetPathForUri failed");
        continue;
    }
    file_paths.push_back(std::move(file_path));
  }
  OhosDragInfo drag_info;
  HandleDragInfo(drag_info_value, file_paths, drag_info);
  render->OnDropEvent(drag_info);
}

void OnDragMoveCB(int32_t widget_id,
                  const float window_x,
                  const float window_y) {
  auto render = XComponentManager::GetInstance()->GetXComponent(widget_id);
  if (render == nullptr) {
    LOGE("[OhosDrag] [drag-move-event]can not get render: %{public}d", widget_id);
    return;
  }
  render->OnDragMoveEvent(window_x, window_y);
}

void OnDragLeaveCB(int32_t widget_id) {
  auto render = XComponentManager::GetInstance()->GetXComponent(widget_id);
  if (render == nullptr) {
    LOGE("[OhosDrag] [drag-leave-event]can not get render: %{public}d", widget_id);
    return;
  }
  render->OnDragLeaveEvent();
}

void OnDragEndCB(int32_t widget_id) {
  auto render = XComponentManager::GetInstance()->GetXComponent(widget_id);
  if (render == nullptr) {
    LOGE("[OhosDrag] [drag-end-event]can not get render: %{public}d", widget_id);
    return;
  }
  render->OnDragEndEvent();
}

std::string DragDropOhosAdapter::GetDraggedExtensionFileName() {
  return dragged_extension_file_name_;
}

void DragDropOhosAdapter::SetDraggedExtensionFileName(const std::string& file_name) {
  dragged_extension_file_name_ = file_name;
}

JSBIND_CLASS(OhosDragParamToJs) {
  JSBIND_CONSTRUCTOR<>();
  JSBIND_PROPERTY(basicData);
  JSBIND_PROPERTY(imageTempUri);
  JSBIND_PROPERTY(bookmarkBuffer);
  JSBIND_PROPERTY(webCustomBuffer);
  JSBIND_PROPERTY(pixelBuffer);
  JSBIND_PROPERTY(width);
  JSBIND_PROPERTY(height);
  JSBIND_PROPERTY(widgetId);
  JSBIND_PROPERTY(touchX);
  JSBIND_PROPERTY(touchY);
}

JSBIND_CLASS(OhosBasicDragData) {
  JSBIND_CONSTRUCTOR<>();
  JSBIND_PROPERTY(text);
  JSBIND_PROPERTY(url);
  JSBIND_PROPERTY(urlTitle);
  JSBIND_PROPERTY(html);
}

JSBIND_GLOBAL() {
  JSBIND_FUNCTION(OnDragEnterCB);
  JSBIND_FUNCTION(OnDropCB);
  JSBIND_FUNCTION(OnDragLeaveCB);
  JSBIND_FUNCTION(OnDragEndCB);
  JSBIND_FUNCTION(OnDragMoveCB);
}

}  // namespace adapter
}  // namespace ohos
