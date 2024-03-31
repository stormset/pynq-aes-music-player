#ifndef AES_MUSIC_PLAYER_APP_UI_THREAD_H
#define AES_MUSIC_PLAYER_APP_UI_THREAD_H


#include <utility>

#include "pthread_wrapper.h"
#include "aes.h"

class ui_thread : public pthread_wrapper {
public:
    ui_thread(aes &aes_inst, std::string dir_name, int main_to_ui_read_pipe_fd, int player_to_ui_read_pipe_fd, int ui_to_player_write_pipe_fd) :
        _aes_inst(aes_inst),
        _dir_name(std::move(dir_name)),
        _main_to_ui_read_pipe_fd(main_to_ui_read_pipe_fd),
        _player_to_ui_read_pipe_fd(player_to_ui_read_pipe_fd),
        _ui_to_player_write_pipe_fd(ui_to_player_write_pipe_fd) { }

protected:
    void run () override;

private:
    aes& _aes_inst;
    std::string _dir_name;
    int _main_to_ui_read_pipe_fd;
    int _player_to_ui_read_pipe_fd, _ui_to_player_write_pipe_fd;

    enum _file_status {STOPPED, PREPARING, PLAYING};


    void _start_playing(const std::string& path, void *buffer = nullptr, size_t buffer_size = 0) const;
    void _stop_playing() const;
    void _change_volume(int val) const;
    static inline const char* _file_status_string(_file_status s);
    static unsigned short _get_terminal_height();
};


#endif //AES_MUSIC_PLAYER_APP_UI_THREAD_H
