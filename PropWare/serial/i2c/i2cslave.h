/**
 * @file        PropWare/serial/i2c/i2cslave.h
 *
 * @author		Markus Ebner
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

#include <PropWare/gpio/pin.h>
#include <PropWare/gpio/port.h>

namespace PropWare {

class I2CSlave;

typedef void(*I2CCallback)(I2CSlave*);

/**
 * @brief   Basic I2C slave driver
 *
 * Requires that the SDA and SCL pins have sufficient pullups. These should be selected based on the capacitance of the
 * devices on the I2C bus, and the expected clock speed.
 *
 * The driver supports restarts and only 7bit Addressing.
 * The driver does not support clock stretching.
 *
 * @warning	If the timeslot between start & restart, restart & restart, or stop and start is too small (depending on the master),
 * 			a transmission might be completely lost, due to the onReceive callback taking too much time.
 */
class I2CSlave {
    public:
        /**
         * @brief       Create a basic I2CSlave instance
         *
         * @param[in]	address				Address to join the bus as slave with
         * @param[in]	receive_buffer_size	Size of the receive buffer, that will hold a received message (-> maximal message size)
         * @param[in]   sclMask				Pin mask for the SCL pin
         * @param[in]   sdaMask				Pin mask for the SDA pin
         * @param[in]   frequency			Frequency to run the bus (default is highest standard I2C frequency of 400 kHz)
         *
         * @warning		Giving a {@link receive_buffer_size} that is too small will lead to received messages being truncated.
         */
        I2CSlave (const uint8_t address, size_t receive_buffer_size, const Pin::Mask sclMask, const Pin::Mask sdaMask)
				: m_slave_address(address),
				  m_receive_buffer((uint8_t*)malloc(receive_buffer_size)),
				  m_buffer_upper_bound(receive_buffer_size - 1),
				  m_buffer_ptr(receive_buffer_size),
                  m_scl(sclMask, Pin::Dir::IN),
                  m_sda(sdaMask, Pin::Dir::IN) {

            //Set outputs low
            this->m_scl.clear();
            this->m_sda.clear();
        }

        ~I2CSlave() {
			free(m_receive_buffer);
        }

        /**
         * @brief		Give the I2CSlave a callback to notify the user about an incomming transmission
         *
         * @warning		If the execution of this delegates takes too long, data on the bus might be missed.
         */
        void set_on_receive(I2CCallback onReceive) {
			this->m_on_receive = onReceive;
        }

        /**
         * @brief		Give the I2CSlave a callback to notify the user about an ancomming request
         *
         * @warning		This method should have the data to send on the bus prepared. Taking too long before transmit starts could mess the i2c state machines state up.
         */
        void set_on_request(I2CCallback onRequest) {
			this->m_on_request = onRequest;
        }


		/**
		 * @brief		Enter the loop that will watch and operate the bus.
		 */
		_NAKED void run() {
			while(true) { // start loop
				await_start();
				while(true) { //restart loop
					uint8_t address = read_address();
					if((address >> 1) == this->m_slave_address) { // Master is talking to us
						send_ack(); // Tell master we are there
						if((address & 0x1)) { // Master wants us to speak
							m_request_ended = false;
							m_on_request(this);
							break;
						} else { // Master wants us to listen
							bool restart = read_to_end();
							m_on_receive(this);
							reset_receive_buffer(); // Throw away bytes that the user did not fetch
							if(!restart){ break; /* received stop, go back to outer loop and await new start */ }
						}
					} else { // Master is talking to another slave
						//The next thing that interests us now is the nexst start -> go to await_start();
						break;
					}
				}
			}
		}

		/**
		 * @brief		Get the amount of bytes in the receive buffer
		 * @return		The amount of bytes in the receive buffer
		 */
		uint32_t available() {
			return (m_buffer_upper_bound - m_buffer_ptr + 1);
		}

		/**
		 * @brief		Get the next byte from the receiveBuffer
		 * @return		The next byte from the receiveBuffer
		 */
		int read() {
			if(m_buffer_ptr <= m_buffer_upper_bound) {
				return m_receive_buffer[m_buffer_ptr++];
			}
			return -1;
		}


