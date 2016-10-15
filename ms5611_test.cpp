
#include <cstdlib>
#include <iostream>
#include "gtest/gtest.h"
#include "ms5611.h"
#include "ms5611_test.h"

const char *correct_device = "/dev/spidev0.0";
const char *bogus_device = "/dev/no_such_device";
const char *wrong_device = "/dev/null";

TEST(ms5611, constructor)
{
    {
        MS5611 m(bogus_device, 1000000, 0);
        ASSERT_FALSE(MS5611Test::is_ready(m));
    }

    {
        MS5611 m(wrong_device, 1000000, 0);
        ASSERT_FALSE(MS5611Test::is_ready(m));
    }

    {
        MS5611 m(correct_device);
        ASSERT_TRUE(MS5611Test::is_ready(m));
    }

    {
        MS5611 m(correct_device, 0, 0);
        ASSERT_FALSE(MS5611Test::is_ready(m));
    }

    {
        MS5611 m(correct_device, 50000000, 0);
        ASSERT_FALSE(MS5611Test::is_ready(m));
    }

    {
        MS5611 m(correct_device, 20000000);
        ASSERT_TRUE(MS5611Test::is_ready(m));
    }
}

TEST(ms5611, reset)
{
    MS5611 m(correct_device, 20000000);
    ASSERT_TRUE(MS5611Test::is_ready(m));
    uint16_t &c1 = MS5611Test::c(m, 1);
    uint16_t &c7 = MS5611Test::c(m, 7);
    ASSERT_EQ(MS5611Test::crc4(m), c7 & 0x000f);
    c1 ^= 0x0100; // flip a bit
    ASSERT_NE(MS5611Test::crc4(m), c7 & 0x000f);
    ASSERT_TRUE(MS5611Test::read_cal(m));
    ASSERT_EQ(MS5611Test::crc4(m), c7 & 0x000f);
}

TEST(ms5611, read_adc)
{
    uint32_t data;
    MS5611 m(correct_device, 20000000);

    // don't check output of first read in case there's a convert already done
    ASSERT_TRUE(m.read_adc(data));

    // next should return zero since there was no convert
    ASSERT_TRUE(m.read_adc(data));
    ASSERT_EQ(data, 0);
}

TEST(ms5611, start_convert)
{
    MS5611 m(correct_device, 20000000, 0);
    for (int cmd = 0; cmd < 256; cmd++) {
        bool result = MS5611Test::start_convert(m, uint8_t(cmd));
        if (cmd == 0x40 || cmd == 0x42 || cmd == 0x44 || cmd == 0x46 ||
            cmd == 0x48 || cmd == 0x50 || cmd == 0x52 || cmd == 0x54 ||
            cmd == 0x56 || cmd == 0x58) {
            ASSERT_TRUE(result);
            // wait for done; all conversions are done within 10 msec
            usleep(10000);
            uint32_t data;
            ASSERT_TRUE(m.read_adc(data));
            ASSERT_NE(data, 0);
        } else {
            ASSERT_FALSE(result);
        }
    }
}

TEST(ms5611, do_convert)
{
    MS5611 m(correct_device, 20000000, 0);
    for (int cmd = 0; cmd < 256; cmd++) {
        uint32_t data = 0;
        bool result = MS5611Test::do_convert(m, uint8_t(cmd), data);
        if (cmd == 0x40 || cmd == 0x42 || cmd == 0x44 || cmd == 0x46 ||
            cmd == 0x48 || cmd == 0x50 || cmd == 0x52 || cmd == 0x54 ||
            cmd == 0x56 || cmd == 0x58) {
            ASSERT_TRUE(result);
            ASSERT_NE(data, 0);
        } else {
            ASSERT_FALSE(result);
        }
    }
}

TEST(ms5611, get_pressure)
{
    // check at several SPI clock rates
    // (this has been observed to fail at 100,000 Hz)
    unsigned spi_clks[] = {500000, 1000000, 5000000, 10000000, 20000000};
    // check at all oversampling values
    MS5611::Osr oversamps[] = {MS5611::OSR256, MS5611::OSR512, MS5611::OSR1024,
                               MS5611::OSR2048, MS5611::OSR4096};
    // temperature and pressure should be "sane" (within reasonable lab
    // values) and close to the same at the different oversampling values
    int32_t temp_first;
    int32_t pres_first;
    int temp_diff_max = 0;
    int pres_diff_max = 0;
    for (int clk = 0; clk < sizeof(spi_clks) / sizeof(spi_clks[0]); clk++) {
        unsigned spi_clk = spi_clks[clk];
        MS5611 m(correct_device, spi_clk);
        // std::cout << "clk=" << spi_clk << std::endl;
        for (int over = 0; over < sizeof(oversamps) / sizeof(oversamps[0]);
             over++) {
            MS5611::Osr oversamp = oversamps[over];
            // std::cout << "oversamp=" << int(oversamp) << std::endl;
            // temperature
            uint32_t temp_adc;
            ASSERT_TRUE(m.do_convert_temp(temp_adc, oversamp));
            // pressure
            uint32_t pres_adc;
            ASSERT_TRUE(m.do_convert_pres(pres_adc, oversamp));
            // convert to reality
            int32_t temp_x100;
            int32_t pres_x100;
            ASSERT_TRUE(
                m.get_pressure(temp_adc, pres_adc, temp_x100, pres_x100));
            // check: temperature is 0..50 C
            ASSERT_GT(temp_x100, 0);
            ASSERT_LT(temp_x100, 5000);
            // check: pressure is 700..1100 mbar (1100 is about -2500ft, 700 is
            // about 10000ft)
            ASSERT_GT(pres_x100, 70000);
            ASSERT_LT(pres_x100, 110000);
            // std::cout << "temp_adc=0x" << std::hex << temp_adc
            //           << " pres_adc=0x" << std::hex<< pres_adc
            //           << " temp_x100=" << std::dec << temp_x100
            //           << " pres_x100=" << pres_x100 << std::endl;
            if (clk == 0 && over == 0) {
                // first measurement
                temp_first = temp_x100;
                pres_first = pres_x100;
            } else {
                // compare to first measurement
                // check: temperatures are all within 1C of the first one
                int temp_diff = abs(temp_first - temp_x100);
                if (temp_diff_max < temp_diff)
                    temp_diff_max = temp_diff;
                ASSERT_LT(temp_diff, 100);
                // check: pressures are all within 10 mbar of the first one
                int pres_diff = abs(pres_first - pres_x100);
                if (pres_diff_max < pres_diff)
                    pres_diff_max = pres_diff;
                ASSERT_LT(pres_diff, 1000);
            }
        }
    }
    // A handful of runs shows the max temp diff is usually < 0.1C,
    // and the max pressure diff is usually < 2 mbar.
    // std::cout << "temp_diff_max=" << temp_diff_max
    //           << " pres_diff_max=" << pres_diff_max << std::endl;
    // No runs have shown the max diff to be zero, but perhaps it could
    EXPECT_GT(temp_diff_max, 0);
    EXPECT_GT(pres_diff_max, 0);
}

int main(int argc, char *argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
