#ifndef AES_MUSIC_PLAYER_APP_ADAU1761_H
#define AES_MUSIC_PLAYER_APP_ADAU1761_H

#include <string>
#include <functional>

#include "libaxidma.h"

#define CMD_FIFO_DEV_NAME "axis_fifo_0x43c10000"
#define I2S_DMA_INDEX 3

#define DEFAULT_SAMPLE_RATE 48000
#define DEFAULT_VOLUME 32
#define MAX_VOLUME 63

// ADAU1761 registers
#define ADAU1761_REG_CLOCK_CONTROL	                      UINT16_C(0x4000)
#define ADAU1761_REG_PLL_CONTROL	                      UINT16_C(0x4002)
#define ADAU1761_REG_CLOCK_ENABLE_0                       UINT16_C(0x40F9)
#define ADAU1761_REG_CLOCK_ENABLE_1                       UINT16_C(0x40FA)
#define ADAU1761_REG_SERIAL_PORT_0	                      UINT16_C(0x4015)
#define ADAU1761_REG_PLAYBACK_MONO_OUTPUT_CONTROL         UINT16_C(0x4027)
#define ADAU1761_REG_SERIAL_INPUT_ROUTE_CONTROL           UINT16_C(0x40F2)
#define ADAU1761_REG_PLAYBACK_POWER_MANAGEMENT            UINT16_C(0x4029)
#define ADAU1761_REG_DAC_CONTROL_0                        UINT16_C(0x402A)
#define ADAU1761_REG_PLAY_MIXER_LEFT_0 	                  UINT16_C(0x401C)
#define ADAU1761_REG_PLAY_MIXER_RIGHT_0                   UINT16_C(0x401E)
#define ADAU1761_REG_PLAYBACK_HEADPHONE_LEFT_VOL_CONTROL  UINT16_C(0x4023)
#define ADAU1761_REG_PLAYBACK_HEADPHONE_RIGHT_VOL_CONTROL UINT16_C(0x4024)




class adau1761 {
public:
    ~adau1761() { destroy(); }

    void init();
    void destroy();
    void *request_buffer(size_t buffer_size);
    int release_buffer(void *buffer);
    int play(void *buffer, int sample_rate, std::function<void(void*)> callback, void *callback_param);
    uint8_t set_relative_volume(int delta);
    void stop();
    size_t lookup_buffer(void *buffer) const;

private:
    struct _dma_state_t {
        // index of the DMA character device (specified in the device tree)
        int dev_index = I2S_DMA_INDEX;

        // pointer to the DMA character device
        axidma_dev_t dev;

        // command FIFO fd
        int cmd_fifo_fd;

        // tx channel number
        int tx_channel;

        // is the device busy
        bool is_busy = false;

        // current sample rate
        int sample_rate = DEFAULT_SAMPLE_RATE;

        // current volume (only the 6 LSBs are used)
        uint8_t volume = DEFAULT_VOLUME;

        // list of continuous memory chunks allocated
        std::vector<std::pair<size_t, void *>> dma_memories;

        // details of the current ongoing transfer (if no ongoing transfer current_transfer.is_busy == false)
        struct {
            // tx buffer pointer of current transfer
            void *tx_buffer;
            // function to call on transfer completion
            std::function<void(void *)> user_callback;
            // parameter to pass to the user callback function
            void *callback_param;
        } current_transfer;
    } _i2s_dma_state;

    static const std::unordered_map<int, std::array<int, 4>> _pll_divisors;

    static int _init_codec(int fifo_fd, int sample_rate, uint8_t volume);
    static int _set_sample_rate(int fifo_fd, int sample_rate);
    static int _set_volume(int fifo_fd, uint8_t volume);
    static uint8_t _spi_command(int fifo_fd, uint16_t addr, size_t data_len, const uint8_t *data = nullptr, int chip_addr = 0);
    static void _dma_callback(int channel_id, void *data);
};


#endif //AES_MUSIC_PLAYER_APP_ADAU1761_H
