/**
 * @file    ws2812.h
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

#pragma once

#include <PropWare/PropWare.h>
#include <PropWare/pin.h>

namespace PropWare {

class WS2812 {

    public:
        typedef enum {
                                          RGB,
            /** for WS2812 and WS2812B */ GRB
        } Type;

        typedef enum {
            BLACK      = 0x000000,
            RED        = 0xFF0000,
            GREEN      = 0x00FF00,
            BLUE       = 0x0000FF,
            WHITE      = 0xFFFFFF,
            CYAN       = 0x00FFFF,
            MAGENTA    = 0xFF00FF,
            YELLOW     = 0xFFFF00,
            CHARTREUSE = 0x7FFF00,
            ORANGE     = 0xFF6000,
            AQUAMARINE = 0x7FFFD4,
            PINK       = 0xFF5F5F,
            TURQUOISE  = 0x3FE0C0,
            REALWHITE  = 0xC8FFFF,
            INDIGO     = 0x3F007F,
            VIOLET     = 0xBF7FBF,
            MAROON     = 0x320010,
            BROWN      = 0x0E0600,
            CRIMSON    = 0xDC283C
        } Color;

    public:
        static unsigned int color (const unsigned int red, const unsigned int green, const unsigned int blue) {
            return ((red) << 16) | ((green) << 8) | (blue);
        }

        static unsigned int scale (const unsigned int x, const unsigned int l) {
            return x * l / 255;
        }

        static unsigned int colorx (const unsigned int red, const unsigned int green, const unsigned int blue,
                                    const unsigned int l) {
            return (scale(red, l) << 16) | (scale(green, l) << 8) | scale(blue, l);
        }

    public:
        WS2812 (const Pin::Mask pinMask, const Type type) : m_type(type) {
            this->m_pin.set_mask(pinMask);
            this->m_pin.clear();
            this->m_pin.set_dir_out();
        }

        void send (const unsigned int color) const {
            this->send_array(&color, 1);
        }

        void send_array (const unsigned int *buffer, const size_t length) const {
            // Using local variables here saves 50 bytes relative to static class variables and even more compared to
            // local statics. And bonus - no more ws2812.cpp needed, so importing becomes a lot easier
            const unsigned int LONG_PULSE_WIDTH   = (900 * MICROSECOND / 1000);
            const unsigned int SHORT_PULSE_WIDTH  = (350 * MICROSECOND / 1000);
            const unsigned int RESET_DELAY        = (50 * MICROSECOND);

            unsigned int clock      = RESET_DELAY;
            unsigned int t1         = 0;
            unsigned int t2         = 0;
            unsigned int colorbits  = 0;
            unsigned int bitCounter = 0;

            __asm__ volatile (
                    "           fcache #(Ws2812End - Ws2812Start)                                                       \n\t"
                    "           .compress off                                                                   \n\t"
                    "Ws2812Start:                                                                               \n\t"

                    "           add     %[_clock], CNT                                                          \n\t"
                    "           waitcnt %[_clock], #0                                                           \n\t"

                    // NOTE: This assembly snippet adapted from Jon "JonnyMac" McPhalen's WS2812 driver
                    "frame_loop%=:                                                                              \n\t"
                    "		rdlong	%[_colorbits], %[_nextLed]                                              \n\t"
                    "		add	%[_nextLed], #4                                                         \n\t"

                    "fix_colors%=:                                                                              \n\t"
                    "		tjz	%[_swaprg], #__LMM_FCACHE_START+(shift_out%= - Ws2812Start)             \n\t"
                    "		mov	%[_t1], %[_colorbits]                                                   \n\t"
                    "		mov	%[_t2], %[_colorbits]                                                   \n\t"
                    "		and	%[_colorbits], #0xff                                                    \n\t"
                    "		shr	%[_t1], #8                                                              \n\t"
                    "		and	%[_t1], %[_byte1]                                                       \n\t"
                    "		or	%[_colorbits], %[_t1]                                                   \n\t"
                    "		shl	%[_t2], #8                                                              \n\t"
                    "		and	%[_t2], %[_byte2]                                                       \n\t"
                    "		or	%[_colorbits], %[_t2]                                                   \n\t"

                    "shift_out%=:                                                                               \n\t"
                    "		shl	%[_colorbits], #8                                                       \n\t"
                    "		mov	%[_bitCounter], #24                                                     \n\t"

                    "shift_out.loop%=:                                                                          \n\t"
                    "		rcl	%[_colorbits], #1	wc                                              \n\t"
                    "	if_c 	mov	%[_clock], %[_longPulse]  ' bit1hi                                      \n\t"
                    "	if_nc 	mov	%[_clock], %[_shortPulse] ' bit0hi                                      \n\t"
                    "		or	OUTA, %[_pinMask]                                                       \n\t"
                    "		add	%[_clock], CNT                                                          \n\t"
                    "	if_c 	waitcnt	%[_clock], %[_shortPulse] ' bit1lo                                      \n\t"
                    "	if_nc 	waitcnt	%[_clock], %[_longPulse]  ' bit0lo                                      \n\t"
                    "		andn	OUTA, %[_pinMask]                                                       \n\t"
                    "		waitcnt	%[_clock], #0                                                           \n\t"
                    "		djnz	%[_bitCounter], #__LMM_FCACHE_START+(shift_out.loop%= - Ws2812Start)    \n\t"
                    "		djnz	%[_nleds], #__LMM_FCACHE_START+(frame_loop%= - Ws2812Start)             \n\t"

                    "           jmp __LMM_RET                                                                   \n\t"
                    "Ws2812End:                                                                                 \n\t"
                    "           .compress default                                                               \n\t"
            : [_clock] "+r"(clock),
            [_t1] "+r"(t1),
            [_t2] "+r"(t2),
            [_colorbits] "+r"(colorbits),
            [_bitCounter] "+r"(bitCounter),
            [_nextLed] "+r"(buffer)
            : [_pinMask] "r"(this->m_pin.get_mask()),
            [_nleds] "r"(length),
            [_swaprg] "r"(this->m_type),
            [_byte1] "r"(BYTE_1),
            [_byte2] "r"(BYTE_2),
            [_shortPulse] "r"(SHORT_PULSE_WIDTH),
            [_longPulse] "r"(LONG_PULSE_WIDTH),
            [_resetDelay] "r"(RESET_DELAY));
        }

        uint32_t wheel (unsigned int position) {
            uint32_t resultingColor;

            // Creates color from 0 to 255 position input
            // -- colors transition r-g-b back to r

            // red range
            if (position < 85)
                resultingColor = color(255 - position * 3, position * 3, 0);

                // green range
            else if (position < 170) {
                position -= 85;
                resultingColor = color(0, 255 - position * 3, position * 3);
            }
                // blue range
            else {
                position -= 170;
                resultingColor = color(position * 3, 0, 255 - position * 3);
            }

            return resultingColor;
        }

        uint32_t wheel_dim (unsigned int position, unsigned int brightness) {
            uint32_t color;

            // Creates color from 0 to 255 position input
            // -- colors transition r-g-b back to r

            // red range
            if (position < 85)
                color = colorx(255 - position * 3, position * 3, 0, brightness);

                // green range
            else if (position < 170) {
                position -= 85;
                color = colorx(0, 255 - position * 3, position * 3, brightness);
            }

                // blue range
            else {
                position -= 170;
                color = colorx(position * 3, 0, 255 - position * 3, brightness);
            }

            return color;
        }

        Type get_type () const {
            return this->m_type;
        }

    private:
        Pin        m_pin;
        const Type m_type;
};

}