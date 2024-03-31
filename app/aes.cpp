#include "aes.h"

#include <string>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <sys/mman.h>
#include <fstream>

#include "util.h"

void aes::init() {
    _dma_state_t* dev_states[] = {&_cipher_dma_state, &_decipher_dma_state};
    const array_t *channel_info;
    unsigned page_addr, page_offset;
    void *ptr;
    unsigned page_size = sysconf(_SC_PAGESIZE);

    // map AES peripheral memory area to access it from user space
    // it is for simplicity, but in general a kernel module should be created to access the device in a controlled way
    // using ioctl/sysfs.
    int fd = open("/dev/mem", O_RDWR);
    if (fd < 1)
        throw std::system_error(ENODEV, std::generic_category(),
                                "Failed to map AES register.");

    page_addr = (AES_BASE_ADDR & (~(page_size-1)));
    page_offset = AES_BASE_ADDR - page_addr;
    _aes_mem_ptr = ptr = mmap(nullptr, page_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, page_addr);
    ptr = ((char *)ptr + page_offset);

    for (auto &dev_state : dev_states) {
        // initialize mutexes
        pthread_mutex_init(&dev_state->state_mutex, nullptr);

        // calculate control register address
        dev_state->current_transfer.config_reg = (volatile uint32_t *)((char *)ptr +
                (dev_state == &_cipher_dma_state ? AES_CIPHER_CFG_REG_OFFSET : AES_DECIPHER_CFG_REG_OFFSET));

        // initialize DMA character device
        if (!(dev_state->dev = axidma_init_dev(dev_state->dev_index)))
            throw std::system_error(ENODEV, std::generic_category(),
                                    "Failed to initialize DMA device");

        // get rx, tx channels of DMA
        channel_info = axidma_get_dma_tx(dev_state->dev);
        if (channel_info->len < 1)
            throw std::system_error(ENODEV, std::generic_category(),
                                    "Failed to get TX channel for DMA device");
        else
            dev_state->tx_channel = channel_info->data[0];

        channel_info = axidma_get_dma_rx(dev_state->dev);
        if (channel_info->len < 1)
            throw std::system_error(ENODEV, std::generic_category(),
                                    "Failed to get RX channel for DMA device");
        else
            dev_state->rx_channel = channel_info->data[0];
    }
}

void aes::destroy() {
    _dma_state_t* dev_states[] = {&_cipher_dma_state, &_decipher_dma_state};

    // unmap AES peripheral memory area
    unsigned page_size = sysconf(_SC_PAGESIZE);
    munmap(_aes_mem_ptr, page_size);

    for (auto &dev_state : dev_states) {
        // free driver allocated continuous memories
        for (const auto& dma_memory_info : dev_state->dma_memories) {
            axidma_free(dev_state->dev, dma_memory_info.second, dma_memory_info.first);
        }

        // destroy DMA character device
        axidma_destroy(dev_state->dev);
    }
}

void aes::encrypt_file(const uint32_t key[AES_KEY_WIDTH / sizeof(uint32_t)], const std::string& input_path,
                       const std::string& output_path, const std::function<void(bool, void *)> *callback, void *callback_param) {
    _do_transfer(key, input_path, output_path, nullptr, 0, callback, callback_param, _cipher_dma_state);
}

void aes::encrypt_file(const uint32_t key[AES_KEY_WIDTH / sizeof(uint32_t)], const std::string &input_path, void *output_buffer,
                       size_t output_buffer_size, const std::function<void(bool, void *)> *callback, void *callback_param) {
    _do_transfer(key, input_path, "", output_buffer, output_buffer_size, callback, callback_param, _cipher_dma_state);
}

void aes::decrypt_file(const uint32_t key[AES_KEY_WIDTH / sizeof(uint32_t)], const std::string &input_path,
                       const std::string &output_path, const std::function<void(bool, void *)> *callback, void *callback_param) {
    _do_transfer(key, input_path, output_path, nullptr, 0, callback, callback_param, _decipher_dma_state);
}

