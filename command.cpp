#include "command.hpp"
#include "driver.hpp"
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sys/wait.h>
#include <unordered_map>

// handle SIGINT
void sigint_handler([[maybe_unused]] int sig) { std::cout << std::endl; }
void sigchld_handler(int sig) {
  // handle SIGCHLD ( print process pid, status, and exit code)

  auto time =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  auto file = std::ofstream("log.txt", ios::app);
  file << time << "Child Process exited with status " << WEXITSTATUS(sig)
       << std::endl;
  file.close();
}

[[noreturn]] int main() {
  signal(SIGINT, sigint_handler);
  signal(SIGCHLD, sigchld_handler);
  while (true) {
    auto driver = cmd::driver();
    std::cout << "kirosh> ";
    driver.parse();
  }
}

namespace cmd {
using pipe_t = int;

vector<filesystem::path> glob(string pattern) {
  // * --> [^/]*
  // ** --> .*
  // ? --> [^/]
  // [.*] --> [.*]

  // get all files in the directory
  // check if the file matches the pattern
  // if it does, add it to the list

  filesystem::path path = pattern;
  filesystem::path dir = path.parent_path();
  return {};
}

int driver::parse() {
  auto parser = yy::parser(*this);
  return parser.parse();
}

void Redirect::apply() {
  switch (type) {
  case FD:
    if (_duplicate)
      dup2(dest_fd, src_fd);
    else {
      dup2(dest_fd, src_fd);
      close(dest_fd);
    }
    break;
  case FILENAME:
    int fd = open(dest_filename.c_str(), _file_flags, 0644);
    if (fd == -1) {
      std::cerr << "open";
      exit(1);
    }
    dup2(fd, src_fd);
    close(fd);
    break;
  }
}

Builtin get_builtin(const string &cmd) {
  auto builtins = std::unordered_map<string, Builtin>{
      {"cd", Builtin::CD},
      {"exit", Builtin::EXIT},
      {"pwd", Builtin::PWD},
  };

  if (builtins.find(cmd) != builtins.end()) return builtins[cmd];
  return Builtin::NONE;
}

int SimpleCommand::builtin() {
  // TODO: handle redirections for built-in commands
  switch (get_builtin(_cmd)) {
  case Builtin::CD:
    if (_args.empty()) {
      std::cerr << "cd: missing argument" << std::endl;
      return 1;
    }
    if (chdir(_args[0].c_str()) == -1) {
      std::cerr << "cd: " << _args[0] << ": No such file or directory"
                << std::endl;
      return 1;
    }
    return 0;
  case Builtin::EXIT:
    exit(0);
  case Builtin::PWD:
    std::cout << filesystem::current_path() << std::endl;
    return 0;
  default:
    return -1;
  }
}

int SimpleCommand::execute(ExecMode exec_mode) {
  // handle built-in commands
  int status = this->builtin();
  if (status != -1) return status;

  // Create argv (command + args + nullptr)
  std::vector<char *> argv;
  argv.push_back(const_cast<char *>(_cmd.c_str()));

  for (const auto &arg : _args)
    argv.push_back(const_cast<char *>(arg.c_str()));

  argv.push_back(nullptr);

  // Execute command
  pid_t pid = fork();

  if (pid == 0) {
    // child process

    // redirect I/O
    for (auto &redirect : _redirs)
      redirect.apply();

    // Execute command
    execvp(argv[0], argv.data());

    // execvp failed (command probably not found)
    std::cerr << "Command not found: " << argv[0] << std::endl;
    exit(1);
  } else if (pid > 0) {

    // parent process
    if (exec_mode == ExecMode::ASYNC) return 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) status = WEXITSTATUS(status);

  } else {
    // fork failed
    std::cerr << "fork";
    status = 1;
  }

  return status;
}

int Pipeline::execute() {
  if (_pipeline.empty()) return 0;
  if (_pipeline.size() == 1) return _pipeline.front().execute(_exec_mode);
  std::cout << "here";

  int status = 0;
  int file_desc[2]; // file descriptors for pipe [0] read, [1] write
  int prev_file_desc = -1;

  // execute all commands in pipeline except the last one
  for (auto i = _pipeline.begin(); i != _pipeline.end() - 1; ++i) {
    // create pipe
    if (pipe(file_desc) == -1) {
      std::cerr << "pipe";
      exit(1);
    }

    if (i != _pipeline.begin())
      i->push_redirect(Redirect(STDIN_FILENO, prev_file_desc));

    i->push_redirect(Redirect(STDOUT_FILENO, file_desc[1]));
    prev_file_desc = file_desc[0];

    i->execute(ExecMode::ASYNC);

    // close write end of pipe
    // read end will be closed by the next command
    close(file_desc[1]);
  }
  // execute last command in pipeline
  _pipeline.back().push_redirect(Redirect(STDIN_FILENO, prev_file_desc));
  _pipeline.back().execute(_exec_mode);

  // wait for all commands to finish
  if (_exec_mode == ExecMode::SYNC) {
    for (size_t i = 0; i < _pipeline.size(); ++i) {
      int status;
      waitpid(-1, &status, 0);
      if (WIFEXITED(status)) status = WEXITSTATUS(status);
    }
  }

  return status;
}

int CommandList::execute() {
  int status = 0;
  for (size_t i = 0; i < _commands.size(); ++i) {
    status = _commands[i].execute();

    if (i == _commands.size() - 1) break; // last command isn't joined
    auto join_mode = _joins[i];
    if (join_mode == JoinMode::AND && status != 0) break;
    if (join_mode == JoinMode::OR && status == 0) break;
  }

  return status;
}

// For debug printing
std::ostream &operator<<(std::ostream &os, const SimpleCommand &cmd) {
  os << cmd._cmd;
  for (const auto &arg : cmd._args)
    os << " '" << arg << "'";
  return os;
}

std::ostream &operator<<(std::ostream &os, const Pipeline &pipe) {
  if (pipe._pipeline.empty()) return os;

  os << pipe._pipeline[0];
  for (auto i = pipe._pipeline.begin() + 1; i != pipe._pipeline.end(); ++i)
    os << "|" << (pipe._exec_mode == ExecMode::ASYNC ? " &" : " ;");
  return os;
}

std::ostream &operator<<(std::ostream &os, const CommandList &cmd) {
  if (cmd._commands.empty()) return os;
  os << cmd._commands[0];
  for (auto i = cmd._commands.begin() + 1; i != cmd._commands.end(); ++i) {
    const auto &pipeline = *i;
    os << ", " << pipeline;
  }
  return os;
}

} // namespace cmd
