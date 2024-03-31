#ifndef AES_MUSIC_PLAYER_APP_AES_H
#define AES_MUSIC_PLAYER_APP_AES_H


#include <string>
#include <functional>

#include "libaxidma.h"

#define CIPHER_DMA_INDEX 1
#define DECIPHER_DMA_INDEX 2

#define AES_TEXT_WIDTH 16
#define AES_KEY_WIDTH 16

#define AES_BASE_ADDR 0x43C00000
#define AES_CIPHER_CFG_REG_OFFSET 0
#define AES_DECIPHER_CFG_REG_OFFSET 4

class aes {
public:
    ~aes() { destroy(); }

    void init();
    void destroy();
    void encrypt_file(const uint32_t key[AES_KEY_WIDTH / sizeof(uint32_t)], const std::string& input_path, const std::string& output_path,
                      const std::function<void(bool, void*)>* callback, void *callback_param);
    void encrypt_file(const uint32_t key[AES_KEY_WIDTH / sizeof(uint32_t)], const std::string& input_path, void *output_buffer, size_t output_buffer_size,
                      const std::function<void(bool, void*)>* callback, void *callback_param);
    void decrypt_file(const uint32_t key[AES_KEY_WIDTH / sizeof(uint32_t)], const std::string& input_path, const std::string& output_path,
                      const std::function<void(bool, void*)>* callback, void *callback_param);
    void decrypt_file(const uint32_t key[AES_KEY_WIDTH / sizeof(uint32_t)], const std::string& input_path, void *output_buffer, size_t output_buffer_size,
                      const std::function<void(bool, void*)>* callback, void *callback_param);

private:
    struct _dma_state_t {
        // index of the DMA character device (specified in the device tree)
        int dev_index;

        // pointer to the DMA character device
        axidma_dev_t dev;

        // rx, tx channel numbers
        int tx_channel, rx_channel;

        // is the device busy
        // As the scatter-gather descriptor ring is normally larger than needed to describe a transfer of a regular-sized
        // file, it would be possible to extend this with support of multiple ongoing transfers, but for this
        // the driver needs to be extended.
        bool is_busy = false;

        // list of continuous memory chunks allocated
        std::vector<std::pair<size_t, void *>> dma_memories;

        // details of the current ongoing transfer (if no ongoing transfer current_transfer.is_busy == false)
        struct {
            // tx buffer pointer of current transfer
            void * tx_buffer;
            // rx buffer pointer of current transfer
            void * rx_buffer;
            // file path to copy the content of the rx buffer once finished
            std::string output_file_path;
            // buffer to copy the content of the rx buffer once finished
            void *output_buffer;
            // size of the output buffer, an exception is raised if smaller than the output fits into
            size_t output_buffer_size;
            // function to call on transfer completion
            const std::function<void(bool, void *)>* user_callback;
            // parameter to pass to the user callback function
            void *callback_param;
            // virtual address of the config register of the device (mmap-ped during init)
            volatile uint32_t *config_reg;
        } current_transfer;
        // mutual exclusion
        pthread_mutex_t state_mutex;
    };

    _dma_state_t _cipher_dma_state {.dev_index = CIPHER_DMA_INDEX, .dev{}, .tx_channel{}, .rx_channel{}, .dma_memories{}, .current_transfer{}, .state_mutex{}};
    _dma_state_t _decipher_dma_state {.dev_index = DECIPHER_DMA_INDEX, .dev{}, .tx_channel{}, .rx_channel{}, .dma_memories{}, .current_transfer{}, .state_mutex{}};

    void *_aes_mem_ptr{};

    static void _do_transfer(const uint32_t key[4], const std::string& input_path, const std::string& output_path, void *output_buffer, size_t output_buffer_size,
                             const std::function<void(bool, void*)>* callback, void *callback_param, _dma_state_t &dma_state);
    static void _dma_callback(int channel_id, void *data);
};


#endif //AES_MUSIC_PLAYER_APP_AES_H
