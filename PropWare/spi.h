/**
 * @file        spi.h
 *
 * @project     PropWare
 *
 * @author      David Zemon
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

#ifndef PROPWARE_SPI_H_
#define PROPWARE_SPI_H_

#include <propeller.h>
#include <stdlib.h>
#include <stdarg.h>
#include <PropWare/PropWare.h>
#include <PropWare/pin.h>

/** @name   SPI Extra Code Options
 * @{ */
// This allows Doxygen to document the macro without permanently enabling it
//#undef SPI_OPTION_DEBUG
/**
 * Parameter checking within each function call. I recommend you leave this
 * option enabled unless speed is critical
 * <p>
 * DEFAULT: On
 */
#define SPI_OPTION_DEBUG_PARAMS
/**
 * Allows for fast send and receive routines without error checking or timing
 * delays; Normal routines still available when enabled
 *
 * TODO: Use the counter module instead of "xor clkPin, clkPin"
 * <p>
 * DEFAULT: On
 */
#define SPI_OPTION_FAST
/**
 * TODO: Figure out why this doesn't work... :(
 * <p>
 * DEFAULT: Off
 */
#define SPI_OPTION_FAST_SECTOR
// This allows Doxygen to document the macro without permanently enabling it
#undef SPI_OPTION_FAST_SECTOR
/** @} */

namespace PropWare {

/**
 * @brief       SPI serial communications library; Core functionality comes from
 *              a dedicated assembly cog
 *
 * @detailed    Generally, multiple instances of the SPI class are not desired.
 *              To avoid the programmer from accidently creating multiple
 *              instances, this class is set up as a singleton. A static
 *              instance can be retrieved with PropWare::SPI::getInstance(). If
 *              multiple instances of PropWare::SPI are desired, the PropWare
 *              library (and your project) should be built from source with
 *              PROPWARE_NO_SAFE_SPI defined
 */
class SPI {
    public:
        /** Used as index for an array of PropWare objects */
        static const uint8_t PROPWARE_OBJECT_NUMBER = 0;

    public:
        /**
         * @brief   Descriptor for SPI signal as defined by Motorola modes
         *
         * @detailed    CPOL 0 refers to a low polarity (where the clock idles
         *              in the low state) and CPOL 1 is for high polarity.
         *              TODO: Describe phase
         * <table><tr><td>SPI Mode</td><td>CPOL</td><td>CPHA</td></tr><tr>
         * <td>0</td><td>0</td><td>0</td></tr><tr><td>1</td><td>0</td><td>1</td>
         * </tr><tr><td>2</td><td>1</td><td>0</td></tr><tr><td>3</td><td>1</td>
         * <td>1</td></tr></table>
         */
        /* Raw text version of the above HTML table
         *
         * SPI Mode     CPOL    CPHA
         * 0            0       0
         * 1            0       1
         * 2            1       0
         * 3            1       1
         */
        typedef enum {
            /** Mode 0 */MODE_0,
            /** Mode 1 */MODE_1,
            /** Mode 2 */MODE_2,
            /** Mode 3 */MODE_3,
            /** Number of SPI modes */MODES
        } Mode;

        /**
         * @brief   Determine if data is communicated with the LSB or MSB
         *          sent/received first
         *
         * @note    Initial value is SPI_MODES + 1 making them easily
         *          distinguishable
         */
        typedef enum {
            /**
             * Start the enumeration where SPI::Mode left off; this ensures no
             * overlap
             */
            LSB_FIRST = SPI::MODES,
            MSB_FIRST,
            BIT_MODES
        } BitMode;

        /**
         * Error codes - Proceeded by nothing
         */
        typedef enum {
            /** No error */NO_ERROR = 0,
            /** First SPI error */BEG_ERROR,
            /** SPI Error  0 */INVALID_PIN = BEG_ERROR,
            /** SPI Error  1 */INVALID_CLOCK_INIT,
            /** SPI Error  2 */INVALID_MODE,
            /** SPI Error  3 */INVALID_PIN_MASK,
            /** SPI Error  4 */TOO_MANY_BITS,
            /** SPI Error  5 */TIMEOUT,
            /** SPI Error  6 */TIMEOUT_RD,
            /** SPI Error  7 */EXCESSIVE_PAR_SZ,
            /** SPI Error  8 */COG_NOT_STARTED,
            /** SPI Error  9 */MODULE_NOT_RUNNING,
            /** SPI Error 10 */INVALID_FREQ,
            /** SPI Error 11 */INVALID_BYTE_SIZE,
            /** SPI Error 12 */ADDR_MISALIGN,
            /** SPI Error 13 */INVALID_BITMODE,
            /** Last SPI error code */END_ERROR = SPI::INVALID_BITMODE
        } ErrorCode;

