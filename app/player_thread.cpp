#include <iostream>
#include "player_thread.h"

#include "adau1761.h"
#include "sndfile.h"
#include "virtual_file_wrapper.h"

void player_thread::run() {
    adau1761 codec;
    fd_set read_fds;
    player_thread_msg player_rx_msg, player_tx_msg;

    // initialize codec
    codec.init();

    // message handling
    while (true) {
        // TODO: check against FD_SETSIZE limit
        FD_ZERO(&read_fds);
        FD_SET(_ui_to_player_read_pipe_fd, &read_fds);

        while (select(_ui_to_player_read_pipe_fd + 1, &read_fds, nullptr, nullptr, nullptr) == 0);

        if (FD_ISSET(_ui_to_player_read_pipe_fd, &read_fds)) {
            // read data sent by ui_thread
            if(read(_ui_to_player_read_pipe_fd, &player_rx_msg, sizeof(player_rx_msg)) > 0) {
                // handle commands coming from UI thread
                player_tx_msg.command = player_rx_msg.command;
                player_tx_msg.payload = 0;

                switch (player_rx_msg.command) {
                    case player_thread_msg::PLAY: {
                        int ret;
                        void *buff;
                        size_t buff_size;
                        SF_INFO snd_info;
                        SNDFILE *snd_file;
                        virtual_file_wrapper vf;

                        // stop if anything playing
                        codec.stop();

                        player_tx_msg.result = player_thread_msg::FAILURE;

                        if (read(_ui_to_player_read_pipe_fd, &buff, sizeof(&buff)) > 0) {
                            buff_size = player_rx_msg.payload;

                            // create virtual file wrapper from buffer
                            vf.open(buff, buff_size);
                            // open virtual file
                            snd_file = sf_open_virtual(vf.as_sf_virtual_io(), SFM_READ, &snd_info, &vf);

                            if (snd_file) {
                                void *dma_buff = codec.request_buffer(snd_info.frames * snd_info.channels * sizeof(short));
                                if (dma_buff) {
                                    // read audio samples from file
                                    sf_read_short(snd_file, (short *)dma_buff, snd_info.frames * snd_info.channels);

                                    ret = codec.play(dma_buff,
                                                     snd_info.samplerate,
                                                     [this](void *param){
                                                         (void) param;
                                                         player_thread_msg msg {.command = player_thread_msg::STOP, .result = player_thread_msg::SUCCESS, .payload = 0};
                                                         write(_player_to_ui_write_pipe_fd, &msg, sizeof(msg));
                                                     },
                                                     nullptr);
                                    if (ret != 0) {
                                        // error
                                        codec.release_buffer(dma_buff);
                                    } else {
                                        player_tx_msg.result = player_thread_msg::SUCCESS;
                                    }
                                }
                            }

                            free(buff);
                        }

                        write(_player_to_ui_write_pipe_fd, &player_tx_msg, sizeof(player_tx_msg));

                        break;
                    }
                    case player_thread_msg::STOP:
                        codec.stop();
                        player_tx_msg.result = player_thread_msg::SUCCESS;
                        write(_player_to_ui_write_pipe_fd, &player_tx_msg, sizeof(player_tx_msg));

                        break;
                    case player_thread_msg::EXIT:
                        codec.stop();
                        close(_ui_to_player_read_pipe_fd);
                        close(_player_to_ui_write_pipe_fd);
                        goto exit;
                    case player_thread_msg::VOLUME:
                        codec.set_relative_volume((int)player_rx_msg.payload);
                        break;
                }
            }
        }
    }

exit:
    pthread_exit(nullptr);
}
