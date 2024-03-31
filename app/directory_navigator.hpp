#ifndef AES_MUSIC_PLAYER_APP_DIRECTORY_NAVIGATOR_HPP
#define AES_MUSIC_PLAYER_APP_DIRECTORY_NAVIGATOR_HPP

#include <string>
#include <iostream>
#include <vector>
#include <functional>
#include <optional>
#include <unistd.h>
#include <sys/xattr.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>

#define XATTR_NAME_BUFF_SIZE 256
#define XATTR_VAL_BUFF_SIZE 256

template <typename T>
class directory_navigator {
public:
    enum navigate_direction {UP, DOWN};

    struct entry {
        std::string path;
        std::string name;
        std::string suffix;
        bool is_directory;
        T tag;

        entry(std::string path, std::string name, bool is_directory) :
                path(std::move(path)), name(std::move(name)), is_directory(is_directory) {}
    };

    directory_navigator() = default;

    directory_navigator(const std::string& path,
                        const std::function<bool(const entry&)>& navigate_predicate = [](const entry&){ return true; },
                        const char* (*tag_to_str)(T) = nullptr,
                        size_t scroll_height = SIZE_MAX,
                        size_t max_entries = SIZE_MAX) : _scroll_height(scroll_height), _tag_to_str(tag_to_str) {
        open_directory(path, max_entries);
        relative_navigate_to_entry(DOWN, navigate_predicate);
    };

    bool open_directory(const std::string& path, size_t max_entries = SIZE_MAX);
    bool relative_navigate_to_entry(navigate_direction direction, const std::function<bool(const entry&)>& predicate);
    void set_scroll_height(size_t scroll_height);
    bool set_entry_suffix(const std::string &path, const std::string &suffix);
    bool set_entry_tag(const std::string &path, const T &tag);
    bool has_any_tag(const T &tag);
    void change_tags(const T &from, const T &to);

    std::optional<std::reference_wrapper<const entry>> get_current_entry() const;

    template <typename U>
    friend std::ostream& operator<<(std::ostream& os, directory_navigator<U> const& navigator);

private:
    std::vector<entry> _entries;
    size_t _scroll_height = 16;
    long long _current_entry_ind = -1;
    static const std::unordered_map<std::string, std::string> _xattr_symbol_map;
    const char* (*_tag_to_str)(T) = nullptr;

    static bool _get_dir_entries(const std::string& dir, std::vector<entry>& entries, size_t max_entries, int depth);
};

template <typename T>
const std::unordered_map<std::string, std::string> directory_navigator<T>::_xattr_symbol_map = {
        {"user.is_encrypted\001", "🔑"}
};

template <typename T>
std::ostream &operator<<(std::ostream &os, const directory_navigator<T> &navigator) {
    size_t max_height = std::min(navigator._entries.size(), navigator._scroll_height);
    size_t offset = std::max<long long>(0, navigator._current_entry_ind - (long long)max_height);

    for (size_t i = offset; i < std::min(max_height + offset + 1, navigator._entries.size()); ++i) {
        const char *str_tag = (navigator._tag_to_str ? navigator._tag_to_str(navigator._entries[i].tag) : "");
        const std::string &s = navigator._entries[i].suffix;
        size_t tag_len = strlen(str_tag);

        size_t pos = navigator._entries[i].name.rfind("─");
        if (pos != std::string::npos && navigator._current_entry_ind > 0)
            os << navigator._entries[i].name.substr(0, pos)
               << (i == (size_t)navigator._current_entry_ind ? "\033[1;31m" : "")
               << navigator._entries[i].name.substr(pos)
               << (s.empty() ? "" : " [") << s << (s.empty() ? "" : "]")
               << (tag_len == 0 ? "" : " [") << (tag_len == 0 ? "" : str_tag) << (tag_len == 0 ? "" : "]")
               << (i == (size_t)navigator._current_entry_ind ? "\033[0m" : "") << std::endl;
        else
            os << "  " << navigator._entries[i].name
               << (s.empty() ? "" : " [") << s << (s.empty() ? "" : "]")
               << (tag_len == 0 ? "" : " [") << (tag_len == 0 ? "" : str_tag) << (tag_len == 0 ? "" : "]") << std::endl;
    }

    return os;
}

