#include "adau1761.h"

#include <csignal>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <system_error>
#include <utility>

const std::unordered_map<int, std::array<int, 4>> adau1761::_pll_divisors = {
        //  see: https://www.analog.com/media/en/technical-documentation/data-sheets/ADAU1761.pdf p. 28.
        // MCLK @ 13 MHz
        // f           X    R      M     N
        {44100, {   1,    3, 8125, 3849}},
        {48000, {   1,    3, 1625, 1269}}
};

uint8_t adau1761::_spi_command(int fifo_fd, uint16_t addr, size_t data_len, const uint8_t *data, int chip_addr) {
    size_t packet_size = sizeof(uint32_t) * (data_len + 3);
    uint32_t *tx_buff = (uint32_t *) calloc(data_len + 3, sizeof(uint32_t));
    uint32_t rx_val = 0;

    if (tx_buff) {
        tx_buff[0] = ((chip_addr << 1) | (data ? 0x00 : 0x01)) & 0xFF;
        tx_buff[1] = (addr >> 8) & 0xFF;
        tx_buff[2] = addr & 0xFF;

        if (data) {
            // swap endianness
            for (size_t i = 0; i < data_len; ++i) {
                if ((i != data_len - 1) && (i % 2 == 0)) {
                    tx_buff[i + 3] = data[i + 1];
                } else if (i == data_len - 1) {
                    tx_buff[i + 3] = data[i];
                }
            }
        }
        write(fifo_fd, tx_buff, packet_size);

        for (size_t i = 0; i < (data_len + 3); ++i) {
            read(fifo_fd, &rx_val, sizeof(rx_val));
        }

        free(tx_buff);
    }

    return (uint8_t)(rx_val & 0xFF);
}

int adau1761::_set_sample_rate(int fifo_fd, int sample_rate) {
    uint16_t tx_data_long[3];

    // lookup PLL values for sampling frequency
    auto it = _pll_divisors.find(sample_rate);
    if (it == _pll_divisors.cend())
        return -1;

    // set PLL parameters
    tx_data_long[0] = it->second.at(2); // M
    tx_data_long[1] = it->second.at(3); // N
    tx_data_long[2] = (it->second.at(1) << 11) + ((it->second.at(0) - 1) << 9) + (1 << 8) + 0; // R and X

    _spi_command(fifo_fd, ADAU1761_REG_PLL_CONTROL, sizeof(tx_data_long), (uint8_t *)tx_data_long);
    tx_data_long[2] |= 1;
    _spi_command(fifo_fd, ADAU1761_REG_PLL_CONTROL, sizeof(tx_data_long), (uint8_t *)tx_data_long);

    // wait for the PLL to lock
    while (true) {
        if (_spi_command(fifo_fd, ADAU1761_REG_PLL_CONTROL, sizeof(tx_data_long)) & 0x02)
            break;
        usleep(1000);
    }

    return 0;
}

int adau1761::_set_volume(int fifo_fd, uint8_t volume) {
    uint8_t data;

    // read current value for left channel
    data = _spi_command(fifo_fd, ADAU1761_REG_PLAYBACK_HEADPHONE_LEFT_VOL_CONTROL, sizeof(data));
    data = ((volume & 0x3F) << 2) + (data & 0x3); // change volume (6 MSBs of the register)
    _spi_command(fifo_fd, ADAU1761_REG_PLAYBACK_HEADPHONE_LEFT_VOL_CONTROL, sizeof(data), &data);

    // read current value for right channel
    data = _spi_command(fifo_fd, ADAU1761_REG_PLAYBACK_HEADPHONE_RIGHT_VOL_CONTROL, sizeof(data));
    data = ((volume & 0x3F) << 2) + (data & 0x3); // change volume (6 MSBs of the register)
    _spi_command(fifo_fd, ADAU1761_REG_PLAYBACK_HEADPHONE_RIGHT_VOL_CONTROL, sizeof(data), &data);

    return 0;
}

