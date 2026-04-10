#ifndef MUSICD_PLAYBACK_ENGINE_H_
#define MUSICD_PLAYBACK_ENGINE_H_

#include <cstdint>
#include <string>
#include <sys/types.h>

#include "musicd/audio_output_manager.h"
#include "musicd/types.h"

namespace musicd {

class PlaybackEngine {
 public:
  PlaybackEngine();
  ~PlaybackEngine();

  void SetOutput(const AudioOutput& output);

  bool Play(const Track& track);
  bool Pause();
  bool Resume();
  void Stop();
  bool Poll();

  bool is_running() const;
  bool is_paused() const;
  int64_t current_position_ms() const;
  const AudioOutput& output() const;

 private:
  static std::string EscapeShellArg(const std::string& value);
  static int64_t NowMs();
  bool SendSignalToProcessGroup(int signal_number);

  AudioOutput output_;
  pid_t process_group_id_;
  bool paused_;
  int64_t seek_base_ms_;
  int64_t start_ms_;
  int64_t pause_at_ms_;
  int64_t paused_acc_ms_;
  Track current_track_;
};

}  // namespace musicd

#endif  // MUSICD_PLAYBACK_ENGINE_H_
