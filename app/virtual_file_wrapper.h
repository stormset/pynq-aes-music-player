#ifndef AES_MUSIC_PLAYER_APP_VIRTUAL_FILE_WRAPPER_H
#define AES_MUSIC_PLAYER_APP_VIRTUAL_FILE_WRAPPER_H


#include "sndfile.h"

#include <functional>

class virtual_file_wrapper {
public:
    virtual_file_wrapper() : virtual_file_wrapper(nullptr, 0) {};
    virtual_file_wrapper(void *buffer, sf_count_t buffer_size) : _buffer(buffer), _buffer_size(buffer_size), _offset(0) {};
    void open(void *buffer, sf_count_t buffer_size);
    SF_VIRTUAL_IO *as_sf_virtual_io();
private:
    sf_count_t _get_filelen();
    sf_count_t _seek(sf_count_t offset, int whence);
    sf_count_t _read(void *ptr, sf_count_t count);
    sf_count_t _write(const void *ptr, sf_count_t count);
    sf_count_t _tell();

    SF_VIRTUAL_IO sf_virtual_io_wrapper {
        .get_filelen = [](void *user_data){
            return std::invoke(&virtual_file_wrapper::_get_filelen, *(virtual_file_wrapper *)user_data);
        },
        .seek = [](sf_count_t offset, int whence, void *user_data){
            return std::invoke(&virtual_file_wrapper::_seek, *(virtual_file_wrapper *)user_data, offset, whence);
        },
        .read = [](void *ptr, sf_count_t count, void *user_data){
            return std::invoke(&virtual_file_wrapper::_read, *(virtual_file_wrapper *)user_data, ptr, count);
        },
        .write = [](const void *ptr, sf_count_t count, void *user_data){
            return std::invoke(&virtual_file_wrapper::_write, *(virtual_file_wrapper *)user_data, ptr, count);
        },
        .tell = [](void *user_data){
            return std::invoke(&virtual_file_wrapper::_tell, *(virtual_file_wrapper *)user_data);
        },
    };

    void *_buffer;
    sf_count_t _buffer_size, _offset;
};


#endif //AES_MUSIC_PLAYER_APP_VIRTUAL_FILE_WRAPPER_H
