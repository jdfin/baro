#include <cassert>
#include <cmath>
#include <cstring>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>
#include <unistd.h>
#include "ms5611.h"

using namespace std;

const char *dev_name = "/dev/spidev0.0";
constexpr unsigned spi_clk = 20000000;
constexpr unsigned log_interval_s = 1;

static double pressure_to_altitude(double p, double t)
{
    // http://keisan.casio.com/exec/system/1224585971
    double p0 = 1013.25; // pressure at sea level
    return ((pow(p0 / p, 1 / 5.257) - 1) * (t + 273.15)) / 0.0065;
}

static void show_csv_hdr()
{
    cout << "date, time, "
         << "adc_temp_dec, adc_temp_hex, adc_pres_dec, adc_pres_hex, "
         << "temp_c, pres_mbar, alt_m" << endl;
}

static void show_csv(const MS5611 &ms5611,
                     chrono::system_clock::time_point now_time,
                     uint32_t adc_temp, uint32_t adc_pres)
{
    int32_t temp, pres;
    ms5611.get_pressure(adc_temp, adc_pres, temp, pres);
    double alt = pressure_to_altitude(pres / 100.0, temp / 100.0);

    time_t t = chrono::system_clock::to_time_t(now_time);
    struct tm t_tm;
    localtime_r(&t, &t_tm);
    char t_str[80];
    memset(t_str, 0, sizeof(t_str));
    strftime(t_str, 79, "%F, %T, ", &t_tm);

    cout.setf(ios::fixed, ios::floatfield);
    cout.precision(2);
    cout << t_str << dec << setw(0) << adc_temp << ", " << hex << setw(8)
         << setfill('0') << adc_temp << ", " << dec << setw(0) << adc_pres
         << ", " << hex << setw(8) << setfill('0') << adc_pres << ", "
         << temp / 100.0 << ", " << pres / 100.0 << ", " << alt << endl;
}

static void usage(const char *prog_name)
{
    printf("usage: %s [-d] [-i N]\n", prog_name);
    printf("       -d       dump calibration parameters (no)\n");
    printf("       -i N     log interval, seconds (1)\n");
    exit(1);
}

int main(int argc, char *argv[])
{
    bool dump_cal = false;
    unsigned long interval_s = 1;

    int c;
    while ((c = getopt(argc, argv, "di:?")) != -1) {
        switch (c) {
        case 'd':
            dump_cal = true;
            break;
        case 'i':
            interval_s = strtoul(optarg, NULL, 0);
            if (interval_s == 0)
                usage(argv[0]);
            break;
        default:
            usage(argv[0]);
            break;
        }
    }

    auto next_time = chrono::system_clock::now();
    auto interval = chrono::seconds(interval_s);

    tzset();

    MS5611 ms5611(dev_name, spi_clk);

    if (dump_cal)
        ms5611.dump_prom();

    show_csv_hdr();

    while (true) {
        next_time += interval;
        this_thread::sleep_until(next_time);
        auto sleep_interval = chrono::milliseconds(10);
        // temperature
        if (ms5611.start_convert_temp()) {
            this_thread::sleep_for(sleep_interval);
            uint32_t adc_temp;
            if (ms5611.read_adc(adc_temp)) {
                // pressure
                if (ms5611.start_convert_pres()) {
                    this_thread::sleep_for(sleep_interval);
                    uint32_t adc_pres;
                    if (ms5611.read_adc(adc_pres)) {
                        show_csv(ms5611, next_time, adc_temp, adc_pres);
                    } else {
                        cerr << "read adc error (pressure)" << endl;
                    }
                } else {
                    cerr << "start convert error (pressure)" << endl;
                }
            } else {
                cerr << "read adc error (temperature)" << endl;
            }
        } else {
            cerr << "start convert error (temperature)" << endl;
        }
    }

    return 0;
}
