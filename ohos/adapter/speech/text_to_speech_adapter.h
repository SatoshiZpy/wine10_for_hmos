// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_SPEECH_TEXT_TO_SPEECH_ADAPTER_H
#define OHOS_ADAPTER_SPEECH_TEXT_TO_SPEECH_ADAPTER_H

#include <cstdint>
#include <memory>
#include <string>

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/export.h"

namespace ohos::adapter::textToSpeech {

enum OhosTtsEvent {
  TTS_ON_START = 0,
  TTS_ON_END = 1,
  TTS_ON_WORD = 2,
  TTS_ON_ERROR = 3,
  TTS_ON_CANCELLED = 4,
  TTS_ON_INTERRUPTED = 5,
  TTS_ON_PAUSE = 6,
  TTS_ON_RESUME = 7
};

struct VoiceInfo {
  std::string language;
  int person;
  std::string style;
  // GA:available / EOM:discard
  std::string status;
  std::string gender;
  std::string description;
};

using OhOsVoices = std::vector<VoiceInfo>;

struct EngineCreationParamsExtraParams {
  std::string style;
  std::string locate;
  std::string name;
};

struct EngineCreationParams {
  std::string language;
  int online;
  int person;
  EngineCreationParamsExtraParams extraParams;
};

struct SpeakingParamsExtraParams {
  double speed;
  double volume;
  double pitch;
  std::string languageContext;
  std::string audioType;
  int playType;
  int soundChannel;
  int queueMode;
};

struct SpeakingParams {
  std::string requestId;
  SpeakingParamsExtraParams extraParams;
};

class ADAPTER_EXPORT_API TextToSpeechAdapter {
 public:
  static TextToSpeechAdapter& GetInstance();
  TextToSpeechAdapter(const TextToSpeechAdapter&) = delete;
  TextToSpeechAdapter(TextToSpeechAdapter&&) = delete;
  TextToSpeechAdapter& operator=(const TextToSpeechAdapter&) = delete;
  virtual ~TextToSpeechAdapter() = default;

  TextToSpeechAdapter();
  bool InitializeTextToSpeechEngine(const EngineCreationParams& params);
  OhOsVoices TextToSpeechEngineListVoices();

  bool TextToSpeechlSupported();
  bool TextToSpeechInitialized();

  bool Speak(const std::string& parsed_utterance, const SpeakingParams& params);
  bool IsSpeaking();
  bool StopSpeaking();
  bool Shutdown();

  // OH is not supported pause and resume.
  // The current solution is to obtain audio streams using the onData method.
  void Pause();
  void Resume();

  void TextToSpeechEngineGetArrayBuffer();
  void TextToSpeechEngineOnError();

  void RegisterCallback(std::function<void()>, OhosTtsEvent type);

 private:
  bool GetAkiBooleanResult(const std::string& func_name,
                           const EngineCreationParams& extra_params);
  OhOsVoices GetAkiOhOsVoicesArrayResult(const std::string& func_name);

  bool is_supported_ = false;
  bool is_initialized_ = false;

  OhOsVoices voices_;

  std::function<void()> start_event_callback_;
  std::function<void()> end_event_callback_;
};

}  // namespace ohos::adapter::textToSpeech

#endif  // OHOS_ADAPTER_SPEECH_TEXT_TO_SPEECH_ADAPTER_H