    public:
        // (Default: CLKFREQ/10) Wait 0.1 seconds before throwing a timeout
        // error
        static const uint32_t WR_TIMEOUT_VAL;
        static const uint32_t RD_TIMEOUT_VAL;
        static const uint8_t MAX_PAR_BITS = 31;
        static const int32_t MAX_CLOCK;

#ifndef PROPWARE_NO_SAFE_SPI
    private:
#else
    public:
#endif
        /**
         * @brief   Create a new instance of SPI which will, upon calling
         *          SPI::start(), will start a new assembly cog. Creating
         *          multiple instances of PropWare::SPI allows the user to have
         *          multiple, independent SPI modules for simultaneous
         *          communication
         */
        SPI ();

    public:
        /**
         * @brief   Retrieve an instance of the SPI module
         *
         * @return  Address of an SPI module
         */
        static SPI* getInstance ();

        /**
         * @brief       Initialize an SPI module by starting a new cog
         *
         * @param[in]   mosi        PinNum mask for MOSI
         * @param[in]   miso        PinNum mask for MISO
         * @param[in]   sclk        PinNum mask for SCLK
         * @param[in]   frequency   Frequency, in Hz, to run the SPI clock; Must
         *                          be less than CLKFREQ/4
         * @param[in]   polarity    Polarity of the clock - idle low or high;
         *                          must be one of SPI_POLARITY_LOW or
         *                          SPI_POLARITY_HIGH
         *
         * @return      Returns 0 upon success, otherwise error code
         */
        PropWare::ErrorCode start (const PropWare::Pin::Mask mosi,
                const PropWare::Pin::Mask miso, const PropWare::Pin::Mask sclk,
                const int32_t frequency, const SPI::Mode mode,
                const SPI::BitMode bitmode);

        /**
         * @brief   Stop a running SPI cog
         *
         * @return  Returns 0 upon success, otherwise error code (will return
         *          SPI::COG_NOT_STARTED if no cog has previously been started)
         */
        PropWare::ErrorCode stop (void);

        /**
         * @brief    Determine if the SPI cog has already been initialized
         *
         * @return       Returns 1 if the SPI cog is up and running, 0 otherwise
         */
        bool is_running (void);

        /**
         * @brief   Wait for the SPI cog to signal that it is in the idle state
         *
         * @return  May return non-zero error code when a timeout occurs
         */
        PropWare::ErrorCode wait (void);

        /**
         * @brief   Wait for a specific value from the assembly cog
         *
         * @param[in]   value   The value being waited on
         *
         * @return  May return non-zero error code when a timeout occurs
         */
        PropWare::ErrorCode wait_specific (const uint32_t value);

        /**
         * @brief       Set the mode of SPI communication
         *
         * @param[in]   mode    Sets the SPI mode to one SPI::MODE_0,
         *                      SPI::MODE_1, SPI::MODE_2, or SPI::MODE_3
         *
         * @return      Can return non-zero in the case of a timeout
         */
        PropWare::ErrorCode set_mode (const SPI::Mode mode);

        /**
         * @brief       Set the bitmode of SPI communication
         *
         * @param[in]   mode    Select one of SPI::LSB_FIRST or SPI::MSB_FIRST
         *                      to choose which bit will be shifted out first
         *
         * @return      Can return non-zero in the case of a timeout
         */
        PropWare::ErrorCode set_bit_mode (const SPI::BitMode bitmode);

        /**
         * @brief       Change the SPI module's clock frequency
         *
         * @param[in]   frequency   Frequency, in Hz, to run the SPI clock; Must
         *                          be less than CLKFREQ/4 (for 80 MHz, 900 kHz
         *                          is the fastest I've tested successfully)
         *
         * @return      Returns 0 upon success, otherwise error code
         */
        PropWare::ErrorCode set_clock (const int32_t frequency);

        /**
         * @brief       Retrieve the SPI module's clock frequency
         *
         * @param[out]  *frequency  Frequency, in Hz, that the SPI module is
         *                          running
         *
         * @return      Returns 0 upon success, otherwise error code
         */
        PropWare::ErrorCode get_clock (int32_t *frequency);

        /**
         * @brief       Send a value out to a peripheral device
         *
         * Pass a value and mode into the assembly cog to be sent to the
         * peripheral; NOTE: this function is non-blocking and chip-select
         * should not be set inactive immediately after the return (you should
         * call spi_wait() before setting chip-select inactive)
         *
         * @param[in]   bits        Number of bits to be shifted out
         * @param[in]   value       The value to be shifted out
         *
         * @return      Returns 0 upon success, otherwise error code
         */
        PropWare::ErrorCode shift_out (uint8_t bits, uint32_t value);