		/**
		 * @brief		Send the given {@link byte} on the bus during a request from the bus master.
		 *
		 * @param[in]	byte	Byte to send to the requesting master.
		 *
		 * @warning		Calling this method too late may result in a defective state of the i2c state machine.
		 */
		void write(const uint8_t byte) {
			if(m_request_ended) { return; }
			uint32_t datamask;

			__asm__ volatile(
				"		mov			%[datamask],		#128				\n\t" // Initialize the mask that specifies the bit from the byte to send
				"		waitpne		%[SCLMask],			%[SCLMask]			\n\t" // Wait for the clock to be low first
				"		or			dira,				%[SDAMask]			\n\t" // Take SDA >after< clock is low (master has sda, since he is sending an ACK)

				"loop%=: "
				"		test		%[byte],			%[datamask]		wz	\n\t" // Test whether bit to send is 0 or 1
				"		muxnz		outa,				%[SDAMask]			\n\t" // Set the bit on the bus while the clock is low
				"		waitpeq		%[SCLMask],			%[SCLMask]			\n\t" // Wait for the next clock cycle to start
				"		shr			%[datamask],		#1				wz	\n\t" // Shift the mask one down to select the next lower bit
				"		waitpne		%[SCLMask],			%[SCLMask]			\n\t" // Wait for the clock cycle to end
				"if_nz	brs			#loop%=									\n\t" // Continue until datamask is 0 (no bit left)

				//wait for ack
				"		andn		dira,				%[SDAMask]			\n\t" // Set SDA to input, because master has to pull it down
				"		waitpeq		%[SCLMask],			%[SCLMask]			\n\t" // Wait for the ACK-Clock to begin
				"		test		%[SDAMask],			ina				wz	\n\t" // Test whether master pulled SDA down or not
				"if_z	mov			%[requestEnded],	#0					\n\t" // SDA low -> ACK / SDA high -> NAK
				"if_nz	mov			%[requestEnded],	#1					\n\t"
			: // Output
			[datamask]		"=&r"	(datamask),
			[requestEnded]	"=&r"	(m_request_ended)
			: // Input
			[SDAMask]	"r"		(m_sda.get_mask()),
			[SCLMask]	"r"		(m_scl.get_mask()),
			[byte]		"r"		(byte));
		}


	private:

		/**
		 * @brief		Wait for a start / restart condition on the bus.
		 */
		void await_start() {
			__asm__ volatile(
				"loop%=: "
				"		waitpeq		%[SDAMask],		%[SDAMask]		\n\t" // Wait for sda to be high
				"		waitpne		%[SDAMask],		%[SDAMask]		\n\t" // Wait for sda to get low
				"		test		%[SCLMask],		ina			wz	\n\t" // If scl was high while sda got low...
				"if_z	brs			#loop%=							\n\t" // ... return, otherwise: start anew
			: // Output
			: // Input
			[SDAMask]		"r" (m_sda.get_mask()),
			[SCLMask]		"r" (m_scl.get_mask()));
		}

		/**
		 * @brief		Read one byte from the bus without sending any response.
		 */
		uint8_t read_address() {
			uint32_t result;
			uint32_t bitcounter;

			__asm__ volatile(
			FC_START("ReadAddressStart", "ReadAddressEnd")
				"		mov		%[result],		#0					\n\t"
				"		mov		%[bitcounter],	#8					\n\t"
				"nextBit%=: "
				"		waitpne	%[SCLMask],		%[SCLMask]			\n\t" // Wait for clock to get low (should already be low at this time)
				"		waitpeq	%[SCLMask],		%[SCLMask]			\n\t" // Wait for clock to get high
				"		test	%[SDAMask],		ina				wc	\n\t" // Read bit from bus ...
				"		rcl		%[result],		#1					\n\t" // ... and store in result
				"		djnz	%[bitcounter],	#" FC_ADDR("nextBit%=", "ReadAddressStart") "	\n\t"
				FC_END("ReadAddressEnd")
				: [result]		"=&r"	(result),
				  [bitcounter]	"=&r"	(bitcounter)
				: [SDAMask]	"r"		(m_sda.get_mask()),
				  [SCLMask] "r"		(m_scl.get_mask())
			);
			return (uint8_t)result;
		}

		/**
		 * @brief		Wait for the next clock and pull the data line down to signal the master an ACK
		 */
		inline __attribute((always_inline)) void send_ack() {
			//The code does not work anymore when removing inline and attribute always_inline. Why is this?
            __asm__ volatile(
				"		waitpne		%[SCLMask],		%[SCLMask]			\n\t" // Wait for SCL to be low first
				"		or			dira,			%[SDAMask]			\n\t" // Take SDA and ...
				"		andn		outa,			%[SDAMask]			\n\t" // ... pull it down
				"		waitpeq		%[SCLMask],		%[SCLMask]			\n\t" // Wait for SCL to go high ...
				"		waitpne		%[SCLMask],		%[SCLMask]			\n\t" // ... and wait for it to go low again
				"		andn		dira,			%[SDAMask]			\n\t" // Let go of SDA again (high by float)
            : : // Inputs
            [SDAMask] "r"(this->m_sda.get_mask()),
            [SCLMask] "r"(this->m_scl.get_mask()));
		}

