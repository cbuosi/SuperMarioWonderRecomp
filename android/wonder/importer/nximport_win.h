// POSIX shim for the host build of the importer on Windows (MSVC / clang-cl). Include it before anything else:
// it turns off the CRT's own "nonstdc" POSIX aliases so the declarations below are the only ones.
//
// The importer is single threaded and only needs a handful of calls. Two of them must not use the CRT's
// 32-bit versions: `struct stat`/`fstat` (st_size would truncate a 3.7 GB NSP) and `off_t` (the offset of
// `pread`), so both are mapped to the _*64 variants.
#pragma once
#ifdef _WIN32

#define _CRT_DECLARE_NONSTDC_NAMES 0  // no CRT open/close/read/write/unlink/off_t/ssize_t aliases
#define _CRT_SECURE_NO_WARNINGS 1
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <windows.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>

using ssize_t = std::int64_t;
using off_t = std::int64_t;

// Turning off the nonstdc names also hides the O_* spellings; they are the _O_* ones.
#define O_RDONLY _O_RDONLY
#define O_WRONLY _O_WRONLY
#define O_RDWR _O_RDWR
#define O_CREAT _O_CREAT
#define O_TRUNC _O_TRUNC
#define O_APPEND _O_APPEND
#define O_EXCL _O_EXCL
#define O_CLOEXEC _O_NOINHERIT
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#define stat _stat64   // only used as `struct stat` and as the stat() call
#define fstat _fstat64
#define rename nxi_win_rename

inline int open(const char* path, int flags, int /*mode*/ = 0) {
    return _open(path, flags | _O_BINARY, _S_IREAD | _S_IWRITE);
}
inline int close(int fd) { return _close(fd); }
inline ssize_t read(int fd, void* buf, std::size_t n) {
    return _read(fd, buf, n > 0x7fff0000u ? 0x7fff0000u : unsigned(n));
}
inline ssize_t write(int fd, const void* buf, std::size_t n) {
    return _write(fd, buf, n > 0x7fff0000u ? 0x7fff0000u : unsigned(n));
}
inline ssize_t pread(int fd, void* buf, std::size_t n, off_t off) {
    if (_lseeki64(fd, off, SEEK_SET) < 0) return -1;
    return read(fd, buf, n);
}
inline int fdatasync(int fd) { return _commit(fd); }
inline int unlink(const char* path) { return _unlink(path); }

// `_mkdir` fails on a drive prefix ("C:") and on an existing folder with codes POSIX does not use; the caller
// only looks for EEXIST, so report that whenever the folder is already there.
inline int mkdir(const char* path, int /*mode*/) {
    if (_mkdir(path) == 0) return 0;
    DWORD a = GetFileAttributesA(path);
    if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)) {
        errno = EEXIST;
        return -1;
    }
    return -1;
}

// POSIX rename replaces the destination; the CRT's does not, and the importer renames "<file>.part" over a
// file a previous run may have left.
inline int nxi_win_rename(const char* from, const char* to) {
    if (MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING)) return 0;
    errno = GetLastError() == ERROR_FILE_NOT_FOUND ? ENOENT : EACCES;
    return -1;
}

// Only the container test uses it: a temporary folder from the template's "XXXXXX" tail.
inline char* mkdtemp(char* tmpl) {
    static const char cs[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    std::size_t n = std::strlen(tmpl);
    if (n < 6) {
        errno = EINVAL;
        return nullptr;
    }
    std::uint32_t seed = GetTickCount() ^ GetCurrentProcessId();
    for (int tries = 0; tries < 128; tries++) {
        for (std::size_t i = n - 6; i < n; i++) {
            seed = seed * 1103515245u + 12345u;
            tmpl[i] = cs[(seed >> 16) % 36u];
        }
        if (_mkdir(tmpl) == 0) return tmpl;
    }
    errno = EEXIST;
    return nullptr;
}

struct statvfs {
    std::uint64_t f_bavail = 0;
    std::uint64_t f_frsize = 1;
};
inline int statvfs(const char* dir, struct statvfs* out) {
    ULARGE_INTEGER avail{}, total{}, free_total{};
    if (!GetDiskFreeSpaceExA(dir, &avail, &total, &free_total)) return -1;
    out->f_bavail = avail.QuadPart;
    out->f_frsize = 1;
    return 0;
}

#endif  // _WIN32
