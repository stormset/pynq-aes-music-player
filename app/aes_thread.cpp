#include "aes_thread.h"

void aes_thread::_dma_complete_callback(bool is_success, void *callback_params) {
    aes_thread *aes_thread = static_cast<class aes_thread *>(callback_params);

    pthread_mutex_lock(&aes_thread->_aes_mutex);

    aes_thread->_operation_status = (is_success ? SUCCESS : FAILED);

    pthread_cond_signal(&aes_thread->_aes_cond);
    pthread_mutex_unlock(&aes_thread->_aes_mutex);
}

void aes_thread::run() {
    // init mutex & condition flag
    pthread_mutex_init(&_aes_mutex, nullptr);
    pthread_cond_init(&_aes_cond, nullptr);
    
    pthread_mutex_lock(&_aes_mutex);
    _operation_status = BUSY;
    pthread_mutex_unlock(&_aes_mutex);

    std::function<void(bool, void*)> cb = std::function<void(bool, void*)>(aes_thread::_dma_complete_callback);
    try {
        switch (_current_operation) {
            case ENCRYPT:
                _aes_inst.encrypt_file(_key.data(), _input_path, _output_path, &cb, this);
                break;
            case ENCRYPT_INTO:
                _aes_inst.encrypt_file(_key.data(), _input_path, _output_buffer, _output_buffer_size, &cb,this);
                break;
            case DECRYPT:
                _aes_inst.decrypt_file(_key.data(), _input_path, _output_path, &cb, this);
                break;
            case DECRYPT_INTO:
                _aes_inst.decrypt_file(_key.data(), _input_path, _output_buffer, _output_buffer_size, &cb, this);
                break;
        }
    } catch (const std::exception &e) {
        _exception_ptr = std::current_exception();

        goto send_results;
    }

    // wait until operation finishes
    pthread_mutex_lock(&_aes_mutex);
    while (_operation_status == BUSY)
        pthread_cond_wait(&_aes_cond, &_aes_mutex);
    pthread_mutex_unlock(&_aes_mutex);

send_results:
    void *inst_ptr = this;
    write(_aes_to_ui_write_pipe_fd, &inst_ptr, sizeof(&inst_ptr));
    close(_aes_to_ui_write_pipe_fd);

    pthread_exit(nullptr);
}

aes_thread::operation_result_t aes_thread::get_status() {
    operation_result_t status;
    // if an exception was thrown during the run, rethrow it
    if (_exception_ptr) {
        std::rethrow_exception(_exception_ptr);
    }

    // otherwise return status
    pthread_mutex_lock(&_aes_mutex);
    status = _operation_status;
    pthread_mutex_unlock(&_aes_mutex);

    return status;
}

aes_thread::operation_t aes_thread::get_operation() const {
    return _current_operation;
}

std::string aes_thread::get_input_path() const {
    return _input_path;
}

std::string aes_thread::get_output_path() const {
    return _output_path;
}

std::pair<size_t, void *> aes_thread::get_output_buffer() const {
    return std::pair{_output_buffer_size, _output_buffer};
}