int adau1761::_init_codec(int fifo_fd, int sample_rate, uint8_t volume) {
    uint8_t tx_data;

    // switch from I2C to SPI (3 consecutive reads at address 0x4000)
    _spi_command(fifo_fd, ADAU1761_REG_CLOCK_CONTROL, 1);
    _spi_command(fifo_fd, ADAU1761_REG_CLOCK_CONTROL, 1);
    _spi_command(fifo_fd, ADAU1761_REG_CLOCK_CONTROL, 1);

    // set sample rate
    if (_set_sample_rate(fifo_fd, sample_rate) != 0)
        return -1;

    // enable PLL
    tx_data = 0x0F; // use PLL as clock source, core clock speed 1024 * f_sampling
    _spi_command(fifo_fd, ADAU1761_REG_CLOCK_CONTROL, sizeof(tx_data), &tx_data);
    //usleep(10000); // wait (needed)
 
    // setup clocks of used circuits (disable not needed to save power)
    tx_data = 0x4B; // enable all necessary clocks (disabled: ALC, dejitter (decimator, interpolator))
    _spi_command(fifo_fd, ADAU1761_REG_CLOCK_ENABLE_0, sizeof(tx_data), &tx_data);
    tx_data = 0x03; // enable both clock generators (for DACs and LRCLK/BCLK)
    _spi_command(fifo_fd, ADAU1761_REG_CLOCK_ENABLE_1, sizeof(tx_data), &tx_data);

    // setup serial port 1
    tx_data = 0x01; // serial data port bus mode: master (LRCLK, BCLK are driven by the codec)
    _spi_command(fifo_fd, ADAU1761_REG_SERIAL_PORT_0, sizeof(tx_data), &tx_data);

    // setup playback path (ADAU1761 see p. 35)
    tx_data = 0x00; // mute mono output
    _spi_command(fifo_fd, ADAU1761_REG_PLAYBACK_MONO_OUTPUT_CONTROL, sizeof(tx_data), &tx_data);

    tx_data = 0x01; // serial data input routing: serial port 0 is routed to L, R DACs
    _spi_command(fifo_fd, ADAU1761_REG_SERIAL_INPUT_ROUTE_CONTROL, sizeof(tx_data), &tx_data);

    tx_data = 0x03; // enable right playback channel, enable left playback channel
    _spi_command(fifo_fd, ADAU1761_REG_PLAYBACK_POWER_MANAGEMENT, sizeof(tx_data), &tx_data);

    tx_data = 0x03; // enable both DACs
    _spi_command(fifo_fd, ADAU1761_REG_DAC_CONTROL_0, sizeof(tx_data), &tx_data);

    tx_data = 0x21; // left mixer: right DAC muted, left DAC enabled, gain disable, mixer enabled
    _spi_command(fifo_fd, ADAU1761_REG_PLAY_MIXER_LEFT_0, sizeof(tx_data), &tx_data);
    tx_data = 0x41; // right mixer: right DAC enabled, left DAC muted, gain disable, mixer enabled
    _spi_command(fifo_fd, ADAU1761_REG_PLAY_MIXER_RIGHT_0, sizeof(tx_data), &tx_data);

    tx_data = 0x03; // enable headphone out
    _spi_command(fifo_fd, ADAU1761_REG_PLAYBACK_HEADPHONE_LEFT_VOL_CONTROL, sizeof(tx_data), &tx_data);
    _spi_command(fifo_fd, ADAU1761_REG_PLAYBACK_HEADPHONE_RIGHT_VOL_CONTROL, sizeof(tx_data), &tx_data);

    // set volume
    _set_volume(fifo_fd, volume);

    return 0;
}

void adau1761::init() {
    const array_t *channel_info;

    // open command FIFO
    _i2s_dma_state.cmd_fifo_fd = open("/dev/" CMD_FIFO_DEV_NAME, O_RDWR);
    if (_i2s_dma_state.cmd_fifo_fd < 0) {
        throw std::system_error(ENODEV, std::generic_category(),
                                "Failed to open command FIFO for device.");
    }

    // initialize DMA character device
    if (!(_i2s_dma_state.dev = axidma_init_dev(_i2s_dma_state.dev_index)))
        throw std::system_error(ENODEV, std::generic_category(),
                                "Failed to initialize DMA device");

    // get rx, tx channels of DMA
    channel_info = axidma_get_dma_tx(_i2s_dma_state.dev);
    if (channel_info->len < 1)
        throw std::system_error(ENODEV, std::generic_category(),
                                "Failed to get TX channel for DMA device");
    else
        _i2s_dma_state.tx_channel = channel_info->data[0];

    _init_codec(_i2s_dma_state.cmd_fifo_fd, _i2s_dma_state.sample_rate, _i2s_dma_state.volume);
}

void adau1761::destroy() {
    // free driver allocated continuous memories
    for (const auto& dma_memory_info : _i2s_dma_state.dma_memories) {
        // if there is an ongoing transfer of the memory to be freed, stop the transfer
        if (_i2s_dma_state.is_busy && _i2s_dma_state.current_transfer.tx_buffer == dma_memory_info.second)
            axidma_stop_transfer(_i2s_dma_state.dev, _i2s_dma_state.tx_channel);

        axidma_free(_i2s_dma_state.dev, dma_memory_info.second, dma_memory_info.first);
    }

    // destroy DMA character device
    axidma_destroy(_i2s_dma_state.dev);
}

size_t adau1761::lookup_buffer(void *buffer) const {
    auto it = std::find_if(_i2s_dma_state.dma_memories.cbegin(),
                           _i2s_dma_state.dma_memories.cend(),
                           [buffer](const auto & e){ return e.second == buffer; });

    if (it != _i2s_dma_state.dma_memories.cend())
        return it->first;

    return 0;
}

