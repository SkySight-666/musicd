#include "musicd/music_daemon.h"

#include <string>

int main(int argc, char** argv) {
  musicd::MusicDaemon daemon;
  if (argc >= 3 && std::string(argv[1]) == "play") {
    daemon.SetStartupTrackUrl(argv[2]);
  }
  return daemon.Run();
}
