// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_MULTIPROCESS_SCOPED_SOCKET_H_
#define OHOS_ADAPTER_MULTIPROCESS_SCOPED_SOCKET_H_

namespace ohos::adapter::multiprocess {

class ScopedSocket {
 public:
  explicit ScopedSocket(int fd);

  int GetSocket() { return fd_; }

  int Accept();

  int Close();

  ~ScopedSocket();

  bool Valid() const {
    return fd_ != -1;
  }

 private:
  int fd_ = 0;
};
}  // namespace ohos::adapter::multiprocess

#endif  // OHOS_ADAPTER_MULTIPROCESS_SCOPED_SOCKET_H_