// TODO: rewrite depth-first search to iterative version
template <typename T>
bool directory_navigator<T>::_get_dir_entries(const std::string& dir, std::vector<entry>& entries, size_t max_entries, int depth) {
    std::string entry_path, entry_name;
    DIR *dir_stream;
    struct dirent *dir_entry;
    struct stat file_stat{};
    char xattr_name[XATTR_NAME_BUFF_SIZE] = {'\0'};
    char xattr_val[XATTR_VAL_BUFF_SIZE] = {'\0'};

    if (entries.size() > max_entries)
        return true;

    if (depth == 0)
        entries.emplace_back(dir, dir, true);

    if (!(dir_stream = opendir(dir.c_str()))) {
        entries.emplace_back(dir, "[ Error opening directory " + dir + " ]", true);

        return false;
    }

    while ((dir_entry = readdir(dir_stream))) {
        if (entries.size() > max_entries)
            return true;

        entry_path = dir + "/" + dir_entry->d_name;
        entry_name = dir_entry->d_name;

        if(entry_name != "." && entry_name != "..") {
            // prepare prefix according to depth (TODO: optimize)
            std::string pre;
            pre.reserve((depth + 1) * 8);
            for (int i = 0; i < depth; ++i) pre += "  ║";
            pre += "  ╟─ ";
            entries.emplace_back(entry_path, pre + dir_entry->d_name, false);
        } else {
            continue;
        }

        // Get file stats to determine if it is a directory
        if (stat(entry_path.c_str(), &file_stat))
            continue; // skip in case error

        if (S_ISDIR(file_stat.st_mode)) {
            entries.back().is_directory = true;
            _get_dir_entries(entry_path, entries, max_entries, depth + 1);
        } else {
            // get file extended attributes and display corresponding symbols
            std::string post;

            if (listxattr(entry_path.c_str(), xattr_name, XATTR_NAME_BUFF_SIZE) != -1) {
                size_t len, pos = 0;
                do {
                    std::string xattr_name_s(xattr_name + pos);
                    len = xattr_name_s.length();
                    pos += len + 1;

                    if (len > 0) {
                        // get value of xattr and query map
                        std::string xattr_val_s;
                        if (getxattr(entry_path.c_str(), xattr_name_s.c_str(), xattr_val, XATTR_VAL_BUFF_SIZE - 1) != -1){
                            xattr_val_s = xattr_val;
                        }
                        auto it = _xattr_symbol_map.find(xattr_name_s + xattr_val_s);
                        if (it != _xattr_symbol_map.cend())
                            post += " " + it->second + " ";
                    }
                } while(len > 0 && pos < XATTR_NAME_BUFF_SIZE);

                entries.back().name += std::string(post.cbegin(), post.cend());
            }
        }
    }

    closedir(dir_stream);

    return true;
}

template <typename T>
bool directory_navigator<T>::open_directory(const std::string &path, size_t max_entries) {
    _entries.clear();
    return _get_dir_entries(path, _entries, max_entries, 0);
}

template <typename T>
bool directory_navigator<T>::relative_navigate_to_entry(directory_navigator<T>::navigate_direction direction,
                                                        const std::function<bool(const entry&)>& predicate) {
    switch (direction) {
        case DOWN: {
            auto it = std::find_if(std::next(_entries.cbegin(), (long)_current_entry_ind + 1),
                                   _entries.cend(), predicate);
            if (it != _entries.end()) {
                _current_entry_ind = std::distance(_entries.cbegin(), it);
                return true;
            }
            break;
        }
        case UP:
            auto it = std::find_if(_entries.crbegin() + (long)(_entries.size() - _current_entry_ind),
                                   _entries.crend(), predicate);
            if (it != _entries.crend()) {
                _current_entry_ind = std::distance(_entries.cbegin(), it.base() - 1);
                return true;
            }
            break;
    }

    return false;
}

template <typename T>
void directory_navigator<T>::set_scroll_height(size_t scroll_height) {
    _scroll_height = scroll_height;
}

template <typename T>
bool directory_navigator<T>::set_entry_suffix(const std::string &path, const std::string &suffix) {
    auto it = std::find_if(_entries.begin(),_entries.end(), [&path](const directory_navigator::entry& e){ return e.path == path; });
    if (it != _entries.end()) {
        it->suffix = suffix;
        return true;
    }

    return false;
}

template <typename T>
bool directory_navigator<T>::set_entry_tag(const std::string &path, const T &tag) {
    auto it = std::find_if(_entries.begin(),_entries.end(), [&path](const directory_navigator::entry& e){ return e.path == path; });
    if (it != _entries.end()) {
        it->tag = tag;
        return true;
    }

    return false;
}

template<typename T>
bool directory_navigator<T>::has_any_tag(const T &tag) {
    auto it = std::find_if(_entries.cbegin(), _entries.cend(), [&tag](const entry &e) { return e.tag == tag; });

    return it != _entries.cend();
}

template <typename T>
void directory_navigator<T>::change_tags(const T &from, const T &to) {
    for (auto& e : _entries) {
        if (e.tag == from)
            e.tag = to;
    }
}

template <typename T>
std::optional<std::reference_wrapper<const typename directory_navigator<T>::entry>> directory_navigator<T>::get_current_entry() const {
    std::reference_wrapper<const directory_navigator::entry> ref = std::cref(_entries[_current_entry_ind]);

    return (_current_entry_ind < 0) ? std::nullopt : std::optional<std::reference_wrapper<const directory_navigator<T>::entry>>{ref};
}


#endif //AES_MUSIC_PLAYER_APP_DIRECTORY_NAVIGATOR_HPP
