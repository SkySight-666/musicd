#ifndef MUSICD_MUSIC_DAEMON_H_
#define MUSICD_MUSIC_DAEMON_H_

#include <string>
#include <vector>

#include "musicd/audio_output_manager.h"
#include "musicd/ipc_server.h"
#include "musicd/playback_engine.h"
#include "musicd/queue_manager.h"
#include "musicd/types.h"

namespace musicd {

class MusicDaemon {
 public:
  void SetStartupTrackUrl(const std::string& url);
  int Run();

 private:
  using CommandArgs = std::vector<std::string>;

  bool StartPlaybackForCurrentTrack();
  void ApplyOutputSnapshot(const AudioOutputSnapshot& snapshot);
  void PauseForOutputChange(const AudioOutputSnapshot& snapshot);
  void UpdateStateFromEngine();
  void PrintStatus() const;

  std::string HandleCommand(const std::string& request);
  std::string HandlePlay(const CommandArgs& args);
  std::string HandleEnqueue(const CommandArgs& args);
  std::string HandleSetOutput(const CommandArgs& args);
  std::string HandleGetState() const;
  std::string HandleListOutputs() const;

  static CommandArgs SplitCommand(const std::string& request);
  static std::string Trim(const std::string& value);
  static std::string FetchDurationMs(const std::string& url);
  static std::string BuildTrackLine(const Track& track);

  std::string startup_track_url_;
  bool should_exit_ = false;
  std::string socket_path_ = "/tmp/musicd.sock";
  std::string manual_output_name_;
  AudioOutputSnapshot current_snapshot_;
  AudioOutputManager output_manager_;
  IpcServer ipc_server_;
  PlaybackEngine playback_engine_;
  QueueManager queue_manager_;
  PlayerState state_;
};

}  // namespace musicd

#endif  // MUSICD_MUSIC_DAEMON_H_
