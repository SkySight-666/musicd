#include "musicd/audio_output_manager.h"

#include <array>
#include <cstdio>
#include <sstream>
#include <unistd.h>

namespace musicd {

std::string AudioOutputManager::RunCommand(const std::string& command) {
  std::array<char, 512> buffer{};
  std::string output;
  FILE* pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) return output;
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    output += buffer.data();
  }
  pclose(pipe);
  return output;
}

std::string AudioOutputManager::Trim(const std::string& input) {
  const std::string whitespace = " \t\r\n";
  const std::size_t begin = input.find_first_not_of(whitespace);
  if (begin == std::string::npos) return "";
  const std::size_t end = input.find_last_not_of(whitespace);
  return input.substr(begin, end - begin + 1);
}

std::string AudioOutputManager::BuildSignature(
    const std::vector<AudioOutput>& outputs,
    const std::string& hardware_state_fingerprint) {
  std::ostringstream builder;
  for (const AudioOutput& output : outputs) {
    builder << static_cast<int>(output.type) << ":" << output.name << ";";
  }
  builder << "#hw:" << hardware_state_fingerprint;
  return builder.str();
}

std::string AudioOutputManager::DetectHardwareStateFingerprint() {
  // Analog Type-C detect path from vendor logs: gpio-463 state in debug gpio dump.
  std::string gpio_line = Trim(RunCommand("cat /sys/kernel/debug/gpio 2>/dev/null | grep gpio-463 | head -1"));
  std::string gpio_state = "na";
  if (!gpio_line.empty()) {
    if (gpio_line.find(" hi") != std::string::npos || gpio_line.find("\thi") != std::string::npos) gpio_state = "hi";
    else if (gpio_line.find(" lo") != std::string::npos || gpio_line.find("\tlo") != std::string::npos) gpio_state = "lo";
    else gpio_state = gpio_line;
  }

  // Digital Type-C detect path from vendor logs: card4 existence.
  const char* card4_path = "/sys/class/sound/card4";
  const std::string card4_state = (access(card4_path, F_OK) == 0) ? "1" : "0";

  // Legacy h2w detect path.
  const std::string h2w_state = Trim(RunCommand("cat /sys/class/switch/h2w/state 2>/dev/null"));

  std::ostringstream fingerprint;
  fingerprint
      << "gpio463=" << gpio_state
      << ";card4=" << card4_state
      << ";h2w=" << (h2w_state.empty() ? "na" : h2w_state)
      << ";";
  return fingerprint.str();
}

std::vector<AudioOutput> AudioOutputManager::DetectBluetoothOutputs() {
  std::vector<AudioOutput> outputs;
  const std::string raw = RunCommand("bluealsa-aplay -L 2>/dev/null");
  std::istringstream stream(raw);
  std::string line;
  while (std::getline(stream, line)) {
    line = Trim(line);
    if (line.rfind("bluealsa:", 0) != 0) continue;
    if (line.find("a2dp") == std::string::npos) continue;

    AudioOutput output;
    output.type = AudioOutputType::kBluetooth;
    output.name = line;
    output.label = "Bluetooth A2DP";

    const std::size_t dev_pos = line.find("DEV=");
    const std::size_t comma_pos = line.find(",", dev_pos);
    if (dev_pos != std::string::npos && comma_pos != std::string::npos && comma_pos > dev_pos + 4) {
      output.label += ": " + line.substr(dev_pos + 4, comma_pos - dev_pos - 4);
    }
    outputs.push_back(output);
  }
  return outputs;
}

std::vector<AudioOutput> AudioOutputManager::DetectAlsaOutputs() {
  std::vector<AudioOutput> outputs;
  const std::string raw = RunCommand("aplay -l 2>/dev/null | grep '^card' | head -4");
  std::istringstream stream(raw);
  std::string line;
  while (std::getline(stream, line)) {
    line = Trim(line);
    if (line.empty()) continue;
    if (line.find("Loopback") != std::string::npos) continue;

    int card_number = -1;
    int device_number = 0;
    if (std::sscanf(line.c_str(), "card %d:", &card_number) != 1) continue;

    const std::size_t device_pos = line.find("device ");
    if (device_pos != std::string::npos) {
      std::sscanf(line.c_str() + device_pos, "device %d:", &device_number);
    }

    AudioOutput output;
    output.type = AudioOutputType::kAlsa;
    output.name = "hw:" + std::to_string(card_number) + "," + std::to_string(device_number);
    output.label = "ALSA: " + line;
    outputs.push_back(output);
  }

  AudioOutput fallback;
  fallback.type = AudioOutputType::kAlsa;
  fallback.name = "default";
  fallback.label = "ALSA: default";
  outputs.push_back(fallback);
  return outputs;
}

AudioOutputSnapshot AudioOutputManager::Scan() const {
  AudioOutputSnapshot snapshot;

  std::vector<AudioOutput> bluetooth_outputs = DetectBluetoothOutputs();
  std::vector<AudioOutput> alsa_outputs = DetectAlsaOutputs();

  snapshot.outputs.insert(snapshot.outputs.end(), bluetooth_outputs.begin(), bluetooth_outputs.end());
  snapshot.outputs.insert(snapshot.outputs.end(), alsa_outputs.begin(), alsa_outputs.end());

  if (!bluetooth_outputs.empty()) snapshot.preferred = bluetooth_outputs.front();
  else if (!alsa_outputs.empty()) snapshot.preferred = alsa_outputs.front();

  snapshot.signature = BuildSignature(snapshot.outputs, DetectHardwareStateFingerprint());
  return snapshot;
}

}  // namespace musicd
