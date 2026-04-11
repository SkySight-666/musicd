#include "musicd/music_daemon.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <thread>

namespace musicd {

namespace {

constexpr int kLoopSleepMs = 200;

}  // namespace

void MusicDaemon::SetStartupTrackUrl(const std::string& url) {
  startup_track_url_ = url;
}

int MusicDaemon::Run() {
  std::cout << "musicd starting, socket=" << socket_path_ << std::endl;
  if (!ipc_server_.Start(socket_path_)) {
    std::cerr << "musicd failed to start IPC server: " << socket_path_ << std::endl;
    return 1;
  }

  current_snapshot_ = output_manager_.Scan();
  ApplyOutputSnapshot(current_snapshot_);

  if (!startup_track_url_.empty()) {
    Track track;
    track.id = startup_track_url_;
    track.title = startup_track_url_;
    track.source_url = startup_track_url_;
    const std::string duration_ms = FetchDurationMs(startup_track_url_);
    if (!duration_ms.empty()) track.duration_ms = std::strtol(duration_ms.c_str(), nullptr, 10);
    queue_manager_.ReplaceQueue({track});
    state_.queue_index = queue_manager_.current_index();
    StartPlaybackForCurrentTrack();
  }

  PrintStatus();

  while (ipc_server_.running() && !should_exit_) {
    ipc_server_.PollOnce([this](const std::string& request) {
      return HandleCommand(request);
    });

    const bool still_running = playback_engine_.Poll();
    const AudioOutputSnapshot next_snapshot = output_manager_.Scan();
    if (next_snapshot.signature != current_snapshot_.signature) {
      PauseForOutputChange(next_snapshot);
      current_snapshot_ = next_snapshot;
    }

    if (!still_running && state_.playing && queue_manager_.AdvanceToNext()) {
      state_.queue_index = queue_manager_.current_index();
      StartPlaybackForCurrentTrack();
    } else if (!still_running && state_.playing) {
      state_.playing = false;
      state_.paused = false;
      state_.position_ms = 0;
    }

    UpdateStateFromEngine();
    std::this_thread::sleep_for(std::chrono::milliseconds(kLoopSleepMs));
  }

  playback_engine_.Stop();
  ipc_server_.Stop();
  return 0;
}

bool MusicDaemon::StartPlaybackForCurrentTrack() {
  const Track* track = queue_manager_.current_track();
  if (track == nullptr) return false;
  const int current_index = queue_manager_.current_index();
  if (!playback_engine_.Play(*track)) return false;
  state_.current_track = *track;
  state_.playing = true;
  state_.paused = false;
  state_.paused_by_output_change = false;
  state_.queue_index = current_index;
  return true;
}

void MusicDaemon::ApplyOutputSnapshot(const AudioOutputSnapshot& snapshot) {
  AudioOutput chosen = snapshot.preferred;
  if (!manual_output_name_.empty()) {
    for (const AudioOutput& output : snapshot.outputs) {
      if (output.name == manual_output_name_) {
        chosen = output;
        break;
      }
    }
  }
  playback_engine_.SetOutput(chosen);
  state_.audio_output_name = chosen.name;
  state_.audio_output_label = chosen.label;
}

void MusicDaemon::PauseForOutputChange(const AudioOutputSnapshot& snapshot) {
  ApplyOutputSnapshot(snapshot);
  if (!playback_engine_.is_running()) return;
  playback_engine_.Pause();
  state_.playing = false;
  state_.paused = true;
  state_.paused_by_output_change = true;
  std::cout << "musicd audio output changed, playback paused on " << state_.audio_output_label << std::endl;
}

void MusicDaemon::UpdateStateFromEngine() {
  state_.position_ms = static_cast<long>(playback_engine_.current_position_ms());
  state_.playing = playback_engine_.is_running() && !playback_engine_.is_paused();
  state_.paused = playback_engine_.is_paused();
  state_.queue_index = queue_manager_.current_index();
  const Track* current_track = queue_manager_.current_track();
  if (current_track != nullptr) state_.current_track = *current_track;
}

void MusicDaemon::PrintStatus() const {
  std::cout << "musicd preferred output: " << state_.audio_output_label << " [" << state_.audio_output_name << "]" << std::endl;
  if (!startup_track_url_.empty()) {
    std::cout << "musicd startup track: " << startup_track_url_ << std::endl;
  }
}

std::string MusicDaemon::HandleCommand(const std::string& request) {
  const std::string trimmed = Trim(request);
  if (trimmed.empty()) return "ERR empty command\n";

  std::string command;
  std::string value;
  const std::size_t first_space = trimmed.find_first_of(" \t");
  if (first_space == std::string::npos) {
    command = trimmed;
  } else {
    command = trimmed.substr(0, first_space);
    value = Trim(trimmed.substr(first_space + 1));
  }

  CommandArgs args;
  args.push_back(command);
  if (!value.empty()) args.push_back(value);

  if (command == "GET_STATE") return HandleGetState();
  if (command == "PLAY") return HandlePlay(args);
  if (command == "ENQUEUE") return HandleEnqueue(args);
  if (command == "PAUSE") return playback_engine_.Pause() ? "OK paused\n" : "ERR pause failed\n";
  if (command == "RESUME") {
    state_.paused_by_output_change = false;
    return playback_engine_.Resume() ? "OK resumed\n" : "ERR resume failed\n";
  }
  if (command == "STOP") {
    playback_engine_.Stop();
    state_.playing = false;
    state_.paused = false;
    state_.position_ms = 0;
    return "OK stopped\n";
  }
  if (command == "NEXT") {
    if (!queue_manager_.AdvanceToNext()) return "ERR no next track\n";
    state_.queue_index = queue_manager_.current_index();
    return StartPlaybackForCurrentTrack() ? "OK next\n" : "ERR next failed\n";
  }
  if (command == "LIST_OUTPUTS") return HandleListOutputs();
  if (command == "SET_OUTPUT") return HandleSetOutput(args);
  if (command == "QUIT") {
    should_exit_ = true;
    return "OK quitting\n";
  }
  return "ERR unknown command\n";
}

std::string MusicDaemon::HandlePlay(const CommandArgs& args) {
  if (args.size() < 2) return "ERR PLAY requires url\n";
  Track track;
  track.id = args[1];
  track.title = args[1];
  track.source_url = args[1];
  const std::string duration_ms = FetchDurationMs(args[1]);
  if (!duration_ms.empty()) track.duration_ms = std::strtol(duration_ms.c_str(), nullptr, 10);
  queue_manager_.ReplaceQueue({track});
  state_.queue_index = queue_manager_.current_index();
  return StartPlaybackForCurrentTrack() ? "OK playing\n" : "ERR play failed\n";
}

std::string MusicDaemon::HandleEnqueue(const CommandArgs& args) {
  if (args.size() < 2) return "ERR ENQUEUE requires url\n";
  Track track;
  track.id = args[1];
  track.title = args[1];
  track.source_url = args[1];
  const std::string duration_ms = FetchDurationMs(args[1]);
  if (!duration_ms.empty()) track.duration_ms = std::strtol(duration_ms.c_str(), nullptr, 10);
  queue_manager_.Enqueue(track);
  return "OK enqueued\n";
}

std::string MusicDaemon::HandleSetOutput(const CommandArgs& args) {
  if (args.size() < 2) return "ERR SET_OUTPUT requires name\n";
  manual_output_name_ = args[1];
  ApplyOutputSnapshot(current_snapshot_);
  if (playback_engine_.is_running()) {
    playback_engine_.Pause();
    state_.paused_by_output_change = true;
    return "OK output switched, playback paused\n";
  }
  return "OK output switched\n";
}

std::string MusicDaemon::HandleGetState() const {
  std::ostringstream response;
  response << "playing=" << (state_.playing ? 1 : 0) << "\n";
  response << "paused=" << (state_.paused ? 1 : 0) << "\n";
  response << "paused_by_output_change=" << (state_.paused_by_output_change ? 1 : 0) << "\n";
  response << "position_ms=" << state_.position_ms << "\n";
  response << "queue_index=" << state_.queue_index << "\n";
  response << "audio_output_name=" << state_.audio_output_name << "\n";
  response << "audio_output_label=" << state_.audio_output_label << "\n";
  response << BuildTrackLine(state_.current_track);
  response << "\n";
  return response.str();
}

std::string MusicDaemon::HandleListOutputs() const {
  std::ostringstream response;
  for (const AudioOutput& output : current_snapshot_.outputs) {
    response << output.name << "|" << output.label << "\n";
  }
  if (current_snapshot_.outputs.empty()) response << "default|ALSA: default\n";
  return response.str();
}

MusicDaemon::CommandArgs MusicDaemon::SplitCommand(const std::string& request) {
  std::istringstream stream(Trim(request));
  CommandArgs args;
  std::string token;
  while (stream >> token) {
    args.push_back(token);
  }
  return args;
}

std::string MusicDaemon::Trim(const std::string& value) {
  const std::string whitespace = " \t\r\n";
  const std::size_t begin = value.find_first_not_of(whitespace);
  if (begin == std::string::npos) return "";
  const std::size_t end = value.find_last_not_of(whitespace);
  return value.substr(begin, end - begin + 1);
}

std::string MusicDaemon::FetchDurationMs(const std::string& url) {
  std::ostringstream command;
  command
      << "ffprobe -v error -show_entries format=duration "
      << "-of default=noprint_wrappers=1:nokey=1 "
      << "'" << url << "' 2>/dev/null";
  FILE* pipe = popen(command.str().c_str(), "r");
  if (pipe == nullptr) return "";

  char buffer[128];
  std::string output;
  if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    output = Trim(buffer);
  }
  pclose(pipe);
  if (output.empty()) return "";
  const double seconds = std::atof(output.c_str());
  if (seconds <= 0.0) return "";
  return std::to_string(static_cast<long>(seconds * 1000.0));
}

std::string MusicDaemon::BuildTrackLine(const Track& track) {
  std::ostringstream response;
  response << "track_id=" << track.id << "\n";
  response << "track_title=" << track.title << "\n";
  response << "track_artist=" << track.artist << "\n";
  response << "track_album=" << track.album << "\n";
  response << "track_url=" << track.source_url << "\n";
  response << "track_duration_ms=" << track.duration_ms;
  return response.str();
}

}  // namespace musicd
