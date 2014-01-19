/**
 * @file    SD_Demo.h
 *
 * @author  David Zemon
 * @project SD_Demo
 */

#ifndef SD_DEMO_H_
#define SD_DEMO_H_

#define DEBUG
//#define LOW_RAM_MODE
#define TEST_WRITE
#define TEST_SHELL

// Includes
#include <propeller.h>
#include <PropWare.h>
#include <sd.h>

#ifdef DEBUG
#include <stdio.h>
#endif

#if (!(defined SD_VERBOSE) && !(defined SD_VERBOSE_BLOCKS))
#define printf      __simple_printf
#endif

#define CS          BIT_4
#define MOSI        BIT_0
#define MISO        BIT_1
#define SCLK        BIT_2

#define OLD_FILE    "STUFF.TXT"
#define NEW_FILE    "TEST.TXT"

void error (const uint8_t err);

#endif /* SD_DEMO_H_ */
