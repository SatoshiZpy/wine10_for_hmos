// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_CERT_MANAGER_CERT_MANAGER_ADAPTER_H_
#define OHOS_ADAPTER_CERT_MANAGER_CERT_MANAGER_ADAPTER_H_

#include <napi/native_api.h>

#include <string>

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/export.h"

namespace ohos::adapter {

enum CertType {
  OTHER_CERT,
  CA_CERT,
  USER_CERT,
  SERVER_CERT,
  NUM_CERT_TYPES
};

// Holds a certificate along with additional information.
struct OhosCertInfo {
  std::string cert;
  std::string uri;
  CertType type;
};

class ADAPTER_EXPORT_API CertManagerAdapter {
 public:
  static CertManagerAdapter& GetInstance();

  CertManagerAdapter() = default;
  CertManagerAdapter(const CertManagerAdapter&) = delete;
  CertManagerAdapter& operator=(const CertManagerAdapter&) = delete;

  virtual ~CertManagerAdapter() = default;

  using CertInfoList = std::vector<OhosCertInfo>;

  virtual CertInfoList ListCertsInfo();
  CertInfoList ListCACertsInfo(const std::string& pathDir);
  int32_t GetUserId();
  virtual int InstallPersonalCert(std::shared_ptr<char[]> cert_data,
                          uint32_t len,
                          const std::string& cert_pass,
                          const std::string& alias);
  int UninstallPersonalCert(const std::string& uri);
  int SignDataByUri(const std::string& uri,
                    uint8_t* input,
                    size_t inputlen,
                    uint8_t* out,
                    size_t* outlen);

  void ConvertCertInfo(aki::Value cert_info_value, OhosCertInfo* cert_info);

  std::vector<std::string> GetCertCrl(const std::string& pathDirPrefix, bool getUserId);
  void ShowCertificateManagerDialog();

 private:
  int32_t user_id_ = 0;
};

}  // namespace ohos::adapter

#endif  // OHOS_ADAPTER_CERT_MANAGER_CERT_MANAGER_ADAPTER_H_
