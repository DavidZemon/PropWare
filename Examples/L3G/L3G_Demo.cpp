/**
 * @file    L3G_Demo.c
 *
 * @project L3G_Demo
 *
 * @author  David Zemon
 *
 * @copyright
 * The MIT License (MIT)<br>
 * <br>Copyright (c) 2013 David Zemon<br>
 * <br>Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:<br>
 * <br>The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.<br>
 * <br>THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// Includes
#include <tinyio.h>
#include "L3G_Demo.h"

// Main function
int main () {
    int8_t err;
    int16_t gyroVals[3];
    PropWare::L3G gyro;

    if ((err = gyro.start(MOSI, MISO, SCLK, CS, PropWare::L3G::DPS_2000)))
        error(err);

    // Though this functional call is not necessary (default value is 0), I
    // want to bring attention to this function. It will determine whether the
    // l3g_read* functions will always explicitly set the SPI modes before
    // each call, or assume that the SPI cog is still running in the proper
    // configuration
    gyro.always_set_spi_mode(1);

    while (1) {
        if ((err = gyro.read_all(gyroVals)))
            error(err);
        printf("Gyro vals... X: %i\tY: %i\tZ: %i\n", gyroVals[0], gyroVals[1],
                gyroVals[2]);
        waitcnt(CLKFREQ/20 + CNT);
    }

    return 0;
}

void error (const int8_t err) {
    uint32_t shiftedValue = (uint8_t) err;

    // Shift the error bits by 16 to put them atop the QUICKSTART LEDs
    shiftedValue <<= 16;

    // Set the Quickstart LEDs for output (used to display the error code)
    PropWare::GPIO::set_dir(DEBUG_LEDS, PropWare::GPIO::OUT);

    while (1) {
        PropWare::GPIO::pin_write(DEBUG_LEDS, shiftedValue);
        waitcnt(CLKFREQ/5 + CNT);
        PropWare::GPIO::pin_clear(DEBUG_LEDS);
        waitcnt(CLKFREQ/5 + CNT);
    }
}