#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <string>
#include "ms5611.h"

using namespace std;

// correct for gcc and a few other (but not all) compilers
#define FUNC_NAME __PRETTY_FUNCTION__

// create device
//
// open the SPI device
// configure SPI
// reset the chip
// read cal data
MS5611::MS5611(const string &dev_name, unsigned spi_clk, int verbosity)
    : _dev_name(dev_name), _fd(-1), _verbosity(verbosity)
{
    if (_verbosity > 1)
        cout << FUNC_NAME << ": " << dev_name << ", " << spi_clk << ", "
             << verbosity << endl;

    if (spi_clk == 0 || spi_clk > 20000000) {
        if (_verbosity > 0)
            cerr << FUNC_NAME << " ERROR: spi_clk=" << spi_clk << " invalid"
                 << endl;
        return;
    }

    // open spi device
    _fd = open(_dev_name.c_str(), O_RDWR);
    if (_fd < 0) {
        if (_verbosity > 0)
            cerr << FUNC_NAME << " ERROR: opening " << _dev_name << endl;
        return;
    }

    // Configure spi bus. The chip should work in either mode 0 or mode 3;
    // most of the waveforms in the data sheet look like mode 0 so use that.
    uint8_t mode = SPI_MODE_0;
    uint8_t bits = 8;
    uint32_t clk = spi_clk;
    if (ioctl(_fd, SPI_IOC_WR_MODE, &mode) < 0 ||
        ioctl(_fd, SPI_IOC_RD_MODE, &mode) < 0 ||
        ioctl(_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(_fd, SPI_IOC_RD_BITS_PER_WORD, &bits) < 0 ||
        ioctl(_fd, SPI_IOC_WR_MAX_SPEED_HZ, &clk) < 0 ||
        ioctl(_fd, SPI_IOC_RD_MAX_SPEED_HZ, &clk) < 0) {
        if (_verbosity > 0)
            cerr << FUNC_NAME << " ERROR: initializing " << _dev_name << endl;
        close(_fd);
        _fd = -1;
        return;
    }

    // reset chip
    if (!_reset()) {
        // error message already printed
        close(_fd);
        _fd = -1;
        return;
    }

    // read calibration data
    if (!_read_cal()) {
        if (_verbosity > 0)
            cerr << FUNC_NAME << " ERROR: reading calibration data" << endl;
        close(_fd);
        _fd = -1;
        return;
    }

    // check crc of cal data
    if ((_c[7] & 0x000f) != _crc4()) {
        if (_verbosity > 0)
            cerr << FUNC_NAME << " ERROR: calibration data CRC" << endl;
        close(_fd);
        _fd = -1;
        return;
    }
}

MS5611::~MS5611()
{
    if (_verbosity > 1)
        cout << FUNC_NAME << endl;

    if (_fd < 0)
        return;

    close(_fd);
    _fd = -1;
}

// reset chip
//
// send the reset command
// clock in a byte
// wait 3 msec
// clock in another byte
//
// The chip holds SDO low while resetting, then sets it high. We read a byte
// immediately after the reset command to see that it is low, then another
// one when the reset is complete to see that it is high.
bool MS5611::_reset()
{
    if (_fd < 0) {
        if (_verbosity > 0)
            cerr << FUNC_NAME << " ERROR: device not ready" << endl;
        return false;
    }

    struct spi_ioc_transfer spi_cmd[3];
    memset(spi_cmd, 0, sizeof(spi_cmd));

    uint8_t tx_data[1] = {0x1e}; // reset
    spi_cmd[0].tx_buf = uint64_t(&tx_data[0]);
    spi_cmd[0].len = 1;

    // first byte read should be all zeros
    uint8_t rx_data_0[1] = {0};
    spi_cmd[1].rx_buf = uint64_t(&rx_data_0[0]);
    spi_cmd[1].len = 1;
    spi_cmd[1].delay_usecs = 3000; // 2.8 msec according to data sheet

    // a byte read after the reset should be all ones
    uint8_t rx_data_1[1] = {0};
    spi_cmd[2].rx_buf = uint64_t(&rx_data_1[0]);
    spi_cmd[2].len = 1;

    if (ioctl(_fd, SPI_IOC_MESSAGE(3), spi_cmd) < 0) {
        if (_verbosity > 0)
            cerr << FUNC_NAME << " ERROR: issuing command" << endl;
        return false;
    }

    if (rx_data_0[0] != 0 || rx_data_1[0] != 0xff) {
        if (_verbosity > 0)
            cerr << FUNC_NAME << " ERROR: resetting chip ("
                 << "0x" << hex << setw(2) << setfill('0') << int(rx_data_0[0])
                 << ", 0x" << hex << setw(2) << setfill('0')
                 << int(rx_data_1[0]) << ")" << endl
                 << dec << setw(0) << setfill(' ');
        return false;
    }

    return true;
}

// read one calibration word
bool MS5611::_read_cal_word(int n, uint16_t &data)
{
    if (_fd < 0) {
        if (_verbosity > 0)
            cerr << FUNC_NAME << " ERROR: device not ready" << endl;
        return false;
    }

    if (n >= 8) {
        if (_verbosity > 0)
            cerr << FUNC_NAME << " ERROR: n=" << n << " invalid" << endl;
        return false;
    }

    struct spi_ioc_transfer spi_cmd[2];
    memset(spi_cmd, 0, sizeof(spi_cmd));

    uint8_t cmd = 0xa0 + n * 2;
    uint8_t tx_data[1] = {cmd};
    spi_cmd[0].tx_buf = uint64_t(&tx_data[0]);
    spi_cmd[0].len = 1;

    uint8_t rx_data[2] = {0, 0};
    spi_cmd[1].rx_buf = uint64_t(&rx_data[0]);
    spi_cmd[1].len = 2;

    if (ioctl(_fd, SPI_IOC_MESSAGE(2), spi_cmd) < 0) {
        if (_verbosity > 0)
            cerr << FUNC_NAME << " ERROR: issuing command" << endl;
        return false;
    }

    data = (uint16_t(rx_data[0]) << 8) | uint16_t(rx_data[1]);

    return true;
}

// read all calibration words into _c[]
bool MS5611::_read_cal()
{
    for (int n = 0; n < 8; n++)
        if (!_read_cal_word(n, _c[n]))
            // error message already printed
            return false;

    // crc is expected to be checked elsewhere

    return true;
}

// calculate crc4 over calibration words
// based on http://www.amsys.info/sheets/amsys.en.an520_e.pdf
uint8_t MS5611::_crc4()
{
    uint16_t rem = 0;
    uint16_t c7_save = _c[7];
    _c[7] &= 0xff00;
    for (int byte = 0; byte < 16; byte++) {
        if (byte % 2 == 1)
            rem ^= _c[byte >> 1] & 0x00ff;
        else
            rem ^= _c[byte >> 1] >> 8;
        for (int bit = 0; bit < 8; bit++) {
            if (rem & 0x8000)
                rem = (rem << 1) ^ 0x3000;
            else
                rem = (rem << 1);
        }
    }
    _c[7] = c7_save;
    return rem >> 12;
}

// start a conversion
//
// cmd is one of the "convert" commands from the datasheet
bool MS5611::_start_convert(uint8_t cmd)
{
    if (_fd < 0) {
        if (_verbosity > 0)
            cerr << FUNC_NAME << " ERROR: device not ready" << endl;
        return false;
    }

    // 0x40, 0x42, 0x44, 0x46, 0x48, 0x50, 0x52, 0x54, 0x56, 0x58
    if ((cmd & 0xe1) != 0x40 || (cmd & 0x0e) > 0x08) {
        if (_verbosity > 0)
            cerr << FUNC_NAME << " ERROR: cmd="
                 << "0x" << hex << setw(2) << setfill('0') << int(cmd)
                 << " invalid" << endl
                 << dec << setw(0) << setfill(' ');
        return false;
    }

    struct spi_ioc_transfer spi_cmd[1];
    memset(spi_cmd, 0, sizeof(spi_cmd));

    uint8_t tx_data[1] = {cmd};
    spi_cmd[0].tx_buf = uint64_t(&tx_data[0]);
    spi_cmd[0].len = 1;

    if (ioctl(_fd, SPI_IOC_MESSAGE(1), spi_cmd) < 0) {
        if (_verbosity > 0)
            cerr << FUNC_NAME << " ERROR: issuing command" << endl;
        return false;
    }

    return true;
}

// read adc result
bool MS5611::read_adc(uint32_t &data)
{
    if (_fd < 0) {
        if (_verbosity > 0)
            cerr << FUNC_NAME << " ERROR: device not ready" << endl;
        return false;
    }

    struct spi_ioc_transfer spi_cmd[2];
    memset(spi_cmd, 0, sizeof(spi_cmd));

    uint8_t tx_data[1] = {0x00};
    spi_cmd[0].tx_buf = uint64_t(&tx_data[0]);
    spi_cmd[0].len = 1;

    uint8_t rx_data[3] = {0, 0, 0};
    spi_cmd[1].rx_buf = uint64_t(&rx_data[0]);
    spi_cmd[1].len = 3;

    if (ioctl(_fd, SPI_IOC_MESSAGE(2), spi_cmd) < 0) {
        if (_verbosity > 0)
            cerr << FUNC_NAME << " ERROR: issuing command" << endl;
        return false;
    }

    data = (uint32_t(rx_data[0]) << 16) | (uint32_t(rx_data[1]) << 8) |
           uint32_t(rx_data[2]);

    return true;
}

// request conversion, wait, read adc
// this function blocks for the duration of the conversion
bool MS5611::_do_convert(uint8_t cmd, uint32_t &data)
{
    if (_fd < 0) {
        if (_verbosity > 0)
            cerr << FUNC_NAME << " ERROR: device not ready" << endl;
        return false;
    }

    // 0x40, 0x42, 0x44, 0x46, 0x48, 0x50, 0x52, 0x54, 0x56, 0x58
    if ((cmd & 0xe1) != 0x40 || (cmd & 0x0e) > 0x08) {
        if (_verbosity > 0)
            cerr << FUNC_NAME << " ERROR: cmd="
                 << "0x" << hex << setw(2) << setfill('0') << int(cmd)
                 << " invalid" << endl
                 << dec << setw(0) << setfill(' ');
        return false;
    }

    unsigned osr_code = (cmd >> 1) & 0x07; // 0, 1, 2, 3, 4
    unsigned usec_delay = 600 << osr_code; // 600, 1200, 2400, 4800, 9600
    // cout << "osr_code=" << osr_code << " usec_delay=" << usec_delay << endl;

    struct spi_ioc_transfer spi_cmd[3];
    memset(spi_cmd, 0, sizeof(spi_cmd));

    uint8_t tx_convert[1] = {cmd};
    spi_cmd[0].tx_buf = uint64_t(&tx_convert[0]);
    spi_cmd[0].len = 1;
    spi_cmd[0].delay_usecs = usec_delay;
    spi_cmd[0].cs_change = true;

    uint8_t tx_read[1] = {0x00};
    spi_cmd[1].tx_buf = uint64_t(&tx_read[0]);
    spi_cmd[1].len = 1;

    uint8_t rx_data[3] = {0, 0, 0};
    spi_cmd[2].rx_buf = uint64_t(&rx_data[0]);
    spi_cmd[2].len = 3;

    // auto t1 = chrono::system_clock::now();

    if (ioctl(_fd, SPI_IOC_MESSAGE(3), spi_cmd) < 0) {
        if (_verbosity > 0)
            cerr << FUNC_NAME << " ERROR: issuing command" << endl;
        return false;
    }

    // auto t2 = chrono::system_clock::now();
    // cout << "ioctl took "
    //     << chrono::duration_cast<chrono::microseconds>(t2 - t1).count()
    //     << " usec" << endl;

    data = (uint32_t(rx_data[0]) << 16) | (uint32_t(rx_data[1]) << 8) |
           uint32_t(rx_data[2]);

    return true;
}

// get pressure and temperature
bool MS5611::get_pressure(uint32_t temp_adc, uint32_t pres_adc,
                          int32_t &temp_x100, int32_t &pres_x100) const
{
    uint32_t d1 = pres_adc;
    uint32_t d2 = temp_adc;

    // asserts are based on numerical ranges (not sanity)

    assert(_c[1] < (1 << 16));
    assert(_c[2] < (1 << 16));
    assert(_c[3] < (1 << 16));
    assert(_c[4] < (1 << 16));
    assert(_c[5] < (1 << 16));
    assert(_c[6] < (1 << 16));

    assert(d1 < (1 << 24));
    assert(d2 < (1 << 24));

    // do most calculations with 64 bits to prevent overflow

    int64_t c1 = _c[1];
    int64_t c2 = _c[2];
    int64_t c3 = _c[3];
    int64_t c4 = _c[4];
    int64_t c5 = _c[5];
    int64_t c6 = _c[6];

    // calculate temperature

    int32_t dT = d2 - c5 * 256;
    int32_t temp = 2000 + dT * c6 / (1 << 23);
    // validate range according to part's spec
    if (temp < -4000 || temp > 8500) {
        if (_verbosity > 0)
            cerr << FUNC_NAME << " ERROR: temperature " << temp
                 << " out of range" << endl;
        return false;
    }

    // second-order temperature adjustments
    int64_t t2 = 0;
    int64_t off2 = 0;
    int64_t sens2 = 0;
    int32_t t_lo = temp - 2000;
    if (t_lo < 0) {
        t2 = dT * dT / (1 << 31);
        off2 = 5 * t_lo * t_lo / 2;
        sens2 = off2 / 2;
        t_lo = temp - -1500;
        if (t_lo < 0) {
            off2 += (7 * t_lo * t_lo);
            sens2 += (11 * t_lo * t_lo / 2);
        }
    }

    temp_x100 = temp - t2;

    // calculate pressure
    int64_t off = c2 * (1 << 16) + (c4 * dT) / (1 << 7) - off2;
    int64_t sens = c1 * (1 << 15) + (c3 * dT) / (1 << 8) - sens2;
    pres_x100 = (d1 * sens / (1 << 21) - off) / (1 << 15);

    return true;
}

void MS5611::dump_prom()
{
    if (_verbosity > 0)
        for (int i = 0; i < 8; i++)
            cout << "prom[" << i << "] = " << _c[i] << endl;
}
