#pragma once
#include <deque>
#include <fcntl.h>
#include <filesystem>
#include <string>
#include <sys/wait.h>
#include <utility>
#include <variant>
#include <vector>

using namespace std;

// TODO: add docs

void sigint_handler(int);
void sigchld_handler(int);
std::string prompt();
std::vector<filesystem::path> glob(const filesystem::path &pattern);
void log(pid_t pid, int status);

namespace cmd {

class Command;
class Pipeline;
class CommandList;

// TODO: use variants for Executable and Argument to allow recursive parsing
using Executable = std::variant<Command, Pipeline, CommandList>;
using FileDescriptor = int;
using Argument = std::variant<string, Executable>;

enum class JoinMode {
  AND,  // &&
  OR,   // ||
  THEN, // ; or &
};

enum class ExecMode {
  SYNC,  // ;
  ASYNC, // &
};

enum class Builtin {
  CD,
  EXIT,
  PWD,
  NONE,
};

Builtin get_builtin(const string &cmd);

struct Redirect {
private:
  FileDescriptor src_fd;
  string dest_filename;
  FileDescriptor dest_fd;
  int _file_flags;
  bool _duplicate = true;

  enum { FD, FILENAME } type;

public:
  Redirect(FileDescriptor src_fd, string dest_filename,
           int flags = O_CREAT | O_WRONLY, bool duplicate = true)
      : src_fd(src_fd), dest_filename(std::move(dest_filename)), dest_fd(0),
        _file_flags(flags), _duplicate(duplicate), type(FILENAME) {}

  Redirect(FileDescriptor src_fd, const FileDescriptor dest_fd,
           bool duplicate = true)
      : src_fd(src_fd), dest_fd(dest_fd), _file_flags(0), _duplicate(duplicate),
        type(FD) {}

  void apply() const;
};

class Command {
private:
  string _cmd;
  vector<string> _args;
  deque<Redirect> _redirects;
  bool _append = false;

  [[nodiscard]] int builtin() const;

public:
  void push_arg(const string &arg) {
    // globbing:
    auto paths = glob(arg);
    if (!paths.empty()) {
      for (const auto &path : paths)
        _args.push_back(path.string());
      return;
    }
    _args.push_back(arg);
  }
  void set_cmd(const string &cmd) { _cmd = cmd; }

  void push_redirect(Redirect &&redirect) { _redirects.push_back(redirect); }
  void push_front_redirect(Redirect &&redirect) {
    _redirects.push_front(redirect);
  }

  [[nodiscard]] int execute(ExecMode exec_mode = ExecMode::SYNC) const;

  friend std::ostream &operator<<(std::ostream &os, const Command &cmd);
};

class Pipeline {
  vector<Command> _pipeline;
  ExecMode _exec_mode = ExecMode::SYNC;

public:
  void set_async() { _exec_mode = ExecMode::ASYNC; }
  void push_back(const Command &cmd) { _pipeline.push_back(cmd); }
  int execute();

  friend std::ostream &operator<<(std::ostream &os, const Pipeline &pipe);
};

class CommandList {
  vector<Pipeline> _commands;
  vector<JoinMode> _joins;

public:
  CommandList() = default;
  explicit CommandList(const Pipeline &pipe) { _commands.push_back(pipe); }

  void push_back(const Pipeline &cmd, JoinMode mode) {
    _commands.push_back(cmd);
    _joins.push_back(mode);
  }

  void set_last_async() { _commands.back().set_async(); }
  int execute();
  friend std::ostream &operator<<(std::ostream &os, const CommandList &cmd);
};

} // namespace cmd
