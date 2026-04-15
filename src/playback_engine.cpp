#include "musicd/playback_engine.h"

#include <cerrno>
#include <csignal>
#include <cstdint>
#include <ctime>
#include <fcntl.h>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace musicd {

namespace {

constexpr const char* kLogPath = "/tmp/musicd.log";

std::string NormalizeOutputName(const std::string& raw) {
  std::string out;
  out.reserve(raw.size());
  for (char ch : raw) {
    if (ch >= 'A' && ch <= 'Z') out.push_back(static_cast<char>(ch - 'A' + 'a'));
    else if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') continue;
    else out.push_back(ch);
  }
  return out;
}

std::string ResolvePlaybackDeviceName(const std::string& raw) {
  const std::string normalized = NormalizeOutputName(raw);

  if (normalized.empty()) return "speaker";
  if (normalized.rfind("bluealsa:", 0) == 0) return raw;

  if (normalized == "speaker" ||
      normalized == "default" ||
      normalized == "hw:1,0" ||
      normalized == "plughw:1,0") {
    return "speaker";
  }

  if (normalized == "tc_analog" ||
      normalized == "analog_hs" ||
      normalized == "analoghs" ||
      normalized == "hw:3,0" ||
      normalized == "plughw:3,0") {
    return "tc_analog";
  }

  if (normalized == "tc_digital" ||
      normalized == "digital_hs" ||
      normalized == "digitalhs" ||
      normalized == "hw:4,0" ||
      normalized == "plughw:4,0") {
    return "tc_digital";
  }

  return raw;
}

}  // namespace

PlaybackEngine::PlaybackEngine()
    : process_group_id_(-1),
      paused_(false),
      seek_base_ms_(0),
      start_ms_(0),
      pause_at_ms_(0),
      paused_acc_ms_(0) {}

PlaybackEngine::~PlaybackEngine() {
  Stop();
}

void PlaybackEngine::SetOutput(const AudioOutput& output) {
  output_ = output;
  output_.name = ResolvePlaybackDeviceName(output.name);
}

bool PlaybackEngine::Play(const Track& track) {
  if (track.source_url.empty()) return false;
  Stop();

  std::string device_name = output_.name.empty() ? "speaker" : output_.name;
  std::ostringstream command;
  command
      << "ffmpeg -nostdin -hide_banner -loglevel error -i "
      << EscapeShellArg(track.source_url)
      << " -vn -af aresample=44100 -c:a pcm_s16le -f s16le -ar 44100 -ac 2 -"
      << " 2>>" << EscapeShellArg(kLogPath)
      << " | aplay -q -f S16_LE -r 44100 -c 2 -D "
      << EscapeShellArg(device_name)
      << " -";

  const pid_t child_pid = fork();
  if (child_pid < 0) return false;

  if (child_pid == 0) {
    setsid();
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    const int log_fd = open(kLogPath, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd >= 0) {
      dup2(log_fd, STDERR_FILENO);
      close(log_fd);
    }
    execlp("sh", "sh", "-lc", command.str().c_str(), static_cast<char*>(nullptr));
    _exit(1);
  }

  process_group_id_ = child_pid;
  paused_ = false;
  seek_base_ms_ = 0;
  start_ms_ = NowMs();
  pause_at_ms_ = 0;
  paused_acc_ms_ = 0;
  current_track_ = track;
  return true;
}

bool PlaybackEngine::Pause() {
  if (!is_running() || paused_) return false;
  if (!SendSignalToProcessGroup(SIGSTOP)) return false;
  paused_ = true;
  pause_at_ms_ = NowMs();
  return true;
}

bool PlaybackEngine::Resume() {
  if (!is_running() || !paused_) return false;
  if (!SendSignalToProcessGroup(SIGCONT)) return false;
  paused_ = false;
  if (pause_at_ms_ > 0) paused_acc_ms_ += (NowMs() - pause_at_ms_);
  pause_at_ms_ = 0;
  return true;
}

void PlaybackEngine::Stop() {
  if (process_group_id_ > 0) {
    SendSignalToProcessGroup(SIGKILL);
    waitpid(process_group_id_, nullptr, 0);
  }
  process_group_id_ = -1;
  paused_ = false;
  seek_base_ms_ = 0;
  start_ms_ = 0;
  pause_at_ms_ = 0;
  paused_acc_ms_ = 0;
  current_track_ = Track{};
}

bool PlaybackEngine::Poll() {
  if (process_group_id_ <= 0) return false;
  int status = 0;
  const pid_t wait_result = waitpid(process_group_id_, &status, WNOHANG);
  if (wait_result == 0) return true;
  process_group_id_ = -1;
  paused_ = false;
  pause_at_ms_ = 0;
  paused_acc_ms_ = 0;
  return false;
}

bool PlaybackEngine::is_running() const {
  return process_group_id_ > 0;
}

bool PlaybackEngine::is_paused() const {
  return paused_;
}

int64_t PlaybackEngine::current_position_ms() const {
  if (!is_running()) return 0;
  const int64_t now_ms = NowMs();
  int64_t paused_ms = paused_acc_ms_;
  if (paused_ && pause_at_ms_ > 0) paused_ms += (now_ms - pause_at_ms_);
  int64_t position_ms = seek_base_ms_ + (now_ms - start_ms_ - paused_ms);
  if (position_ms < 0) position_ms = 0;
  return position_ms;
}

const AudioOutput& PlaybackEngine::output() const {
  return output_;
}

std::string PlaybackEngine::EscapeShellArg(const std::string& value) {
  std::string escaped = "'";
  for (char ch : value) {
    if (ch == '\'') escaped += "'\\''";
    else escaped += ch;
  }
  escaped += "'";
  return escaped;
}

int64_t PlaybackEngine::NowMs() {
  timespec time_spec{};
  clock_gettime(CLOCK_MONOTONIC, &time_spec);
  return static_cast<int64_t>(time_spec.tv_sec) * 1000LL + time_spec.tv_nsec / 1000000LL;
}

bool PlaybackEngine::SendSignalToProcessGroup(int signal_number) {
  if (process_group_id_ <= 0) return false;
  return kill(-process_group_id_, signal_number) == 0;
}

}  // namespace musicd