void aes::decrypt_file(const uint32_t key[AES_KEY_WIDTH / sizeof(uint32_t)], const std::string &input_path, void *output_buffer,
                       size_t output_buffer_size, const std::function<void(bool, void *)> *callback, void *callback_param) {
    _do_transfer(key, input_path, "", output_buffer, output_buffer_size, callback, callback_param, _decipher_dma_state);
}

void aes::_dma_callback(int channel_id, void *data) {
    (void) channel_id;
    auto *dma_state = (aes::_dma_state_t *)data;
    bool is_success = true;
    void *rx_buff = nullptr, *tx_buff = nullptr;
    size_t rx_buff_size, tx_buff_size;

    // lookup rx buffer and size of the finished transfer in the dma_memories vector
    auto rx_it = std::find_if(dma_state->dma_memories.cbegin(),
                                  dma_state->dma_memories.cend(),
                                  [&dma_state](const auto & e){ return e.second == dma_state->current_transfer.rx_buffer; });
    if (rx_it != dma_state->dma_memories.cend()) {
        rx_buff_size = rx_it->first;
        rx_buff = rx_it->second;
    }

    // lookup tx buffer and size of the finished transfer in the dma_memories vector
    auto tx_it = std::find_if(dma_state->dma_memories.cbegin(),
                      dma_state->dma_memories.cend(),
                      [&dma_state](const auto & e){ return e.second == dma_state->current_transfer.tx_buffer; });
    if (tx_it != dma_state->dma_memories.cend()) {
        tx_buff_size = tx_it->first;
        tx_buff = tx_it->second;
    }

    if (!dma_state->current_transfer.output_file_path.empty()) {
        // write content of rx buffer into file
        try {
            auto out_file = std::fstream(dma_state->current_transfer.output_file_path, std::ios::out | std::ios::binary);
            out_file.write((char *)rx_buff, (std::streamsize)rx_buff_size);
            out_file.close();
        } catch (...) {
            is_success = false;
        }
    } else if (dma_state->current_transfer.output_buffer) {
        memcpy(dma_state->current_transfer.output_buffer, rx_buff, rx_buff_size);
    }

    // cleanup
    pthread_mutex_lock(&dma_state->state_mutex);

    if (rx_buff) {
        axidma_free(dma_state->dev, rx_buff, rx_buff_size);
        dma_state->dma_memories.erase(rx_it);
    }
    if (tx_buff) {
        axidma_free(dma_state->dev, tx_buff, tx_buff_size);
        dma_state->dma_memories.erase(tx_it);
    }

    dma_state->is_busy = false;

    // call user callback function
    if (dma_state->current_transfer.user_callback)
        std::invoke(*dma_state->current_transfer.user_callback, is_success, dma_state->current_transfer.callback_param);

    pthread_mutex_unlock(&dma_state->state_mutex);
}

