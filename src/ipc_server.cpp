#include "musicd/ipc_server.h"

#include <cerrno>
#include <cstring>
#include <functional>
#include <string>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace musicd {

namespace {

constexpr std::size_t kBufferSize = 4096;

bool SetNonBlocking(int fd) {
  const int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) return false;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

}  // namespace

IpcServer::IpcServer() : server_fd_(-1), running_(false) {}

IpcServer::~IpcServer() {
  Stop();
}

bool IpcServer::Start(const std::string& socket_path) {
  Stop();

  server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd_ < 0) return false;
  if (!SetNonBlocking(server_fd_)) {
    Stop();
    return false;
  }

  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  socket_path_ = socket_path;
  if (socket_path_.size() >= sizeof(address.sun_path)) {
    Stop();
    return false;
  }

  std::strncpy(address.sun_path, socket_path_.c_str(), sizeof(address.sun_path) - 1);
  unlink(socket_path_.c_str());
  if (bind(server_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    Stop();
    return false;
  }

  if (listen(server_fd_, 8) != 0) {
    Stop();
    return false;
  }

  running_ = true;
  return true;
}

void IpcServer::Stop() {
  running_ = false;
  if (server_fd_ >= 0) {
    close(server_fd_);
    server_fd_ = -1;
  }
  if (!socket_path_.empty()) {
    unlink(socket_path_.c_str());
    socket_path_.clear();
  }
}

bool IpcServer::running() const {
  return running_;
}

void IpcServer::PollOnce(const std::function<std::string(const std::string&)>& handler) {
  if (!running_ || server_fd_ < 0) return;

  while (true) {
    const int client_fd = accept(server_fd_, nullptr, nullptr);
    if (client_fd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) break;
      return;
    }

    std::string request;
    char buffer[kBufferSize];
    while (true) {
      const ssize_t read_size = read(client_fd, buffer, sizeof(buffer));
      if (read_size > 0) {
        request.append(buffer, static_cast<std::size_t>(read_size));
        if (request.find('\n') != std::string::npos) break;
        continue;
      }
      if (read_size == 0) break;
      if (errno == EINTR) continue;
      break;
    }

    const std::string response = handler(request);
    if (!response.empty()) {
      ssize_t written_total = 0;
      while (written_total < static_cast<ssize_t>(response.size())) {
        const ssize_t written = write(
            client_fd,
            response.data() + written_total,
            response.size() - static_cast<std::size_t>(written_total));
        if (written <= 0) break;
        written_total += written;
      }
    }
    close(client_fd);
  }
}

}  // namespace musicd
