// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/cert_manager/cert_manager_adapter.h"

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/trace.h"

namespace ohos::adapter {

constexpr int kListCertsInfoWaitTime = 3;

CertManagerAdapter& CertManagerAdapter::GetInstance() {
  static CertManagerAdapter instance;
  return instance;
}

CertManagerAdapter::CertInfoList CertManagerAdapter::ListCertsInfo() {
  TRACE_EVENT_0("CertManagerAdapter::ListCertsInfo");
  auto promise = std::make_shared<std::promise<bool>>();
  auto cert_infos = std::make_shared<CertInfoList>();
  std::function<void(aki::Value, int32_t)> callback =
      [promise, cert_infos, this](aki::Value ohos_cert_infos, int32_t cert_count) -> void {
    if (!ohos_cert_infos.IsArray()) {
      promise->set_value(true);
      return;
    }

    for (int32_t i = 0; i < cert_count; i++) {
      OhosCertInfo cert_info;
      ConvertCertInfo(ohos_cert_infos[i], &cert_info);
      cert_info.type = CertType::USER_CERT;
      cert_infos->push_back(cert_info);
    }
    promise->set_value(true);
  };

  auto func = ohos::adapter::GetJSFunction(
      "CertManagerAdapter.GetAllPrivateCertificates");
  if (func) {
    func->Invoke<void>(callback);
    auto future = promise->get_future();
    auto status = future.wait_for(std::chrono::seconds(kListCertsInfoWaitTime));
    if (status == std::future_status::timeout) {
      LOGE("CertManagerAdapter.ListCertsInfo Wait timeout");
      return *cert_infos;
    }
  }
  return *cert_infos;
}

int CertManagerAdapter::InstallPersonalCert(std::shared_ptr<char[]> cert_data,
                                            uint32_t cert_len,
                                            const std::string& cert_pass,
                                            const std::string& alias) {
  TRACE_EVENT_0("CertManagerAdapter::InstallPersonalCert");
  auto promise = std::make_shared<std::promise<int32_t>>();
  std::function<void(int32_t)> callback =
      [promise](int32_t ret_value) -> void { promise->set_value(ret_value); };

  auto func =
      ohos::adapter::GetJSFunction("CertManagerAdapter.InstallPersonalCert");
  if (func) {
    aki::ArrayBuffer certArrayBuffer((uint8_t*)cert_data.get(), cert_len);
    func->Invoke<void>(std::move(certArrayBuffer), cert_pass, alias, callback);
    auto future = promise->get_future();
    auto status = future.wait_for(std::chrono::seconds(3));
    if (status == std::future_status::timeout) {
      LOGE("CertManagerAdapter.InstallPersonalCert Wait timeout");
      return -1;
    }
    bool result = future.get();
    return result;
  }

  return -1;
}

int CertManagerAdapter::UninstallPersonalCert(const std::string& uri) {
  TRACE_EVENT_0("CertManagerAdapter::UninstallPersonalCert");
  int ret = -1;
  std::promise<bool> promise;
  std::function<void(int32_t)> callback = [&](int32_t ret_value) -> void {
    ret = ret_value;
    promise.set_value(true);
  };
  auto func = ohos::adapter::GetJSFunction("CertManagerAdapter.RemoveCertByUri");
  if (func) {
    func->Invoke<void>(uri, callback);
  }
  bool result = promise.get_future().get();
  return ret;
}

int CertManagerAdapter::SignDataByUri(const std::string& uri,
                                      uint8_t* input,
                                      size_t inputlen,
                                      uint8_t* out,
                                      size_t* outlen) {
  TRACE_EVENT_0("CertManagerAdapter::SignByCertUri");
  int ret = -1;
  auto func = ohos::adapter::GetJSFunction("CertManagerAdapter.SignByCertUri");
  if (func) {
    std::promise<aki::ArrayBuffer> promise;
    std::function<void(aki::ArrayBuffer)> callback =
        [&promise](aki::ArrayBuffer buff) { promise.set_value(buff); };
    aki::ArrayBuffer inputBuffer(input, inputlen);
    func->Invoke<void>(uri, inputBuffer, callback);
    aki::ArrayBuffer buffer = promise.get_future().get();
    *outlen = buffer.GetLength();
    if (out) {
      memcpy(out, buffer.GetData(), *outlen);
    }
    ret = 0;
  }
  return ret;
}

CertManagerAdapter::CertInfoList CertManagerAdapter::ListCACertsInfo(const std::string& pathDir) {
  TRACE_EVENT_0("CertManagerAdapter::ListCACertsInfo");
  CertInfoList cert_infos;
  std::vector<std::string> cert_crl_list = GetCertCrl(pathDir, false);
  for (uint32_t i = 0; i < cert_crl_list.size(); i++) {
    OhosCertInfo cert_info;
    cert_info.cert = cert_crl_list[i];
    cert_info.uri = "";
    cert_info.type = CertType::CA_CERT;
    cert_infos.push_back(cert_info);
  }
  return cert_infos;
}

int32_t CertManagerAdapter::GetUserId() {
  if (user_id_ != 0)
    return user_id_;

  auto func = ohos::adapter::GetJSFunction("CertManagerAdapter.ReturnUserId");
  if (func) {
    std::promise<int32_t> promise;
    std::function<void(int32_t)> callback =
      [&promise](int32_t user_id) -> void { promise.set_value(user_id); };

    func->Invoke<void>(callback);
    user_id_ = promise.get_future().get();
  }
  return user_id_;
}

std::vector<std::string> CertManagerAdapter::GetCertCrl(const std::string& pathDirPrefix, bool getUserId) {
  std::vector<std::string> cert_crl;
  auto func = ohos::adapter::GetJSFunction("CertManagerAdapter.GetCertCrl");
  if (func) {
    std::promise<bool> promise;
    std::function<void(std::vector<std::string>, int32_t)> callback =
        [&](std::vector<std::string>cert_url, int32_t len) {
          for (int32_t i = 0; i < len; i++) {
            cert_crl.push_back(cert_url[i]);
          }
          promise.set_value(true);
        };

    func->Invoke<void>(pathDirPrefix, getUserId, callback);
    promise.get_future().get();
  }
  return cert_crl;
}

void CertManagerAdapter::ConvertCertInfo(aki::Value cert_info_value,
                                         OhosCertInfo* cert_info) {
  cert_info->cert = cert_info_value["cert"].As<std::string>();
  cert_info->uri = cert_info_value["uri"].As<std::string>();
}

void CertManagerAdapter::ShowCertificateManagerDialog() {
  if (auto func = ohos::adapter::GetJSFunction(
      "CertManagerAdapter.ShowCertificateManagerDialog")) {
    func->Invoke<void>();
  }
}

JSBIND_CLASS(OhosCertInfo) {
  JSBIND_PROPERTY(cert);
  JSBIND_PROPERTY(uri);
}

}  // namespace ohos::adapter