void aes::_do_transfer(const uint32_t key[AES_KEY_WIDTH / sizeof(uint32_t)], const std::string &input_path,
                       const std::string &output_path, void *output_buffer, size_t output_buffer_size,
                       const std::function<void(bool, void *)> *callback, void *callback_param,
                       aes::_dma_state_t &dma_state) {
    void *rx_buff = nullptr, *tx_buff = nullptr;
    size_t rx_buff_size, tx_buff_size;
    std::streamsize aligned_file_size;

    if (!dma_state.dev)
        throw std::runtime_error("DMA device is not initialized.");

    pthread_mutex_lock(&dma_state.state_mutex);

    try {
        if (dma_state.is_busy)
            throw std::runtime_error("DMA device is BUSY.");

        aligned_file_size = (std::streamsize)aligned_size(get_file_size(input_path), AES_TEXT_WIDTH);
        rx_buff_size = aligned_file_size;
        tx_buff_size = AES_KEY_WIDTH + aligned_file_size;

        if (output_buffer != nullptr && (std::streamsize)output_buffer_size < aligned_file_size)
            throw std::runtime_error("Output buffer size too small.");

        // allocate tx buffer
        tx_buff = axidma_malloc(dma_state.dev, tx_buff_size);
        if (!tx_buff)
            throw std::runtime_error("Unable to allocate continuous memory for transfer.");
        dma_state.current_transfer.tx_buffer = tx_buff;
        dma_state.dma_memories.emplace_back(tx_buff_size, tx_buff);

        // allocate rx buffer
        rx_buff = axidma_malloc(dma_state.dev, rx_buff_size);
        if (!rx_buff)
            throw std::runtime_error("Unable to allocate continuous memory for transfer.");
        dma_state.current_transfer.rx_buffer = rx_buff;
        dma_state.dma_memories.emplace_back(rx_buff_size, rx_buff);

        // copy key into buffer
        memcpy(tx_buff, key, AES_KEY_WIDTH);
        // set config register of the AES peripheral to key loading
        // this will load the first AES_KEY_WIDTH bytes as key for the following values
        *dma_state.current_transfer.config_reg = 0xFFFFFFFF;

        // read input file into tx buffer
        std::ifstream in_file;
        std::streamsize bytes_read;

        in_file.open(input_path, std::ios::in | std::ios::binary | std::ios::ate);
        if(!in_file)
            throw std::runtime_error("Unable to open input file.");

        in_file.seekg(0, std::ios::beg);
        in_file.read((char *)tx_buff + AES_KEY_WIDTH, aligned_file_size);
        bytes_read = in_file.gcount();

        in_file.close();

        // zero-pad remaining buffer area
        memset((char *)tx_buff + AES_KEY_WIDTH + bytes_read, 0, aligned_file_size - bytes_read);

        // setup current transfer details
        dma_state.is_busy = true;
        dma_state.current_transfer.output_file_path = output_path;
        dma_state.current_transfer.output_buffer = output_buffer;
        dma_state.current_transfer.output_buffer_size = output_buffer_size;
        dma_state.current_transfer.user_callback = callback;
        dma_state.current_transfer.callback_param = callback_param;

        // setup callback
        axidma_set_callback(dma_state.dev, dma_state.rx_channel, aes::_dma_callback, &dma_state);

        // turn of key loading into the AES peripheral
        *dma_state.current_transfer.config_reg = 0x00000000;

        // stop any ongoing transfers
        axidma_stop_transfer(dma_state.dev, dma_state.tx_channel);
        axidma_stop_transfer(dma_state.dev, dma_state.rx_channel);
        pthread_mutex_unlock(&dma_state.state_mutex);

        // start transfer
        int ret = axidma_twoway_transfer(dma_state.dev, dma_state.tx_channel, tx_buff, tx_buff_size, nullptr,
                                         dma_state.rx_channel, rx_buff, rx_buff_size, nullptr, false);
        if (ret < 0)
            throw std::runtime_error("Failed to start transfer.");
    } catch(...) {
        // cleanup
        if (tx_buff)
            axidma_free(dma_state.dev, tx_buff, tx_buff_size);
        if (rx_buff)
            axidma_free(dma_state.dev, rx_buff, rx_buff_size);
        if (rx_buff || tx_buff)
            dma_state.dma_memories.erase(std::remove_if(
                    dma_state.dma_memories.begin(), dma_state.dma_memories.end(),
                    [&rx_buff, &tx_buff](const std::pair<size_t, void *>& e) {
                        return e.second == tx_buff || e.second == rx_buff;
                    }), dma_state.dma_memories.end());

        dma_state.is_busy = false;
        pthread_mutex_unlock(&dma_state.state_mutex);

        // rethrow
        throw;
    }

    pthread_mutex_unlock(&dma_state.state_mutex);
}
