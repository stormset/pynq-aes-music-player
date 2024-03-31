#include "virtual_file_wrapper.h"

#include <cstring>

void virtual_file_wrapper::open(void *buffer, sf_count_t buffer_size) {
    _buffer = buffer;
    _buffer_size = buffer_size;
    _offset = 0;
}

SF_VIRTUAL_IO *virtual_file_wrapper::as_sf_virtual_io() {
    return &sf_virtual_io_wrapper;
}

sf_count_t virtual_file_wrapper::_get_filelen() {
    return _buffer_size;
}

sf_count_t virtual_file_wrapper::_seek(sf_count_t offset, int whence) {
    switch (whence) {
        case SEEK_SET:
            _offset = offset;
            break;
        case SEEK_CUR :
            _offset = _offset + offset;
            break;
        case SEEK_END :
            _offset = _buffer_size + offset;
            break;
    }

    return (_offset < 0 || _offset > _buffer_size) ? -1 : _offset;
}

sf_count_t virtual_file_wrapper::_read(void *ptr, sf_count_t count) {
    if (_offset < 0 || _offset > _buffer_size)
        return 0;

    sf_count_t actual_count = std::min(count, _buffer_size - _offset);

    memcpy(ptr, (char *)_buffer + _offset, actual_count);
    _offset += actual_count;

    return actual_count;
}

sf_count_t virtual_file_wrapper::_write(const void *ptr, sf_count_t count) {
    if (_offset < 0 || _offset > _buffer_size)
        return 0;

    sf_count_t actual_count = std::min(count, _buffer_size - _offset);

    memcpy((char *)_buffer + _offset, ptr, actual_count) ;

    return actual_count;
}

sf_count_t virtual_file_wrapper::_tell() {
    return _offset;
}

