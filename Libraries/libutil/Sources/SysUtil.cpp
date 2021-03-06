/**
 Copyright (c) 2015-present, Facebook, Inc.
 All rights reserved.

 This source code is licensed under the BSD-style license found in the
 LICENSE file in the root directory of this source tree. An additional grant
 of patent rights can be found in the PATENTS file in the same directory.
 */

#include <libutil/SysUtil.h>
#include <libutil/FSUtil.h>

#include <sstream>
#include <unordered_set>
#include <cstring>
#include <cassert>

#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <linux/limits.h>
#if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 16
#include <sys/auxv.h>
#endif
#endif

extern "C" char **environ;

using libutil::SysUtil;
using libutil::FSUtil;

std::string SysUtil::
GetCurrentDirectory()
{
    char path[PATH_MAX + 1];
    if (::getcwd(path, sizeof(path)) == nullptr) {
        path[0] = '\0';
    }
    return path;
}

#if defined(__linux__)
static char initialWorkingDirectory[PATH_MAX] = { 0 };
__attribute__((constructor))
static void InitializeInitialWorkingDirectory()
{
    if (getcwd(initialWorkingDirectory, sizeof(initialWorkingDirectory)) == NULL) {
        abort();
    }
}

#if !(__GLIBC__ >= 2 && __GLIBC_MINOR__ >= 16)
static char initialExecutablePath[PATH_MAX] = { 0 };
__attribute__((constructor))
static void InitialExecutablePathInitialize(int argc, char **argv)
{
    strncpy(initialExecutablePath, argv[0], sizeof(initialExecutablePath));
}
#endif
#endif

std::string SysUtil::
GetExecutablePath()
{
#if defined(__APPLE__)
    uint32_t size = 0;
    if (_NSGetExecutablePath(NULL, &size) != -1) {
        abort();
    }

    std::string buffer;
    buffer.resize(size);
    if (_NSGetExecutablePath(&buffer[0], &size) != 0) {
        abort();
    }

    return FSUtil::NormalizePath(buffer);
#elif defined(__linux__)
#if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 16
    char const *path = reinterpret_cast<char const *>(getauxval(AT_EXECFN));
    if (path == NULL) {
        assert(false);
        abort();
    }
#elif defined(__GLIBC__)
    char const *path = reinterpret_cast<char const *>(initialExecutablePath);
#else
#error Unsupported platform.
#endif

    std::string absolutePath = FSUtil::ResolveRelativePath(std::string(path), std::string(initialWorkingDirectory));
    return FSUtil::NormalizePath(absolutePath);
#else
#error Unsupported platform.
#endif
}

ext::optional<std::string> SysUtil::
GetEnvironmentVariable(std::string const &variable)
{
    if (char *value = getenv(variable.c_str())) {
        return std::string(value);
    } else {
        return ext::nullopt;
    }
}

std::unordered_map<std::string, std::string> SysUtil::
GetEnvironmentVariables()
{
    std::unordered_map<std::string, std::string> environment;

    for (char **current = environ; *current; current++) {
        std::string variable = *current;
        std::string::size_type offset = variable.find('=');

        std::string name = variable.substr(0, offset);
        std::string value = variable.substr(offset + 1);
        environment.insert(std::make_pair(name, value));
    }

    return environment;
}

std::vector<std::string> SysUtil::
GetExecutablePaths()
{
    std::vector<std::string>        vpaths;
    std::unordered_set<std::string> seen;
    std::string                     path;
    std::istringstream              is(::getenv("PATH"));

    while (std::getline(is, path, ':')) {
        if (seen.find(path) != seen.end()) {
            continue;
        }

        vpaths.push_back(path);
        seen.insert(path);
    }

    return vpaths;
}

std::string SysUtil::
GetUserName()
{
    std::string result;

    struct passwd const *pw = ::getpwuid(::getuid());
    if (pw != nullptr) {
        if (pw->pw_name != nullptr) {
            result = pw->pw_name;
        }
    }
    if (result.empty()) {
        std::ostringstream os;
        os << ::getuid();
        result = os.str();
    }

    ::endpwent();

    return result;
}

std::string SysUtil::
GetGroupName()
{
    std::string result;

    struct group const *gr = ::getgrgid(::getgid());
    if (gr != nullptr) {
        if (gr->gr_name != nullptr) {
            result = gr->gr_name;
        }
    }
    if (result.empty()) {
        std::ostringstream os;
        os << ::getgid();
        result = os.str();
    }

    ::endgrent();

    return result;
}

int32_t SysUtil::
GetUserID()
{
    return ::getuid();
}

int32_t SysUtil::
GetGroupID()
{
    return ::getgid();
}

