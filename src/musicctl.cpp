#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr const char* kDefaultSocketPath = "/tmp/musicd.sock";

void PrintUsage() {
  std::cerr
      << "usage: musicctl <state|outputs|play URL|enqueue URL|pause|resume|stop|next|set-output NAME|card ...|quit>"
      << std::endl;
  std::cerr << "  card list" << std::endl;
  std::cerr << "  card <index>" << std::endl;
  std::cerr << "  card <speaker|analog|digital|bt|bluetooth|NAME>" << std::endl;
}

std::string JoinArgs(int argc, char** argv, int begin) {
  std::string joined;
  for (int i = begin; i < argc; ++i) {
    if (!joined.empty()) joined += " ";
    joined += argv[i];
  }
  return joined;
}

std::string ToLowerAscii(const std::string& value) {
  std::string lowered = value;
  for (char& ch : lowered) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return lowered;
}

std::string BuildPayload(int argc, char** argv, bool* is_card_command, std::string* card_argument) {
  if (argc < 2) return "";
  std::string command = ToLowerAscii(argv[1]);

  if (command == "state") return "GET_STATE\n";
  if (command == "outputs") return "LIST_OUTPUTS\n";
  if (command == "pause") return "PAUSE\n";
  if (command == "resume") return "RESUME\n";
  if (command == "stop") return "STOP\n";
  if (command == "next") return "NEXT\n";
  if (command == "quit") return "QUIT\n";
  if (command == "play") {
    if (argc < 3) return "";
    return "PLAY " + JoinArgs(argc, argv, 2) + "\n";
  }
  if (command == "enqueue") {
    if (argc < 3) return "";
    return "ENQUEUE " + JoinArgs(argc, argv, 2) + "\n";
  }
  if (command == "set-output") {
    if (argc < 3) return "";
    return "SET_OUTPUT " + JoinArgs(argc, argv, 2) + "\n";
  }
  if (command == "card") {
    if (argc < 3) return "";
    *is_card_command = true;
    *card_argument = JoinArgs(argc, argv, 2);
    return "";
  }
  return "";
}

bool SendRequest(const std::string& socket_path, const std::string& payload, std::string* response) {
  response->clear();

  const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    std::cerr << "socket() failed: " << std::strerror(errno) << std::endl;
    return false;
  }

  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  if (socket_path.size() >= sizeof(address.sun_path)) {
    std::cerr << "socket path too long: " << socket_path << std::endl;
    close(fd);
    return false;
  }
  std::strncpy(address.sun_path, socket_path.c_str(), sizeof(address.sun_path) - 1);

  if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    std::cerr << "connect() failed: " << std::strerror(errno) << " (" << socket_path << ")" << std::endl;
    close(fd);
    return false;
  }

  const char* data = payload.c_str();
  std::size_t written_total = 0;
  while (written_total < payload.size()) {
    const ssize_t written = write(fd, data + written_total, payload.size() - written_total);
    if (written < 0) {
      if (errno == EINTR) continue;
      std::cerr << "write() failed: " << std::strerror(errno) << std::endl;
      close(fd);
      return false;
    }
    written_total += static_cast<std::size_t>(written);
  }
  shutdown(fd, SHUT_WR);

  char buffer[4096];
  while (true) {
    const ssize_t read_size = read(fd, buffer, sizeof(buffer));
    if (read_size > 0) {
      response->append(buffer, static_cast<std::size_t>(read_size));
      continue;
    }
    if (read_size == 0) break;
    if (errno == EINTR) continue;
    std::cerr << "read() failed: " << std::strerror(errno) << std::endl;
    close(fd);
    return false;
  }

  close(fd);
  return true;
}

std::vector<std::pair<std::string, std::string>> ParseOutputs(const std::string& raw) {
  std::vector<std::pair<std::string, std::string>> outputs;
  std::istringstream stream(raw);
  std::string line;
  while (std::getline(stream, line)) {
    if (line.empty()) continue;
    const std::size_t sep = line.find('|');
    if (sep == std::string::npos) continue;
    outputs.emplace_back(line.substr(0, sep), line.substr(sep + 1));
  }
  return outputs;
}

bool HandleCardCommand(const std::string& socket_path, const std::string& arg) {
  const std::string card_arg = ToLowerAscii(arg);

  std::string outputs_raw;
  if (!SendRequest(socket_path, "LIST_OUTPUTS\n", &outputs_raw)) return false;
  const auto outputs = ParseOutputs(outputs_raw);

  if (card_arg == "list") {
    if (outputs.empty()) {
      std::cout << "no outputs" << std::endl;
      return true;
    }
    for (std::size_t i = 0; i < outputs.size(); ++i) {
      std::cout << i << ": " << outputs[i].first << " | " << outputs[i].second << std::endl;
    }
    return true;
  }

  std::string selected_name;
  if (card_arg == "speaker") selected_name = "speaker";
  else if (card_arg == "analog") selected_name = "tc_analog";
  else if (card_arg == "digital") selected_name = "tc_digital";
  else if (card_arg == "bt" || card_arg == "bluetooth") {
    for (const auto& output : outputs) {
      if (ToLowerAscii(output.first).rfind("bluealsa:", 0) == 0) {
        selected_name = output.first;
        break;
      }
    }
    if (selected_name.empty()) {
      std::cerr << "no bluetooth output found. use `musicctl card list` first." << std::endl;
      return false;
    }
  }
  else {
    bool numeric = !card_arg.empty();
    for (char ch : card_arg) {
      if (!std::isdigit(static_cast<unsigned char>(ch))) {
        numeric = false;
        break;
      }
    }

    if (numeric) {
      const long index = std::strtol(card_arg.c_str(), nullptr, 10);
      if (index < 0 || static_cast<std::size_t>(index) >= outputs.size()) {
        std::cerr << "invalid card index: " << arg << std::endl;
        return false;
      }
      selected_name = outputs[static_cast<std::size_t>(index)].first;
    } else {
      selected_name = arg;
    }
  }

  std::string response;
  if (!SendRequest(socket_path, "SET_OUTPUT " + selected_name + "\n", &response)) return false;
  std::cout << response;
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  const char* env_socket = std::getenv("MUSICD_SOCKET");
  const std::string socket_path = (env_socket != nullptr && env_socket[0] != '\0')
      ? std::string(env_socket)
      : std::string(kDefaultSocketPath);

  bool is_card_command = false;
  std::string card_argument;
  const std::string payload = BuildPayload(argc, argv, &is_card_command, &card_argument);
  if (is_card_command) {
    return HandleCardCommand(socket_path, card_argument) ? 0 : 1;
  }
  if (payload.empty()) {
    PrintUsage();
    return 1;
  }

  std::string response;
  if (!SendRequest(socket_path, payload, &response)) return 1;
  std::cout << response;
  return 0;
}
