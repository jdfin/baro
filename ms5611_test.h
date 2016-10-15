
#include <cstdint>
#include "ms5611.h"

class MS5611Test
{
public:
    static bool is_ready(const MS5611 &m)
    {
        return m._fd > 0;
    }
    static bool reset(MS5611 &m)
    {
        return m._reset();
    }
    static bool read_cal(MS5611 &m)
    {
        return m._read_cal();
    }
    static uint8_t crc4(MS5611 &m)
    {
        return m._crc4();
    }
    static bool start_convert(MS5611 &m, uint8_t cmd)
    {
        return m._start_convert(cmd);
    }
    static bool do_convert(MS5611 &m, uint8_t cmd, uint32_t &data)
    {
        return m._do_convert(cmd, data);
    }
    static uint16_t &c(MS5611 &m, int n)
    {
        return m._c[n];
    }
};
