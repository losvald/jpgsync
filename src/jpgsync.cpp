#include "master.hpp"
#include "slave.hpp"
#include "util/dir.hpp"
#include "util/fd.hpp"
#include "util/logger.hpp"
#include "util/program_options.hpp"
#include "util/string_utils.hpp"
#include "util/syscall.hpp"

#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#define PROG "jpgsync"
#define CERR ::std::cerr << PROG ": "

bool ExtractPort(const char* begin, const char* end, uint16_t* port) {
  int ret = 0;
  while (begin != end) {
    if (!::isdigit(*begin) || (ret = 10 * ret + *begin++ - '0') > 0xFFFF)
      return false;
  }
  *port = ret;
  return true;
}

struct JpgsyncOptions : public ProgramOptions<> {
  class Root {
   public:
    friend std::ostream& operator<<(std::ostream& os, const Root& r) {
      os << "dir=" << r.dir << ", host=" << r.hostname << ", user=" <<
          r.user << std::endl;
      return os;
    }

    int Init(const std::string& root) {
      size_t pos = root.rfind(':');
      if (pos == std::string::npos) {
        dir = (!root.empty() ? root : std::string("."));
        hostname = user = "";
        return 0;
      }

      if (pos) {
        if ((dir = root.substr(pos + 1)).empty())
          dir = ".";
        hostname = root.substr(0, pos);
        if ((--pos = hostname.find('@')) == std::string::npos)
          return 1;
        if (pos && pos + 1 < hostname.size()) {
          user = hostname.substr(0, pos);
          hostname.erase(0, pos + 1);
          return 2;
        }
      }
      throw Exception("Invalid root: " + root);
    }

    std::string dir;
    std::string hostname;
    std::string user;
  };

  JpgsyncOptions()
      : master('m', "master", "run as master", this),
        slave('s', "slave", "run as slave", this),
        local('l', "local", "run as local (NOTE: for internal use)", this),
        distribute('d', "distribute",
                   "(re-)distribute the program on remote hosts", this),
        nc_test("nc-test", "test launcher/interface with netcat (nc)", this) {}

  void PrintUsage(std::ostream& os) const {
    auto options = " [options]";
    std::cerr <<
        "Usage: " PROG << " [[USER1@]HOST1:]DIR1 [[USER2@]HOST2:]DIR2" <<
        options << std::endl <<
        "  or   " PROG << " [[USER@]HOST:]DIR -m" <<
        options << std::endl <<
        "  or   " PROG << " [[USER@]HOST:]DIR -s MASTER" <<
        options << std::endl <<
        std::endl;
    ProgramOptions<>::PrintUsage(os);
  }

  const std::string& master_host() const { return master_host_; }
  uint16_t master_port() const { return master_port_; }
  const Root& master_root() const { return master_root_; }
  const Root& slave_root() const { return slave_root_; }

  Option<> master;
  Option<std::string> slave;
  Option<> local;
  Option<> distribute;
  Option<> nc_test;

 protected:
  void InitDefaults(int argc, char** argv) {
    using namespace std;

    // check not both -m and -s are specified, unless -l is also specified
    if (slave.count() && master.count())
      throw Exception("Options -m and -s are mutually exclusive.");

    // check the right number of dirs is specified by argv
    if ((slave.count() || master.count()) + (argc - 1) != 2)
      throw Exception("Invalid number of roots (need " +
                      ToString(2 - (slave.count() || master.count())) + ")");

    // parse roots and swap the args such that the remote one comes first
    size_t root_ind = 0;
    bool master_remote = true;
    if (!slave.count()) {
      master_remote = master_root_.Init(argv[++root_ind]);
      if (verbosity())
        cerr << "Master root: " << master_root_;
    }
    if (!master.count()) {
      if (slave_root_.Init(argv[++root_ind]) &&/*root_ind&&*/ !master_remote) {
        swap(master_root_, slave_root_);
        swap(argv[root_ind - 1], argv[root_ind]);
      }
      if (verbosity())
        cerr << "Slave root: " << slave_root_;
    }

    if (slave.count()) {
      size_t pos = slave().rfind(':');
      if (pos != string::npos && pos && ++pos != slave().size() &&
          ExtractPort(&*slave().begin() + pos, &*slave().end(), &master_port_))
        master_host_ = slave().substr(0, pos - 1);
      else
        throw Exception("Invalid master: " + slave());

      if (verbosity()) {
        cerr << "Master host: " << master_host_ << endl;
        cerr << "Master port: " << master_port_ << endl;
      }
    }
  }

