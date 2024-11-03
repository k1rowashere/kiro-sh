#pragma once
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

using namespace std;

// TODO: add docs

namespace cmd {
class SimpleCommand;
class Pipeline;
class CommandList;

// TODO: use variants for Executable and Argument to allow recursive parsing
using Executable = std::variant<SimpleCommand, Pipeline, CommandList>;
using FileDescr = int;
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
  FileDescr src_fd;
  string dest_filename;
  FileDescr dest_fd;
  int _file_flags;
  bool _duplicate = true;

  enum { FD, FILENAME } type;

public:
  Redirect(FileDescr src_fd, string dest_filename,
           int flags = O_CREAT | O_WRONLY, bool duplicate = true)
      : src_fd(src_fd), dest_filename(dest_filename), _file_flags(flags),
        _duplicate(duplicate), type(FILENAME) {}

  Redirect(FileDescr src_fd, FileDescr dest_fd, bool duplicate = true)
      : src_fd(src_fd), dest_fd(dest_fd), _duplicate(duplicate), type(FD) {}

  void apply();
};

struct SimpleCommand {
private:
  string _cmd;
  vector<string> _args;
  vector<Redirect> _redirs;
  bool _append = false;

  int builtin();

public:
  void push_arg(const string &arg) { _args.push_back(arg); }
  void set_cmd(const string &cmd) { _cmd = cmd; }

  void push_redirect(Redirect &&redirect) { _redirs.push_back(redirect); }

  int execute(ExecMode exec_mode = ExecMode::SYNC);

  friend std::ostream &operator<<(std::ostream &os, const SimpleCommand &cmd);
};

class Pipeline {
  vector<SimpleCommand> _pipeline;
  ExecMode _exec_mode = ExecMode::SYNC;

public:
  void set_async() { _exec_mode = ExecMode::ASYNC; }
  void push_back(const SimpleCommand &cmd) { _pipeline.push_back(cmd); }
  int execute();

  friend std::ostream &operator<<(std::ostream &os, const Pipeline &pipe);
};

class CommandList {
  vector<Pipeline> _commands;
  vector<JoinMode> _joins;

public:
  CommandList() {};
  CommandList(Pipeline pipe) { _commands.push_back(pipe); }

  void push_back(const Pipeline &cmd, JoinMode mode) {
    _commands.push_back(cmd);
    _joins.push_back(mode);
  }

  void set_last_async() { _commands.back().set_async(); }
  int execute();
  friend std::ostream &operator<<(std::ostream &os, const CommandList &cmd);
};

vector<filesystem::path> glob(string pattern);

} // namespace cmd
