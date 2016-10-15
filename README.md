# baro - MS5611 demo and test programs

This has been run with the [MS5611](http://www.amsys.info/products/ms5611.htm)
as implemented on the Erle Robotics
[PXFmini](http://erlerobotics.com/blog/pxfmini/) connected to a
[Raspberry Pi 3](https://www.raspberrypi.org/products/raspberry-pi-3-model-b/).
It has only been run on a desktop setup.

There's a temperature/pressure logger (csv output) and a test program.

## install

Copy all code to your Raspberry Pi 3 and `make` it. You'll probably need to
install some extra packages, including
[google test](https://github.com/google/googletest), and maybe adjust the path
to google test in the Makefile. Enable SPI on the Raspberry Pi.

## hardware

The setup tested is a Raspberry Pi 3 connected to the PXFmini by a ribbon
cable.

## run

```
pi@raspberrypi:~/projects/baro $ ./ms5611_log -?
usage: ./ms5611_log [-d] [-i N]
       -d       dump calibration parameters (no)
       -i N     log interval, seconds (1)
pi@raspberrypi:~/projects/baro $
```

```
pi@raspberrypi:~/projects/baro $ ./ms5611_log -i 5
date, time, adc_temp_dec, adc_temp_hex, adc_pres_dec, adc_pres_hex, temp_c, pres_mbar, alt_m
2016-10-14, 21:14:30, 8357308, 007f85bc, 8413336, 00806098, 25.11, 998.63, 127.04
2016-10-14, 21:14:35, 8357310, 007f85be, 8413328, 00806090, 25.11, 998.63, 127.04
2016-10-14, 21:14:40, 8357258, 007f858a, 8413268, 00806054, 25.11, 998.61, 127.21
2016-10-14, 21:14:45, 8357332, 007f85d4, 8413304, 00806078, 25.12, 998.62, 127.13
^C
pi@raspberrypi:~/projects/baro $
```

```
pi@raspberrypi:~/projects/baro $ ./ms5611_test
[==========] Running 6 tests from 1 test case.
[----------] Global test environment set-up.
[----------] 6 tests from ms5611
[ RUN      ] ms5611.constructor
[       OK ] ms5611.constructor (7 ms)
[ RUN      ] ms5611.reset
[       OK ] ms5611.reset (4 ms)
[ RUN      ] ms5611.read_adc
[       OK ] ms5611.read_adc (3 ms)
[ RUN      ] ms5611.start_convert
[       OK ] ms5611.start_convert (105 ms)
[ RUN      ] ms5611.do_convert
[       OK ] ms5611.do_convert (41 ms)
[ RUN      ] ms5611.get_pressure
[       OK ] ms5611.get_pressure (206 ms)
[----------] 6 tests from ms5611 (366 ms total)

[----------] Global test environment tear-down
[==========] 6 tests from 1 test case ran. (366 ms total)
[  PASSED  ] 6 tests.
pi@raspberrypi:~/projects/baro $
```

## notes

Mine seems to always report a temperature about 1 - 2C below what other
thermometers say.

The altitude output in the log assumes a sea level pressure of 1013.25, so
it's rarely correct. See ms5611_log.cpp for the formula used to calculate
altitude from pressure.

You probably need to set TZ in your ~/.profile to get the local time to
print correctly:
```
export TZ='America/Los_Angeles'
```

The makefile looks for environment variables `GMOCK_ROOT` and `GTEST_ROOT`;
I set them in ~/.profile like this:
```
export GMOCK_ROOT=~/projects/googletest/googlemock
export GTEST_ROOT=~/projects/googletest/googletest
```
