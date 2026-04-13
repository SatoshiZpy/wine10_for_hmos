// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/multiprocess/scoped_socket.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>
namespace ohos::adapter::multiprocess {

ohos::adapter::multiprocess::ScopedSocket::ScopedSocket(int fd) : fd_(fd) {}

ohos::adapter::multiprocess::ScopedSocket::~ScopedSocket() {}
int ScopedSocket::Accept() {
  return accept(fd_, nullptr, nullptr);
}

int ScopedSocket::Close() {
  return close(fd_);
}
}  // namespace ohos::adapter::multiprocess
