#include <sys/ioctl.h>
#include <sstream>
#include <fstream>
#include <memory>
#include "ui_thread.h"
#include "player_thread.h"
#include "directory_navigator.hpp"
#include "util.h"
#include "aes_thread.h"

using player_thread_msg = player_thread::player_thread_msg;

void ui_thread::_start_playing(const std::string& path, void *buffer, size_t buffer_size) const {
    player_thread_msg player_tx_msg;

    size_t size = buffer ? buffer_size : get_file_size(path);
    void *buff = buffer ? buffer : malloc(size);
    player_tx_msg.command = player_thread_msg::PLAY;
    player_tx_msg.payload = size;

    if (!buffer && buff) {
        // fill buffer to play from file
        std::ifstream in_file;

        in_file.open(path, std::ios::in | std::ios::binary | std::ios::ate);
        if(!in_file) {
            throw std::system_error(ENOENT, std::generic_category(), "Failed to open input file.");
        }

        in_file.seekg(0, std::ios::beg);
        in_file.read((char *)buff, size);
        in_file.close();
    }

    if (buff) {
        // send PLAY command
        write(_ui_to_player_write_pipe_fd, &player_tx_msg, sizeof(player_tx_msg));
        // send buffer address the file is loaded into
        write(_ui_to_player_write_pipe_fd, &buff, sizeof(&buff));
    } else {
        throw std::system_error(ENOMEM, std::generic_category(), "Failed to allocate memory for playing.");
    }
}

void ui_thread::_stop_playing() const {
    player_thread_msg player_tx_msg;

    player_tx_msg.command = player_thread_msg::STOP;
    player_tx_msg.payload = 0;

    // send STOP command
    write(_ui_to_player_write_pipe_fd, &player_tx_msg, sizeof(player_tx_msg));
}

void ui_thread::_change_volume(int delta) const {
    player_thread_msg player_tx_msg;

    player_tx_msg.command = player_thread_msg::VOLUME;
    player_tx_msg.payload = delta;

    // send VOLUME command
    write(_ui_to_player_write_pipe_fd, &player_tx_msg, sizeof(player_tx_msg));
}