		/**
		 * @brief		Read all bytes the master sends until either a restart or a stop condition is received
		 *
		 * @return		{@code true} if a restart condition was received, {@code false} if a stop condition was received
		 */
		bool read_to_end() {
			uint32_t result;
			uint32_t bitcounter;
			uint32_t is_restart;

			while(true) {
				__asm__ volatile(
					"		mov			%[is_restart],	#2								\n\t"
					"		mov			%[bitcounter],	#7								\n\t"
					"		mov			%[result],		#0								\n\t"
					"		waitpne		%[SCLMask],		%[SCLMask]						\n\t" // Wait for scl to be low first
					"		waitpeq		%[SCLMask],		%[SCLMask]						\n\t" // Wait for scl to go high
					"		test		%[SDAMask],		ina						wc		\n\t" // Read bit and...
					"		rcl			%[result],		#1								\n\t" // ... store in result
					"if_c	brs			#DetectRestart%=								\n\t" // The first bit of a received byte may be b7, or a stop / restart
																							  // If sda was high, it can only be a restart
					"DetectStop%=: "
					"		test		%[SCLMask],		ina						wz		\n\t" // scl went low -> no chance for a stop-condition to be detected ...
					"if_z	brs			#loop%=											\n\t" // ... continue receiving data bits
					"		test		%[SDAMask],		ina						wz		\n\t"
					"if_nz	mov			%[is_restart],	#0								\n\t" // stop detected. Set is_restart to false ...
					"if_nz	brs			#ReceiveEnd%=									\n\t" // ... and exit
					"		brs			#DetectStop%=									\n\t"

					"DetectRestart%=: "
					"		test		%[SCLMask],		ina						wz		\n\t" // scl went low -> no chance for a (re)start-condition to be detected ...
					"if_z	brs			#loop%=											\n\t" // ... continue receiving data bits
					"		test		%[SDAMask],		ina						wz		\n\t"
					"if_z	mov			%[is_restart],	#1								\n\t" // restart detected. Set is_restart to true...
					"if_z	brs			#ReceiveEnd%=									\n\t" // ... and exit
					"		brs			#DetectRestart%=								\n\t"

					"loop%=: "																  // for(int i = 0; i < 8; ++i) {
					"		waitpne		%[SCLMask],		%[SCLMask]						\n\t"		// Wait for ...
					"		waitpeq		%[SCLMask],		%[SCLMask]						\n\t"		// ... next clock
					"		test		%[SDAMask],		ina						wc		\n\t" 		// Read bit and...
					"		rcl			%[result],		#1								\n\t" 		// ... store in result
					"		sub			%[bitcounter],	#1						wz		\n\t"
					"if_nz	brs			#loop%=											\n\t" // }

					"ReceiveEnd%=: "
				: // Outputs
				[result] 		"=&r" (result),
				[bitcounter]	"=&r" (bitcounter),
				[is_restart]	"=&r" (is_restart)
				: // Inputs
				[SDAMask] "r" (this->m_sda.get_mask()),
				[SCLMask] "r" (this->m_scl.get_mask()));

				if(is_restart != 2) {
					return (bool) is_restart;
				} else {
					send_ack();
					append_receive_buffer((uint8_t)result);
				}
			}
		}




	/**
	 * @brief		Add a byte to the receive buffer that the user can then later fetch from it in the onReceive handler.
	 */
	void append_receive_buffer(uint8_t c) {
		if(m_buffer_ptr > 0) {
			m_receive_buffer[--m_buffer_ptr] = c;
		}
	}

	/**
	 * @brief		Reset the receiveBuffer's state for the next message. This throws away bytes that the user did not fetch in the handler.
	 */
	void reset_receive_buffer() {
		m_buffer_ptr = m_buffer_upper_bound + 1;
	}

    private:
        const uint8_t	m_slave_address;
        const Pin		m_scl;
        const Pin		m_sda;

        //receive
		uint8_t*		m_receive_buffer;		// Buffer storing the received messages
		uint32_t		m_buffer_upper_bound;	// receive_buffer_size - 1
		uint32_t		m_buffer_ptr;			// Index always pointing to the last written element in the receiveBuffer (= receive_buffer_size when empty)
		I2CCallback		m_on_receive;

		//request
		bool			m_request_ended;
        I2CCallback		m_on_request;
};

}
