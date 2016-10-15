#pragma once

#include <string>

class MS5611
{

public:
    enum Osr {
        OSR256 = 0,
        OSR512 = 2,
        OSR1024 = 4,
        OSR2048 = 6,
        OSR4096 = 8,
    };

    // verbosity: 0 - nothing, not even error messages
    //            1 - error messages (default)
    //            2 - extra debug messages
    MS5611(const std::string &dev_name, unsigned spi_clk = 1000000,
           int verbosity = 1);

    virtual ~MS5611();

    bool start_convert_temp(Osr oversamp = OSR4096)
    {
        return _start_convert(TEMP | oversamp);
    }

    bool start_convert_pres(Osr oversamp = OSR4096)
    {
        return _start_convert(PRES | oversamp);
    }

    bool do_convert_temp(uint32_t &temp, Osr oversamp = OSR4096)
    {
        return _do_convert(TEMP | oversamp, temp);
    }

    bool do_convert_pres(uint32_t &pres, Osr oversamp = OSR4096)
    {
        return _do_convert(PRES | oversamp, pres);
    }

    bool read_adc(uint32_t &data);

    bool get_pressure(uint32_t temp_adc, uint32_t pres_adc, int32_t &temp_x100,
                      int32_t &pres_x100) const;

    void dump_prom();

private:
    enum Convert { TEMP = 0x40, PRES = 0x50 };

    std::string _dev_name;
    int _fd;
    uint16_t _c[8];
    int _verbosity;

    bool _reset();
    bool _read_cal_word(int n, uint16_t &data);
    bool _read_cal();
    uint8_t _crc4();
    bool _start_convert(uint8_t cmd);
    bool _do_convert(uint8_t cmd, uint32_t &data);

    friend class MS5611Test;
};
