#ifndef MUSICD_AUDIO_OUTPUT_MANAGER_H_
#define MUSICD_AUDIO_OUTPUT_MANAGER_H_

#include <string>
#include <vector>

namespace musicd {

enum class AudioOutputType {
  kNone = 0,
  kAlsa = 1,
  kBluetooth = 2,
};

struct AudioOutput {
  AudioOutputType type = AudioOutputType::kNone;
  std::string name;
  std::string label;
};

struct AudioOutputSnapshot {
  std::vector<AudioOutput> outputs;
  AudioOutput preferred;
  std::string signature;
};

class AudioOutputManager {
 public:
  AudioOutputSnapshot Scan() const;

 private:
  static std::string RunCommand(const std::string& command);
  static std::string Trim(const std::string& input);
  static std::string BuildSignature(
      const std::vector<AudioOutput>& outputs,
      const std::string& hardware_state_fingerprint);
  static std::string DetectHardwareStateFingerprint();
  static std::vector<AudioOutput> DetectBluetoothOutputs();
  static std::vector<AudioOutput> DetectAlsaOutputs();
};

}  // namespace musicd

#endif  // MUSICD_AUDIO_OUTPUT_MANAGER_H_