 private:
  std::string master_host_;
  uint16_t master_port_;
  Root master_root_;
  Root slave_root_;
} gPO;

class Executor {
 public:
  virtual FILE* Execute(const char** args_begin, const char** args_end,
                        std::string* master_port) {
    SetArguments(args_begin, args_end);
    if (gPO.verbosity())
      std::cerr << cmd_ << std::endl;

    FILE* pout;
    // sys_call2(0, fflush, NULL);
    sys_call2_rv(NULL, pout, popen, cmd_.c_str(), "r");
    sys_call_rv(pout_fd_, dup, fileno(pout));

    // wait until master's port is printed to stdout and capture it
    if (master_port != NULL) {
      if (gPO.verbosity())
        std::cerr << "Capturing master's port ..." << std::endl;
      char port_str[126];
      sys_call2(NULL, fgets, port_str, sizeof(port_str), pout);
      std::istringstream iss(port_str);
      if (gPO.verbosity() > 1)
        std::cerr << "Parsing port from line: " << port_str << std::endl;
      do {
        uint16_t master_port_num;
        if ((iss >> *master_port) && ExtractPort(
                &*master_port->cbegin(), &*master_port->cend(),
                &master_port_num)) {
          break;
        }
      } while (!iss.eof());
      if (gPO.verbosity())
        std::cerr << "Captured master's port: " << *master_port << std::endl;
    }

    return pout;
  }

  int Wait(FILE* pout, int* exit_status = NULL) {
    int es;
    sys_call_rv(es, pclose, pout);
    pout_fd_.Close();
    if ((es = WEXITSTATUS(es))) {
      if (exit_status != NULL && !*exit_status)
        *exit_status = es;
      CERR << "terminated with exit status " << es << std::endl;
    }
    return es;
  }

  static Executor* Create(const JpgsyncOptions::Root& root, const char* argv0);
 protected:
  Executor() : cmd_("") {}

  virtual void SetArguments(const char** args_begin, const char** args_end) {
    while (args_begin != args_end) {
      cmd_ += ' ';
      cmd_ += *args_begin++;
    }
  }

  std::string cmd_;
  FD pout_fd_;
};

class LocalExecutor : public Executor {
  friend class Executor;
  LocalExecutor(const JpgsyncOptions::Root& root, const char* argv0)
      : Executor() {
    cmd_ += argv0;
  }
};

class RemoteExecutor : public Executor {
  friend class Executor;
  RemoteExecutor(const JpgsyncOptions::Root& root)
      : Executor(),
        dist_cmd_("") {
    const std::string prog_path = "~/bin/" PROG; // XXX hard code

    cmd_ += "ssh ";
    if (!root.user.empty()) {
      cmd_ += root.user;
      cmd_ += '@';
    }
    cmd_ += root.hostname;

    cmd_ += ' ';
    cmd_ += '\'';
    cmd_ += prog_path;

    if (gPO.distribute.count() && root.hostname != "localhost" &&
        root.hostname != "127.0.0.1") {
      dist_cmd_ = "rsync -Laz";
      dist_cmd_ += (gPO.verbosity() ? 'v' : 'q');
      dist_cmd_ += ' ';
      dist_cmd_ += prog_path;
      dist_cmd_ += ' ';
      dist_cmd_ += root.hostname + ":" + prog_path;
      dist_cmd_ += " 1>&2";
    }
  }

  void SetArguments(const char** args_begin, const char** args_end) {
    Executor::SetArguments(args_begin, args_end);
    cmd_ += '\'';
  }

  FILE* Execute(const char** args_begin, const char** args_end,
                std::string* master_port) {
    if (!dist_cmd_.empty()) {
      if (gPO.verbosity()) {
        std::cerr << "Distributing program ..." << std::endl;
        std::cerr << dist_cmd_.c_str() << std::endl;
      }
      int exit_status;
      sys_call_rv(exit_status, system, dist_cmd_.c_str());
      if ((exit_status = WEXITSTATUS(exit_status))) {
        CERR << "distribution failed with exit status " << exit_status <<
            std::endl;
      }
    }

    return Executor::Execute(args_begin, args_end, master_port);
  }

private:
  std::string dist_cmd_;
};