        /**
         * @brief       Receive a value in from a peripheral device
         *
         * @param[in]   bits        Number of bits to be shifted in
         * @param[out]  *data       Received data will be stored at this address
         * @param[in]   bytes       Number of bytes allocated to *data; Example:
         *                            int newVal;
         *                            spi.shift_in(8, &newVal, sizeof(newVal));
         *                          Or if using a pointer:
         *                            int *newVal;
         *                            spi.shift_in(8, newVal, sizeof(*newVal));
         *
         * @return      Returns 0 upon success, otherwise error code
         */
        PropWare::ErrorCode shift_in (const uint8_t bits, void *data,
                const size_t size);

#ifdef SPI_OPTION_FAST
        /**
         * @brief       Send a value out to a peripheral device
         *
         * @detailed    Pass a value and mode into the assembly cog to be sent
         *              to the peripheral; NOTE: this function is non-blocking
         *              and chip-select should not be set inactive immediately
         *              after the return (you should call spi_wait() before
         *              setting chip-select inactive); Optimized for fastest
         *              possible clock speed; No error checking is performed;
         *              'Timeout' event will never be thrown and possible
         *              infinite loop can happen
         *
         * @param[in]   bits        Number of bits to be shifted out
         * @param[in]   value       The value to be shifted out
         *
         * @return      Returns 0 upon success, otherwise error code
         */
        PropWare::ErrorCode shift_out_fast (uint8_t bits, uint32_t value);

        /**
         * @brief       Quickly receive a value in from a peripheral device
         *
         * Optimized for fastest possible clock speed; No error checking is
         * performed; 'Timeout' event will never be thrown and possible infinite
         * loop can happen
         *
         * @param[in]   bits    Number of bits to be shifted in
         * @param[out]  *data   Received data will be stored at this address
         * @param[in]   bytes   Number of bytes allocated to *data; Example:
         *                        int newVal;
         *                        spi.shift_in_fast(8, &newVal, sizeof(newVal));
         *                      Or if using a pointer:
         *                        int *newVal;
         *                        spi.shift_in_fast(8, newVal, sizeof(*newVal));
         */
        PropWare::ErrorCode shift_in_fast (const uint8_t bits, void *data,
                const uint8_t bytes);

        /**
         * @brief       Read an entire sector of data in from an SD card
         *
         * @param[out]  *addr       First hub address where the data should be
         *                          written
         * @param[in]   blocking    When set to non-zero, function will not
         *                          return until the data transfer is complete
         */
        PropWare::ErrorCode shift_in_sector (const uint8_t addr[],
                const uint8_t blocking);
#endif
        /**
         * @brief   Print through UART an error string followed by entering an
         *          infinite loop
         *
         * @param   err     Error number used to determine error string
         */
        void print_error_str (const SPI::ErrorCode err) const;

    private:
        /************************************
         *** Private Constant Definitions ***
         ************************************/
        static const uint16_t TIMEOUT_WIGGLE_ROOM = 400;
        static const uint8_t FUNC_SEND = 0;
        static const uint8_t FUNC_READ = 1;
        static const uint8_t FUNC_SEND_FAST = 2;
        static const uint8_t FUNC_READ_FAST = 3;
        static const uint8_t FUNC_READ_SECTOR = 4;
        static const uint8_t FUNC_SET_MODE = 5;
        static const uint8_t FUNC_SET_BITMODE = 6;
        static const uint8_t FUNC_SET_FREQ = 7;
        static const uint8_t FUNC_GET_FREQ = 8;

        static const uint8_t BITS_OFFSET = 8;

        static const uint8_t PHASE_BIT = BIT_0;
        // Idle high == HIGH; Idle low == LOW
        static const uint8_t POLARITY_BIT = BIT_1;
        // MSB_FIRST == HIGH; LSB_FIRST == LOW
        static const uint8_t BITMODE_BIT = BIT_2;

    private:
        /***********************************
         *** Private Method Declarations ***
         ***********************************/
        /**
         * @brief       Read the value that the SPI cog just shifted in
         *
         * @param[out]  *par    Address to store the parameter
         * @param[in]   bytes   Number of bytes allocated to *data; Example:
         *                        int newVal;
         *                        spi.read_par(&newVal, sizeof(newVal));
         *                      Or if using a pointer:
         *                        int *newVal;
         *                        spi.read_par(newVal, sizeof(*newVal));
         *
         * @return      Returns 0 upon success, error code otherwise
         */
        PropWare::ErrorCode read_par (void *par, const size_t size);

    private:
        /********************************
         *** Private Member Variables ***
         ********************************/
        volatile uint32_t m_mailbox;
        int8_t m_cog;
        char m_errorInMethod[16];
};

}

#endif /* PROPWARE_SPI_H_ */
