#ifndef AES_MUSIC_PLAYER_APP_AES_THREAD_H
#define AES_MUSIC_PLAYER_APP_AES_THREAD_H


#include <exception>
#include <stdexcept>

#include "pthread_wrapper.h"
#include "aes.h"

class aes_thread : public pthread_wrapper {
public:
    enum operation_t {ENCRYPT, ENCRYPT_INTO, DECRYPT, DECRYPT_INTO};
    enum operation_result_t {SUCCESS, FAILED, BUSY};

    aes_thread(aes &aes_inst, operation_t operation, const std::array<uint32_t , 4> key, const std::string input_path,
               const std::string output_path, void *output_buffer, size_t output_buffer_size, int aes_to_ui_write_pipe_fd) :
                   _aes_inst(aes_inst), _current_operation(operation), _key(key), _input_path(input_path),
                   _output_path(output_path), _output_buffer(output_buffer), _output_buffer_size(output_buffer_size),
                   _aes_to_ui_write_pipe_fd(aes_to_ui_write_pipe_fd) { };

    operation_result_t get_status();
    operation_t get_operation()const;
    std::string get_input_path() const;
    std::string get_output_path() const;
    std::pair<size_t, void *> get_output_buffer() const;

protected:
    void run() override;

private:
    aes& _aes_inst;
    operation_t _current_operation;
    const std::array<uint32_t , 4> _key;
    const std::string _input_path;
    const std::string _output_path;
    void *_output_buffer;
    size_t _output_buffer_size;
    int _aes_to_ui_write_pipe_fd;

    operation_result_t _operation_status{};
    std::exception_ptr _exception_ptr;

    pthread_mutex_t _aes_mutex{};
    pthread_cond_t _aes_cond{};

    static void _dma_complete_callback(bool is_success, void *callback_params);
};


#endif //AES_MUSIC_PLAYER_APP_AES_THREAD_H
