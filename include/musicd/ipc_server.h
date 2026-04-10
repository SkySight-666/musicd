#ifndef MUSICD_IPC_SERVER_H_
#define MUSICD_IPC_SERVER_H_

#include <functional>
#include <string>

namespace musicd {

class IpcServer {
 public:
  IpcServer();
  ~IpcServer();

  bool Start(const std::string& socket_path);
  void Stop();
  bool running() const;
  void PollOnce(const std::function<std::string(const std::string&)>& handler);

 private:
  int server_fd_;
  std::string socket_path_;
  bool running_;
};

}  // namespace musicd

#endif  // MUSICD_IPC_SERVER_H_
