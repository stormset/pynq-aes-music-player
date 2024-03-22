#ifndef AES_MUSIC_PLAYER_APP_DIRECTORY_NAVIGATOR_H
#define AES_MUSIC_PLAYER_APP_DIRECTORY_NAVIGATOR_H

#include <string>
#include <iostream>
#include <vector>
#include <functional>
#include <optional>
#include <unistd.h>
#include <sys/xattr.h>

class directory_navigator {
public:
    enum navigate_direction {UP, DOWN};

    struct entry {
        std::string path;
        std::string name;
        std::string suffix;
        bool is_directory;

        entry(std::string path, std::string name, bool is_directory) :
                path(std::move(path)), name(std::move(name)), is_directory(is_directory) {}
    };

    directory_navigator() = default;
    directory_navigator(const std::string& path,
                        const std::function<bool(const entry&)>& navigate_predicate = [](const entry&){ return true; },
                        size_t scroll_height = SIZE_MAX,
                        size_t max_entries = SIZE_MAX) : _scroll_height(scroll_height) {
        open_directory(path, max_entries);
        relative_navigate_to_entry(DOWN, navigate_predicate);
    };

    bool open_directory(const std::string& path, size_t max_entries = SIZE_MAX);
    bool relative_navigate_to_entry(navigate_direction direction, const std::function<bool(const entry&)>& predicate);
    void set_scroll_height(size_t scroll_height);
    bool set_entry_suffix(const std::string &path, const std::string &suffix);
    std::optional<std::reference_wrapper<const entry>> get_current_entry() const;

    friend std::ostream& operator<<(std::ostream& os, directory_navigator const& navigator);

private:
    std::vector<entry> _entries;
    size_t _scroll_height = 16;
    long long _current_entry_ind = -1;
    static const std::unordered_map<std::string, std::string> _xattr_symbol_map;

    static bool _get_dir_entries(const std::string& dir, std::vector<entry>& entries, size_t max_entries, int depth);
};

#endif //AES_MUSIC_PLAYER_APP_DIRECTORY_NAVIGATOR_H