void ui_thread::run() {
    // vector holding pointers to running AES thread instances
    std::vector<aes_thread *> aes_threads;
    // vector holding the file descriptors of the pipes between this and the AES thread
    std::vector<int> aes_to_ui_read_pipe_fds;

    player_thread_msg player_rx_msg, player_tx_msg;

    fd_set read_fds;
    char stdin_buff[3];
    int ret;

    unsigned short prev_nr_rows, nr_rows = _get_terminal_height();
    std::function is_file_pred = [](const directory_navigator<_file_status>::entry& e){ return !e.is_directory; };
    directory_navigator<_file_status> directory_navigator(_dir_name, is_file_pred, _file_status_string, nr_rows - 7);

    const auto& start_aes_thread = [&](aes_thread::operation_t operation, const std::array<uint32_t , 4> &key,
            const std::string& input_path, const std::string& output_path = "", void * output_buffer = nullptr, size_t output_buffer_size = 0) -> int {
        int p[2];
        if (pipe(p) != 0)
            return -1;

        aes_to_ui_read_pipe_fds.push_back(p[0]);
        aes_thread *t = new aes_thread(_aes_inst, operation, key, input_path, output_path, output_buffer, output_buffer_size, p[1]);

        if (t->start())
            aes_threads.push_back(t);
        else
            return -1;

        return 0;
    };

    while (true) {
        std::cout << "\033[H";
        std::cout << "\x1b[B" << "\x1b[B" << "\x1b[B" << "\x1b[B";
        std::cout << "\033[J";
        std::cout << directory_navigator;

        auto selected = directory_navigator.get_current_entry();

        // TODO: check against FD_SETSIZE limit
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(_main_to_ui_read_pipe_fd, &read_fds);
        FD_SET(_player_to_ui_read_pipe_fd, &read_fds);

        int max_fd = std::max(STDIN_FILENO, _main_to_ui_read_pipe_fd);
        max_fd = std::max(max_fd, _player_to_ui_read_pipe_fd);
        for(const auto& e : aes_to_ui_read_pipe_fds) {
            if (e > max_fd)
                max_fd = e;
            FD_SET(e, &read_fds);
        }

        while (select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr) == 0);

        if (FD_ISSET(_main_to_ui_read_pipe_fd, &read_fds)) {
            // write channel used only to send EOF => close read fd and terminate thread
            close(_main_to_ui_read_pipe_fd);

            // stop player thread
            player_tx_msg.command = player_thread_msg::EXIT;
            player_tx_msg.payload = 0;
            write(_ui_to_player_write_pipe_fd, &player_tx_msg, sizeof(player_tx_msg));

            // TODO: inform all aes_thread threads to gracefully exit (a new pipe, or shared variable)

            // close all pending aes_thread read channels
            for(const auto& e : aes_to_ui_read_pipe_fds) {
                close(e);
            }

            // close pipes to player thread
            close(_player_to_ui_read_pipe_fd);
            close(_ui_to_player_write_pipe_fd);

            break;
        }

        if (FD_ISSET(_player_to_ui_read_pipe_fd, &read_fds)) {
            if(read(_player_to_ui_read_pipe_fd, &player_rx_msg, sizeof(player_rx_msg)) > 0) {
                // handle ui_thread results
                switch (player_rx_msg.command) {
                    case player_thread_msg::PLAY:
                        if (player_rx_msg.result == player_thread_msg::SUCCESS)
                            directory_navigator.change_tags(PREPARING, PLAYING);
                        else
                            directory_navigator.change_tags(PREPARING, STOPPED);
                        break;
                    case player_thread_msg::STOP:
                        directory_navigator.change_tags(PLAYING, STOPPED);

                        break;
                    default:
                        // left empty intentionally
                        break;
                }
            }
        }

        auto it = aes_to_ui_read_pipe_fds.begin();
        int fd;
        while(it != aes_to_ui_read_pipe_fds.end()) {
            fd = *it;
            if (FD_ISSET(fd, &read_fds)) {
                // in case AES thread finishes operation, it sends back a pointer to itself, we read it here
                aes_thread *aes_t;
                if(read(fd, &aes_t, sizeof(&aes_t)) > 0) {
                    // handle aes_thread results
                    try {
                        // get status of AES thread, in case an exception was raised, it will be rethrown, so we should handle it
                        aes_thread::operation_result_t res = aes_t->get_status();
                        std::string input_path = aes_t->get_input_path();

                        switch (res) {
                            case aes_thread::SUCCESS: {
                                std::string output_path = aes_t->get_output_path();

                                switch (aes_t->get_operation()) {
                                    case aes_thread::ENCRYPT: {
                                        // set xattr of the temporary (encrypted) file to show it is encrypted
                                        bool xattr_val = true;
                                        if (setxattr(output_path.c_str(), "user.is_encrypted", &xattr_val, sizeof(xattr_val), 0) == 0) {
                                            // rename temporary (encrypted) file to the original name (this will overwrite th original)
                                            rename(output_path.c_str(), input_path.c_str());
                                            // clear status message
                                            directory_navigator.set_entry_suffix(input_path, "");
                                        } else {
                                            // remove temporary (encrypted) file
                                            remove(output_path.c_str());
                                            // indicate error
                                            directory_navigator.set_entry_suffix(input_path, "Error setting encrypted attribute.");
                                        }
                                        // reopen directory to reflect changes
                                        directory_navigator.open_directory(_dir_name);

                                        break;
                                    }
                                    case aes_thread::ENCRYPT_INTO: {
                                        // left empty intentionally: not used
                                        break;
                                    }
                                    case aes_thread::DECRYPT: {
                                        // remove original (encrypted) file and rename temporary to original
                                        rename(output_path.c_str(), input_path.c_str());

                                        // reopen directory to reflect changes
                                        directory_navigator.open_directory(_dir_name);
                                        // clear status message
                                        directory_navigator.set_entry_suffix(input_path, "");

                                        break;
                                    }
                                    case aes_thread::DECRYPT_INTO: {
                                        // play decrypted content
                                        try {
                                            const auto out = aes_t->get_output_buffer();
                                            _start_playing(input_path, out.second, out.first);
                                            directory_navigator.set_entry_tag(input_path, PREPARING);
                                        } catch (const std::exception &e) {
                                            directory_navigator.set_entry_suffix(input_path, e.what());
                                            directory_navigator.set_entry_tag(input_path, STOPPED);
                                        }

                                        break;
                                    }
                                }

                                break;
                            }
                            case aes_thread::FAILED:
                                if (aes_t->get_operation() == aes_thread::DECRYPT_INTO)
                                    directory_navigator.set_entry_tag(input_path, STOPPED);

                                directory_navigator.set_entry_suffix(input_path, "Something went wrong.");
                                break;
                            case aes_thread::BUSY:
                                // left empty intentionally
                                break;
                        }
                    } catch (const std::exception &e) {
                        directory_navigator.set_entry_suffix(aes_t->get_input_path(), e.what());
                        if (aes_t->get_operation() == aes_thread::DECRYPT_INTO)
                            directory_navigator.set_entry_tag(aes_t->get_input_path(), STOPPED);
                    }

                    // free up AES thread instance
                    delete aes_t;
                    // remove instance from vector
                    aes_threads.erase(std::remove(aes_threads.begin(), aes_threads.end(), aes_t), aes_threads.end());
                }
                close(fd);
                it = aes_to_ui_read_pipe_fds.erase(it);
            } else ++it;
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            // read user input from stdin
            if ((ret = read(STDIN_FILENO, stdin_buff, sizeof(stdin_buff))) > 0 && selected.has_value()) {
                const auto& selected_entry = selected.value().get();

                // check if terminal height changed
                prev_nr_rows = nr_rows;
                nr_rows = _get_terminal_height();
                if (prev_nr_rows != nr_rows)
                    directory_navigator.set_scroll_height(nr_rows - 7);

                // handle keyboard presses (arrows are three-character sequences)
                switch(ret > 2 ? stdin_buff[1] : stdin_buff[0]) {
                    case ' ' : {
                        // check if anything playing
                        if (selected_entry.tag == PLAYING) {
                            // stop the selected
                            _stop_playing();
                        } else if(!directory_navigator.has_any_tag(PREPARING)) {
                            if (directory_navigator.has_any_tag(PLAYING)) {
                                // stop the playing track and start the selected
                                _stop_playing();
                            }

                            // check if file is encrypted first
                            bool file_encrypted = is_encrypted(selected_entry.path);

                            if (!file_encrypted) {
                                try {
                                    _start_playing(selected_entry.path);
                                    directory_navigator.set_entry_tag(selected_entry.path, PREPARING);
                                } catch (const std::exception &e) {
                                    directory_navigator.set_entry_suffix(selected_entry.path, e.what());
                                }
                            } else {
                                // decrypt file into buffer
                                try {
                                    size_t file_size = get_file_size(selected_entry.path);
                                    void *buff = malloc(file_size);

                                    if (buff) {
                                        // TODO: key input
                                        std::array<uint32_t , 4> key{0xFFFFFFFF, 0x00000000, 0xAAAAAAAA, 0xCCCCCCCC};
                                        ret = start_aes_thread(aes_thread::DECRYPT_INTO,
                                                               key,
                                                               selected_entry.path,
                                                               "",
                                                               buff,
                                                               file_size);
                                        if (ret == 0)
                                            directory_navigator.set_entry_tag(selected_entry.path, PREPARING);
                                        else
                                            directory_navigator.set_entry_suffix(selected_entry.path, "Failed to start AES thread.");
                                    } else {
                                        directory_navigator.set_entry_suffix(selected_entry.path, "Failed to allocate memory for decryption.");
                                    }
                                } catch (const std::system_error &e) {
                                    directory_navigator.set_entry_suffix(selected_entry.path, e.what());
                                }
                            }
                        } else {
                            // we should not play anything until preparation is done
                            directory_navigator.set_entry_suffix(selected_entry.path, "Please wait until previous selection finishes preparation.");
                        }

                        break;
                    }
                    case 'e': case 'd': {
                        std::string tmp_name(tempnam(_dir_name.c_str(), selected_entry.path.substr(selected_entry.path.find_last_of("/\\") + 1).c_str()));
                        aes_thread::operation_t operation = stdin_buff[0] == 'e' ? aes_thread::ENCRYPT : aes_thread::DECRYPT;

                        // check if file is encrypted
                        bool file_encrypted = is_encrypted(selected_entry.path);

                        if ((operation == aes_thread::ENCRYPT && !file_encrypted) ||
                            (operation == aes_thread::DECRYPT && file_encrypted)) {
                            try {
                                // TODO: key input
                                std::array<uint32_t , 4> key{0xFFFFFFFF, 0x00000000, 0xAAAAAAAA, 0xCCCCCCCC};
                                ret = start_aes_thread(operation,
                                                       key,
                                                       selected_entry.path,
                                                       tmp_name);
                                if (ret == 0)
                                    directory_navigator.set_entry_suffix(selected_entry.path,
                                                                         operation == aes_thread::ENCRYPT ? "encrypting..." : "decrypting...");
                                else
                                    directory_navigator.set_entry_suffix(selected_entry.path, "Failed to start AES thread.");

                            } catch (const std::system_error &e) {
                                directory_navigator.set_entry_suffix(selected_entry.path, e.what());
                            }
                        } else {
                            directory_navigator.set_entry_suffix(selected_entry.path,
                                                                 (operation == aes_thread::ENCRYPT ? "already encrypted" : "already decrypted"));
                        }

                        break;
                    }
                    case 0x5B:
                        if (ret > 2) {
                            switch (stdin_buff[2]) {
                                case 0x42: // down arrow
                                    directory_navigator.relative_navigate_to_entry(::directory_navigator<_file_status>::DOWN, is_file_pred);
                                    break;
                                case 0x41: // up arrow
                                    directory_navigator.relative_navigate_to_entry(::directory_navigator<_file_status>::UP, is_file_pred);
                                    break;
                                case 0x44: // left arrow
                                    _change_volume(-2);
                                    break;
                                case 0x43: // right arrow
                                    _change_volume(+2);
                                    break;
                            }
                        }
                        break;
                }
            }
        }
    }

    pthread_exit(nullptr);
}

unsigned short ui_thread::_get_terminal_height() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_row;
}

const char *ui_thread::_file_status_string(ui_thread::_file_status s) {
    switch (s)
    {
        case PREPARING:   return "preparing to play...";
        case PLAYING:   return "♪ playing ♪";
        default:      return "";
    }
}
