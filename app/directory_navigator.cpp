#include "directory_navigator.h"

#include <dirent.h>
#include <sys/stat.h>

#define XATTR_NAME_BUFF_SIZE 256
#define XATTR_VAL_BUFF_SIZE 256

const std::unordered_map<std::string, std::string> directory_navigator::_xattr_symbol_map = {
    {"user.is_encrypted\001", "🔑"}
};

std::ostream &operator<<(std::ostream &os, const directory_navigator &navigator) {
    size_t max_height = std::min(navigator._entries.size(), navigator._scroll_height);
    size_t offset = std::max<long long>(0, navigator._current_entry_ind - (long long)max_height);

    for (size_t i = offset; i < std::min(max_height + offset + 1, navigator._entries.size()); ++i) {
        size_t pos = navigator._entries[i].name.rfind("─");
        if (pos != std::string::npos && navigator._current_entry_ind > 0)
            os << navigator._entries[i].name.substr(0, pos)
               << (i == (size_t)navigator._current_entry_ind ? "\033[1;31m" : "")
               << navigator._entries[i].name.substr(pos)
               << navigator._entries[i].suffix
               << (i == (size_t)navigator._current_entry_ind ? "\033[0m" : "") << std::endl;
        else
            os << "  " << navigator._entries[i].name << navigator._entries[i].suffix << std::endl;
    }

    return os;
}

// TODO: rewrite depth-first search to iterative version
bool directory_navigator::_get_dir_entries(const std::string& dir, std::vector<entry>& entries, size_t max_entries, int depth = 0) {
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
                        if (it != _xattr_symbol_map.end())
                            post += " " + it->second + " ";
                    }
                } while(len > 0 && pos < XATTR_NAME_BUFF_SIZE);

                entries.back().name += std::string(post.begin(), post.end());
            }
        }
    }

    closedir(dir_stream);

    return true;
}

bool directory_navigator::open_directory(const std::string &path, size_t max_entries) {
    _entries.clear();
    return _get_dir_entries(path, _entries, max_entries);
}

bool directory_navigator::relative_navigate_to_entry(directory_navigator::navigate_direction direction,
                                                     const std::function<bool(const entry&)>& predicate) {
    switch (direction) {
        case DOWN: {
            auto it = std::find_if(std::next(_entries.begin(), (long)_current_entry_ind + 1),
                                         _entries.end(), predicate);
            if (it != _entries.end()) {
                _current_entry_ind = std::distance(_entries.begin(), it);
                return true;
            }
            break;
        }
        case UP:
            auto it = std::find_if(_entries.rbegin() + (long)(_entries.size() - _current_entry_ind),
                                               _entries.rend(), predicate);
            if (it != _entries.rend()) {
                _current_entry_ind = std::distance(_entries.begin(), it.base() - 1);
                return true;
            }
            break;
    }

    return false;
}

void directory_navigator::set_scroll_height(size_t scroll_height) {
    _scroll_height = scroll_height;
}

bool directory_navigator::set_entry_suffix(const std::string &path, const std::string &suffix) {
    auto it = std::find_if(_entries.begin(),_entries.end(), [&path](const directory_navigator::entry& e){ return e.path == path; });
    if (it != _entries.end()) {
        it->suffix = suffix;
        return true;
    }

    return false;
}

std::optional<std::reference_wrapper<const directory_navigator::entry>> directory_navigator::get_current_entry() const {
    std::reference_wrapper<const directory_navigator::entry> ref = std::cref(_entries[_current_entry_ind]);

    return (_current_entry_ind < 0) ? std::nullopt : std::optional<std::reference_wrapper<const directory_navigator::entry>>{ref};
}
