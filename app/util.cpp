#include <filesystem>

size_t get_file_size(const std::string& path) {
    std::filesystem::path p(path);

    return std::filesystem::file_size(p);
}
