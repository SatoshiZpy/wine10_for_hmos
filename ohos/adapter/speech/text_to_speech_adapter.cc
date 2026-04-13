// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/speech/text_to_speech_adapter.h"

namespace ohos::adapter::textToSpeech {

TextToSpeechAdapter::TextToSpeechAdapter() {}

TextToSpeechAdapter& TextToSpeechAdapter::GetInstance() {
  static TextToSpeechAdapter textToSpeechAdapter;
  return textToSpeechAdapter;
}

void TextToSpeechAdapter::RegisterCallback(std::function<void()> callback,
                                           OhosTtsEvent type) {
  switch (type) {
    case TTS_ON_START:
      start_event_callback_ = callback;
      break;
    case TTS_ON_END:
      end_event_callback_ = callback;
      break;
    default:
      break;
  }
}

bool TextToSpeechAdapter::GetAkiBooleanResult(
    const std::string& func_name,
    const EngineCreationParams& extra_params) {
  if (start_event_callback_ == nullptr || end_event_callback_ == nullptr) {
    LOGE(
        "TTS TextToSpeechAdapter::GetAkiBooleanResult registered callback is "
        "null");
  }

  auto insert = ohos::adapter::GetJSFunction(func_name);
  std::promise<bool> insert_promise;
  std::function<void(bool)> callback = [&insert_promise](bool msg) {
    insert_promise.set_value(msg);
  };
  insert->Invoke<void>(callback, start_event_callback_, end_event_callback_,
                       extra_params);
  bool msg = insert_promise.get_future().get();
  return msg;
}

bool TextToSpeechAdapter::InitializeTextToSpeechEngine(
    const EngineCreationParams& params) {
  bool succeed = false;
  auto func = ohos::adapter::GetJSFunction(
      "SpeechAdapter.createTextToSpeechEngineAndSetListener");
  if (func) {
    succeed = TextToSpeechAdapter::GetAkiBooleanResult(
        "SpeechAdapter.createTextToSpeechEngineAndSetListener", params);
  } else {
    is_initialized_ = false;
    is_supported_ = false;
    LOGE(
        "TTS Cannot find "
        "ets-SpeechAdapter.createTextToSpeechEngineAndSetListener "
        "method");
    return false;
  }

  if (succeed) {
    is_initialized_ = true;
    is_supported_ = true;
  } else {
    is_initialized_ = false;
    is_supported_ = false;
    LOGE(
        "TTS Some error occurred while calling the "
        "ets-SpeechAdapter.createTextToSpeechEngineAndSetListener method.");
  }
  return succeed;
}

bool TextToSpeechAdapter::TextToSpeechlSupported() {
  return is_supported_;
}

bool TextToSpeechAdapter::TextToSpeechInitialized() {
  bool is_initialized;
  auto func = ohos::adapter::GetJSFunction("SpeechAdapter.isInitialized");
  if (func) {
    is_initialized = func->Invoke<bool>();
    std::string result = is_initialized ? "succeeded" : "failed";
    is_initialized
        ? LOGI("TTS OHOS ttsEngine initialization result is: %{public}s",
               result.c_str())
        : LOGE("TTS OHOS ttsEngine initialization result is:  %{public}s",
               result.c_str());
    return is_initialized;
  }
  LOGE("TTS Cannot find ets-SpeechAdapter.isInitialized method.");
  return false;
}

OhOsVoices TextToSpeechAdapter::GetAkiOhOsVoicesArrayResult(
    const std::string& func_name) {
  auto insert = ohos::adapter::GetJSFunction(func_name);

  std::promise<OhOsVoices> insert_promise;
  std::function<void(aki::Value)> callback = [&insert_promise](aki::Value msg) {
    if (!msg["voiceList"].IsArray()) {
      OhOsVoices empty;
      insert_promise.set_value(empty);
      LOGE(
          "TTS TextToSpeechAdapter::GetAkiOhOsVoicesArrayResult obtained voice "
          "list is not a array");
      return;
    };
    OhOsVoices voiceList;
    aki::Value arr = msg["voiceList"];
    int len = msg["length"].As<int>();
    for (int i = 0; i < len; i++) {
      VoiceInfo voiceInfo;
      voiceInfo.language = arr[i]["language"].As<std::string>();
      voiceInfo.person = arr[i]["person"].As<int>();
      voiceInfo.style = arr[i]["style"].As<std::string>();
      voiceInfo.status = arr[i]["status"].As<std::string>();
      voiceInfo.gender = arr[i]["gender"].As<std::string>();
      voiceInfo.description = arr[i]["description"].As<std::string>();
      voiceList.emplace_back(voiceInfo);
    }
    insert_promise.set_value(std::move(voiceList));
  };

  insert->Invoke<void>(callback);
  OhOsVoices msg = insert_promise.get_future().get();
  return msg;
}

OhOsVoices TextToSpeechAdapter::TextToSpeechEngineListVoices() {
  auto func = ohos::adapter::GetJSFunction(
      "SpeechAdapter.textToSpeechEngineListVoices");
  if (func) {
    voices_ = TextToSpeechAdapter::GetAkiOhOsVoicesArrayResult(
        "SpeechAdapter.textToSpeechEngineListVoices");
    if (voices_.empty()) {
      LOGE("TTS The obtained voice list is empty.");
    }
  } else {
    LOGE(
        "TTS Cannot find ets-SpeechAdapter.textToSpeechEngineListVoices "
        "method.");
  }
  return voices_;
}

bool TextToSpeechAdapter::Speak(const std::string& parsed_utterance,
                                const SpeakingParams& params) {
  if (!is_initialized_) {
    LOGE("TTS The ttsEngine has not been initialized. Cannot Speak");
    return false;
  }

  bool succeed = false;
  auto func = ohos::adapter::GetJSFunction("SpeechAdapter.speak");
  if (func) {
    succeed = func->Invoke<bool>(parsed_utterance, params);
    if (succeed) {
      return true;
    } else {
      LOGE(
          "TTS Some error occurred while calling the ets-SpeechAdapter.Speak "
          "method.");
      return false;
    }
  }
  LOGE("TTS Cannot find ets-SpeechAdapter.Speak method");
  return false;
}

bool TextToSpeechAdapter::StopSpeaking() {
  if (!is_initialized_) {
    LOGE("TTS The ttsEngine has not been initialized. Cannot StopSpeaking");
    return false;
  }

  bool succeed = false;
  auto func = ohos::adapter::GetJSFunction("SpeechAdapter.stopSpeaking");
  if (func) {
    succeed = func->Invoke<bool>();
    if (succeed) {
      return true;
    } else {
      LOGE(
          "TTS Some error occurred while calling the "
          "ets-SpeechAdapter.stopSpeaking method.");
      return false;
    }
  }
  LOGE("TTS Cannot find ets-SpeechAdapter.stopSpeaking method.");
  return false;
}

bool TextToSpeechAdapter::IsSpeaking() {
  if (!is_initialized_) {
    LOGE("TTS The ttsEngine has not been initialized. Cannot is_initialized_");
    return false;
  }

  auto func = ohos::adapter::GetJSFunction("SpeechAdapter.isSpeaking");
  if (func) {
    return func->Invoke<bool>();
  }
  LOGE("TTS Cannot find ets-SpeechAdapter.isSpeaking method.");
  return false;
}

bool TextToSpeechAdapter::Shutdown() {
  bool succeed = false;

  auto func =
      ohos::adapter::GetJSFunction("SpeechAdapter.textToSpeechEngineShutdown");
  if (func) {
    succeed = func->Invoke<bool>();
  } else {
    LOGE(
        "TTS Cannot find ets-SpeechAdapter.textToSpeechEngineShutdown method.");
    return false;
  }

  if (succeed) {
    is_initialized_ = false;
    return succeed;
  } else {
    LOGE(
        "TTS Some error occurred while calling the "
        "ets-SpeechAdapter.textToSpeechEngineShutdown method.");
    return succeed;
  }
}

JSBIND_CLASS(SpeakingParamsExtraParams) {
  JSBIND_PROPERTY(speed);
  JSBIND_PROPERTY(volume);
  JSBIND_PROPERTY(pitch);
  JSBIND_PROPERTY(languageContext);
  JSBIND_PROPERTY(audioType);
  JSBIND_PROPERTY(playType);
  JSBIND_PROPERTY(soundChannel);
  JSBIND_PROPERTY(queueMode);
}

JSBIND_CLASS(SpeakingParams) {
  JSBIND_PROPERTY(requestId);
  JSBIND_PROPERTY(extraParams);
}

JSBIND_CLASS(EngineCreationParamsExtraParams) {
  JSBIND_PROPERTY(style);
  JSBIND_PROPERTY(locate);
  JSBIND_PROPERTY(name);
}

JSBIND_CLASS(EngineCreationParams) {
  JSBIND_PROPERTY(language);
  JSBIND_PROPERTY(online);
  JSBIND_PROPERTY(person);
  JSBIND_PROPERTY(extraParams);
}

JSBIND_CLASS(VoiceInfo) {
  JSBIND_PROPERTY(language);
  JSBIND_PROPERTY(person);
  JSBIND_PROPERTY(style);
  JSBIND_PROPERTY(status);
  JSBIND_PROPERTY(gender);
  JSBIND_PROPERTY(description);
}

}  // namespace ohos::adapter::textToSpeech
