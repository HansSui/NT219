// Read-only file mapping used by the benchmark's mmap path.
#pragma once

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace io {

class MappedFile {
public:
    explicit MappedFile(const std::string& path) {
#ifdef _WIN32
        const std::wstring wide = std::filesystem::u8path(path).wstring();
        file_ = CreateFileW(wide.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file_ == INVALID_HANDLE_VALUE) throw std::runtime_error("cannot open file: " + path);
        LARGE_INTEGER size{};
        if (!GetFileSizeEx(file_, &size)) {
            CloseHandle(file_);
            throw std::runtime_error("cannot stat file: " + path);
        }
        size_ = static_cast<std::size_t>(size.QuadPart);
        if (size_ == 0) return;
        mapping_ = CreateFileMappingW(file_, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!mapping_) {
            CloseHandle(file_);
            throw std::runtime_error("cannot map file: " + path);
        }
        data_ = static_cast<const char*>(MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, 0));
        if (!data_) {
            CloseHandle(mapping_);
            CloseHandle(file_);
            throw std::runtime_error("cannot map file: " + path);
        }
#else
        fd_ = ::open(path.c_str(), O_RDONLY);
        if (fd_ < 0) throw std::runtime_error("cannot open file: " + path);
        struct stat st {};
        if (fstat(fd_, &st) != 0) {
            ::close(fd_);
            throw std::runtime_error("cannot stat file: " + path);
        }
        size_ = static_cast<std::size_t>(st.st_size);
        if (size_ == 0) return;
        void* mapped = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
        if (mapped == MAP_FAILED) {
            ::close(fd_);
            throw std::runtime_error("cannot map file: " + path);
        }
        data_ = static_cast<const char*>(mapped);
#endif
    }

    ~MappedFile() {
#ifdef _WIN32
        if (data_) UnmapViewOfFile(data_);
        if (mapping_) CloseHandle(mapping_);
        if (file_ != INVALID_HANDLE_VALUE) CloseHandle(file_);
#else
        if (data_) munmap(const_cast<char*>(data_), size_);
        if (fd_ >= 0) ::close(fd_);
#endif
    }

    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    const char* data() const { return data_; }
    std::size_t size() const { return size_; }

private:
#ifdef _WIN32
    HANDLE file_ = INVALID_HANDLE_VALUE;
    HANDLE mapping_ = nullptr;
#else
    int fd_ = -1;
#endif
    const char* data_ = nullptr;
    std::size_t size_ = 0;
};

}  // namespace io
