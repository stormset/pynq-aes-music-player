#ifndef AES_MUSIC_PLAYER_APP_PLAYER_THREAD_H
#define AES_MUSIC_PLAYER_APP_PLAYER_THREAD_H


#include "pthread_wrapper.h"
#include "aes.h"

class player_thread : public pthread_wrapper {
public:
    struct player_thread_msg {
        enum command_t {PLAY, STOP, VOLUME, EXIT} command{};
        enum result_t {SUCCESS, FAILURE} result{};
        long int payload{};
    };

    player_thread(int ui_to_player_read_pipe_fd, int player_to_ui_write_pipe_fd) :
            _ui_to_player_read_pipe_fd(ui_to_player_read_pipe_fd),
            _player_to_ui_write_pipe_fd(player_to_ui_write_pipe_fd) { }

protected:
    void run() override;

private:
    int _ui_to_player_read_pipe_fd;
    int _player_to_ui_write_pipe_fd;
};


#endif //AES_MUSIC_PLAYER_APP_PLAYER_THREAD_H