Executor* Executor::Create(const JpgsyncOptions::Root& root,
                           const char* argv0) {
  return root.hostname.empty() ?
      static_cast<Executor*>(new LocalExecutor(root, argv0)) :
      static_cast<Executor*>(new RemoteExecutor(root));
}

int main(int argc, char** argv) {
  using namespace std;

  int saved_argc = argc;
  try {
    gPO.Parse(&argc, argv);
  } catch (const ProgramOptions<>::Exception& e) {
    CERR << e.what() << endl;
    gPO.PrintUsage(cerr);
    return 1;
  }
  if (gPO.help()) {
    gPO.PrintUsage(cout);
    return 0;
  }

  int exit_status = 0;

  if (!gPO.local.count()) {
    vector<const char*> args(argv, argv + saved_argc);
    std::string local_arg = "--" + gPO.local.name();
    args.push_back(local_arg.c_str());

    Executor* master;
    Executor* slave;
    if (!gPO.slave.count()) {
      master = Executor::Create(gPO.master_root(), argv[0]);
    }
    if (!gPO.master.count()) {
      slave = Executor::Create(gPO.slave_root(), argv[0]);
    }

    if (!gPO.slave.count() && !gPO.master.count()) {
      // run master and capture its port
      if (gPO.verbosity())
        cerr << "Running master ..." << endl;
      swap(args[1], args[2]);
      args.push_back("-m");
      args[2] = gPO.master_root().dir.c_str();
      std::string master_port;
      auto master_pout = master->Execute(&*args.begin() + 2, &*args.end(),
                                         &master_port);
      args.pop_back();
      swap(args[1], args[2]);

      // run slave, connecting to master's port
      if (gPO.verbosity())
        cerr << "Running slave ..." << endl;
      args.push_back("-s");
      auto slave_arg = (
          gPO.master_root().hostname.empty() ?
          string("localhost") : gPO.master_root().hostname) +
          ":" + master_port;
      args.push_back(slave_arg.c_str());
      args[2] = gPO.slave_root().dir.c_str();
      slave->Wait(slave->Execute(&*args.begin() + 2, &*args.end(), NULL),
                  &exit_status);
      args.pop_back();
      delete slave;

      // await termination of the master
      master->Wait(master_pout, &exit_status);
      delete master;
    } else if (gPO.slave.count()) {
      // run slave (standalone)
      args[1] = gPO.slave_root().dir.c_str();
      slave->Wait(slave->Execute(&*args.begin() + 1, &*args.end(), NULL),
                  &exit_status);
      delete slave;
    } else if (gPO.master.count()) {
      // run master (standalone)
      args[1] = gPO.master_root().dir.c_str();
      master->Wait(master->Execute(&*args.begin() + 1, &*args.end(), NULL),
                   &exit_status);
      delete master;
    }

    if (exit_status)
      CERR << "sync failed" << endl;
    else if (gPO.verbosity())
      cerr << "sync succeeded" << endl;
    return exit_status;
  }

  if (gPO.nc_test.count()) {
    string cmd = (
        gPO.master.count() ?
        "~/bin/mync.sh -l 13579" :
        "nc " + gPO.master_host() + " " + ToString(gPO.master_port()));

    if (gPO.verbosity())
      cerr << cmd << endl;
    exit_status = system(cmd.c_str());
    // sys_call_rv(exit_status, system, cmd.c_str());
    if ((exit_status = WEXITSTATUS(exit_status)))
      CERR << "failed executing: " << cmd << endl;
    return exit_status;
  }

  Logger logger(PROG, gPO.verbosity());
  const auto& root = (gPO.master.count() ?
                      gPO.master_root().dir :
                      gPO.slave_root().dir);

  Peer* peer = NULL;
  try {
    // create a path generator for files in the root directory
    Dir dir(root);
    auto path_gen = [&] { return dir.Next().c_str(); };

    // create the corresponding peer (master / slave)
    if (gPO.master.count()) {
      auto master = new Master(&logger);
      cout << "Listening on port " << master->Listen() << endl;
      peer = master;
    } else {
      auto slave = new Slave(&logger);
      slave->Attach(gPO.master_host(), gPO.master_port());
      peer = slave;
    }

    // synchronize images
    peer->Sync(path_gen, root);
  } catch (const SysCallException& e) {
    logger.Fatal(e.what());
  }
  delete peer;

  return logger.exit_status();
}
