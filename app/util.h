#ifndef AES_MUSIC_PLAYER_APP_UTIL_H
#define AES_MUSIC_PLAYER_APP_UTIL_H

#include <cstddef>
#include <string>

constexpr size_t aligned_size(size_t size, size_t alignment = 16) {
    return (size + alignment - 1) & ~(alignment-1);
}

size_t get_file_size(const std::string& path);
bool is_encrypted(const std::string& path);

#endif //AES_MUSIC_PLAYER_APP_UTIL_H
