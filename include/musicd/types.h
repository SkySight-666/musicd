#ifndef MUSICD_TYPES_H_
#define MUSICD_TYPES_H_

#include <string>
#include <vector>

namespace musicd {

struct Track {
  std::string id;
  std::string title;
  std::string artist;
  std::string album;
  std::string source_url;
  std::string lyric_url;
  long duration_ms = 0;
};

struct PlayerState {
  bool playing = false;
  bool paused = false;
  bool paused_by_output_change = false;
  long position_ms = 0;
  int queue_index = -1;
  std::string audio_output_name;
  std::string audio_output_label;
  Track current_track;
};

using TrackList = std::vector<Track>;

}  // namespace musicd

#endif  // MUSICD_TYPES_H_