void *adau1761::request_buffer(size_t buffer_size) {
    if (buffer_size == 0)
        return nullptr;

    void * buff = axidma_malloc(_i2s_dma_state.dev, buffer_size);
    if (buff)
        _i2s_dma_state.dma_memories.emplace_back(buffer_size, buff);

    return buff;
}

int adau1761::release_buffer(void *buffer) {
    // lookup buffer and size in the dma_memories vector
    auto it = std::find_if(_i2s_dma_state.dma_memories.cbegin(),
                              _i2s_dma_state.dma_memories.cend(),
                              [buffer](const auto & e){ return e.second == buffer; });

    // if not found, return error
    if (it == _i2s_dma_state.dma_memories.cend())
        return -1;

    // if there is an ongoing transfer of the memory to be released, stop the transfer
    if (_i2s_dma_state.is_busy && _i2s_dma_state.current_transfer.tx_buffer == buffer)
        axidma_stop_transfer(_i2s_dma_state.dev, _i2s_dma_state.tx_channel);

    // free continuous memory and remove
    axidma_free(_i2s_dma_state.dev, it->second, it->first);
    _i2s_dma_state.dma_memories.erase(it);

    return 0;
}

int adau1761::play(void *buffer, int sample_rate, std::function<void(void *)> callback, void *callback_param) {
    // lookup buffer size in the dma_memories vector
    size_t buffer_size = lookup_buffer(buffer);

    // if not found, return error
    if (buffer_size == 0)
        return -1;

    // change sample rate if needed
    if (_i2s_dma_state.sample_rate != sample_rate) {
        if (_set_sample_rate(_i2s_dma_state.cmd_fifo_fd, sample_rate) != 0) {
            return -1;
        }

        _i2s_dma_state.sample_rate = sample_rate;
    }

    // stop any ongoing transfer
    axidma_stop_transfer(_i2s_dma_state.dev, _i2s_dma_state.tx_channel);

    // setup callback
    axidma_set_callback(_i2s_dma_state.dev, _i2s_dma_state.tx_channel, adau1761::_dma_callback, &_i2s_dma_state);

    // setup current transfer details
    _i2s_dma_state.is_busy = true;
    _i2s_dma_state.current_transfer.tx_buffer = buffer;
    _i2s_dma_state.current_transfer.user_callback = std::move(callback);
    _i2s_dma_state.current_transfer.callback_param = callback_param;

    // start
    axidma_oneway_transfer(_i2s_dma_state.dev, _i2s_dma_state.tx_channel, buffer, buffer_size, false);

    return 0;
}

uint8_t adau1761::set_relative_volume(int delta) {
    uint8_t vol = _i2s_dma_state.volume;
    if (delta > 0 && vol > MAX_VOLUME - delta) {
        // overflow: set to max volume
        vol = MAX_VOLUME;
    } else if (delta < 0 && vol < -delta) {
        // underflow: set to min volume
        vol = 0;
    } else {
        vol += delta;
    }

    _set_volume(_i2s_dma_state.cmd_fifo_fd, vol);
    _i2s_dma_state.volume = vol;

    return vol;
}

void adau1761::stop() {
    if (_i2s_dma_state.is_busy) {
        axidma_stop_transfer(_i2s_dma_state.dev, _i2s_dma_state.tx_channel);
        size_t size = lookup_buffer(_i2s_dma_state.current_transfer.tx_buffer);
        if (size > 0)
            axidma_free(_i2s_dma_state.dev, _i2s_dma_state.current_transfer.tx_buffer, size);
        _i2s_dma_state.is_busy = false;
    }
}

void adau1761::_dma_callback(int channel_id, void *data) {
    (void) channel_id;
    auto *dma_state = (adau1761::_dma_state_t *)data;
    void *tx_buff = nullptr;
    size_t tx_buff_size;

    // lookup tx buffer and size of the finished transfer in the dma_memories vector
    auto tx_it = std::find_if(dma_state->dma_memories.cbegin(),
                              dma_state->dma_memories.cend(),
                              [&dma_state](const auto & e){ return e.second == dma_state->current_transfer.tx_buffer; });
    if (tx_it != dma_state->dma_memories.cend()) {
        tx_buff_size = tx_it->first;
        tx_buff = tx_it->second;
    }

    // cleanup
    if (tx_buff) {
        axidma_free(dma_state->dev, tx_buff, tx_buff_size);
        dma_state->dma_memories.erase(tx_it);
    }

    dma_state->is_busy = false;

    // call user callback function
    if (dma_state->current_transfer.user_callback)
        std::invoke(dma_state->current_transfer.user_callback, dma_state->current_transfer.callback_param);
}
