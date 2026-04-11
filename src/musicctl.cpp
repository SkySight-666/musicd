#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr const char* kDefaultSocketPath = "/tmp/musicd.sock";

void PrintUsage() {
  std::cerr
      << "usage: musicctl <state|outputs|play URL|enqueue URL|pause|resume|stop|next|set-output NAME|quit>"
      << std::endl;
}

std::string BuildPayload(int argc, char** argv) {
  if (argc < 2) return "";
  const std::string command = argv[1];
  if (command == "state") return "GET_STATE\n";
  if (command == "outputs") return "LIST_OUTPUTS\n";
  if (command == "pause") return "PAUSE\n";
  if (command == "resume") return "RESUME\n";
  if (command == "stop") return "STOP\n";
  if (command == "next") return "NEXT\n";
  if (command == "quit") return "QUIT\n";
  if (command == "play") {
    if (argc < 3) return "";
    return "PLAY " + std::string(argv[2]) + "\n";
  }
  if (command == "enqueue") {
    if (argc < 3) return "";
    return "ENQUEUE " + std::string(argv[2]) + "\n";
  }
  if (command == "set-output") {
    if (argc < 3) return "";
    return "SET_OUTPUT " + std::string(argv[2]) + "\n";
  }
  return "";
}

}  // namespace

int main(int argc, char** argv) {
  const std::string payload = BuildPayload(argc, argv);
  if (payload.empty()) {
    PrintUsage();
    return 1;
  }

  const char* env_socket = std::getenv("MUSICD_SOCKET");
  const std::string socket_path = (env_socket != nullptr && env_socket[0] != '\0')
      ? std::string(env_socket)
      : std::string(kDefaultSocketPath);

  const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    std::cerr << "socket() failed: " << std::strerror(errno) << std::endl;
    return 1;
  }

  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  if (socket_path.size() >= sizeof(address.sun_path)) {
    std::cerr << "socket path too long: " << socket_path << std::endl;
    close(fd);
    return 1;
  }
  std::strncpy(address.sun_path, socket_path.c_str(), sizeof(address.sun_path) - 1);

  if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    std::cerr << "connect() failed: " << std::strerror(errno) << " (" << socket_path << ")" << std::endl;
    close(fd);
    return 1;
  }

  const char* data = payload.c_str();
  std::size_t written_total = 0;
  while (written_total < payload.size()) {
    const ssize_t written = write(fd, data + written_total, payload.size() - written_total);
    if (written < 0) {
      if (errno == EINTR) continue;
      std::cerr << "write() failed: " << std::strerror(errno) << std::endl;
      close(fd);
      return 1;
    }
    written_total += static_cast<std::size_t>(written);
  }
  shutdown(fd, SHUT_WR);

  char buffer[4096];
  while (true) {
    const ssize_t read_size = read(fd, buffer, sizeof(buffer));
    if (read_size > 0) {
      std::cout.write(buffer, read_size);
      continue;
    }
    if (read_size == 0) break;
    if (errno == EINTR) continue;
    std::cerr << "read() failed: " << std::strerror(errno) << std::endl;
    close(fd);
    return 1;
  }

  close(fd);
  return 0;
}
