#include <filesystem>
#include <sys/xattr.h>

size_t get_file_size(const std::string& path) {
    std::filesystem::path p(path);

    return std::filesystem::file_size(p);
}

// determines if the file is encrypted based on the extended attribute "user.is_encrypted"
bool is_encrypted(const std::string& path) {
    int ret;
    bool xattr_val;
    ret = (int)getxattr(path.c_str(), "user.is_encrypted", &xattr_val, sizeof(xattr_val));

    return ret > 0 && xattr_val;
}
