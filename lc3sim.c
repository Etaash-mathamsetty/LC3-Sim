/*
 *   MIT License
 *   Copyright (c) 2025 Etaash Mathamsetty
 *
 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 *
 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define FLAG_N (1 << 2)
#define FLAG_Z (1 << 1)
#define FLAG_P (1 << 0)

#define OS_KBSR 0xFE00
#define OS_KBDR 0xFE02
#define OS_DSR 0xFE04
#define OS_DDR 0xFE06
#define OS_PSR 0xFFFC
#define OS_MCR 0xFFFE
#define OS_MCC 0xFFFF

#define MASK_HIGH 0x7FFF

/* Not valid LC-3 addresses, holds emulator data
   TODO: move emulator data to these addresses
*/

/* supervisor R6 */
#define OS_SSP 0x10000
/* user R6 */
#define OS_USP 0x10001
/* PC */
#define OS_PROGC 0x10002
/* register base */
#define OS_RBASE 0x10003

/* ASM to binary macros, similar enough to actual LC-3 asm */
#define ADDR(dst, r1, r2) 0x1000 | ((dst & 0x7) << 9) | ((r1 & 0x7) << 6) | (r2 & 0x7)
#define ADDIMM(dst, r1, imm5) 0x1000 | ((dst & 0x7) << 9) | ((r1 & 0x7) << 6) | 0b100000 | (imm5 & 0b11111)
#define ANDR(dst, r1, r2) 0x5000 | ((dst & 0x7) << 9) | ((r1 & 0x7) << 6) | (r2 & 0x7)
#define ANDIMM(dst, r1, imm5) 0x5000 | ((dst & 0x7) << 9) | ((r1 & 0x7) << 6) | 0b100000 | (imm5 & 0b11111)
#define STR(src, base, offset6) 0x7000 | ((src & 0x7) << 9) | ((base & 0x7) << 6) | (offset6 & 0b111111)
#define LEA(dst, offset) 0xe000 | ((dst & 0x7) << 9) | (offset & 0b111111111)
#define LD(dst, offset9) 0x2000 | ((dst & 0x7) << 9) | (offset9 & 0b111111111)
#define LDI(dst, offset9) 0xa000 | ((dst & 0x7) << 9) | (offset9 & 0b111111111)
#define LDR(dst, src, offset6) 0x6000 | ((dst & 0x7) << 9) | ((src & 0x7) << 6) | (offset6 & 0b111111)
#define STI(src, offset9) 0xb000 | ((src & 0x7) << 9) | (offset9 & 0b111111111)
#define BR(nzp, offset9) ((nzp & 0x7) << 9) | (offset9 & 0b111111111)
#define JMP(r) 0xc000 | ((r & 0x7) << 6)
#define RET() JMP(7)
#define TRAP(vec8) 0xf000 | (vec8 & 0xff)
#define RTI() 0x8000

/* compose 2 characters into one word for PUTSP */
#define COMPOSE_CH(ch1, ch2) (ch2 & 0xff) | ((ch1 & 0xff) << 8)

/* TRAP/INT addresses for TRAP/INT table */
#define BAD_TRAP 0x200
#define PUTS_TRAP 0x23b
#define HALT_TRAP 0x21a
#define OUT_TRAP 0x24a
#define BAD_INT 0x0 /* TODO */
#define OS_START 0x230
#define USER_PC 0x23a
#define GETC_TRAP 0x254
#define IN_TRAP 0x25a
#define PRIV_MODE_EXCEPTION 0x280

const uint16_t OSProgram[0x500] = {
    /* TRAP VECTORS */
    BAD_TRAP, /* 0 */
    BAD_TRAP, /* 1 */
    BAD_TRAP, /* 2 */
    BAD_TRAP, /* 3 */
    BAD_TRAP, /* 4 */
    BAD_TRAP, /* 5 */
    BAD_TRAP, /* 6 */
    BAD_TRAP, /* 7 */
    BAD_TRAP, /* 8 */
    BAD_TRAP, /* 9 */
    BAD_TRAP, /* a */
    BAD_TRAP, /* b */
    BAD_TRAP, /* c */
    BAD_TRAP, /* d */
    BAD_TRAP, /* e */
    BAD_TRAP, /* f */
    BAD_TRAP, /* 10 */
    BAD_TRAP, /* 11 */
    BAD_TRAP, /* 12 */
    BAD_TRAP, /* 13 */
    BAD_TRAP, /* 14 */
    BAD_TRAP, /* 15 */
    BAD_TRAP, /* 16 */
    BAD_TRAP, /* 17 */
    BAD_TRAP, /* 18 */
    BAD_TRAP, /* 19 */
    BAD_TRAP, /* 1a */
    BAD_TRAP, /* 1b */
    BAD_TRAP, /* 1c */
    BAD_TRAP, /* 1d */
    BAD_TRAP, /* 1e */
    BAD_TRAP, /* 1f */
    GETC_TRAP, /* 20 */
    OUT_TRAP, /* 21 */
    PUTS_TRAP, /* 22 */
    IN_TRAP, /* 23 */
    BAD_TRAP, /* 24 */
    HALT_TRAP, /* 25 */
    BAD_TRAP, /* 26 */
    BAD_TRAP, /* 27 */
    BAD_TRAP, /* 28 */
    BAD_TRAP, /* 29 */
    BAD_TRAP, /* 2a */
    BAD_TRAP, /* 2b */
    BAD_TRAP, /* 2c */
    BAD_TRAP, /* 2d */
    BAD_TRAP, /* 2e */
    BAD_TRAP, /* 2f */
    BAD_TRAP, /* 30 */
    BAD_TRAP, /* 31 */
    BAD_TRAP, /* 32 */
    BAD_TRAP, /* 33 */
    BAD_TRAP, /* 34 */
    BAD_TRAP, /* 35 */
    BAD_TRAP, /* 36 */
    BAD_TRAP, /* 37 */
    BAD_TRAP, /* 38 */
    BAD_TRAP, /* 39 */
    BAD_TRAP, /* 3a */
    BAD_TRAP, /* 3b */
    BAD_TRAP, /* 3c */
    BAD_TRAP, /* 3d */
    BAD_TRAP, /* 3e */
    BAD_TRAP, /* 3f */
    BAD_TRAP, /* 40 */
    BAD_TRAP, /* 41 */
    BAD_TRAP, /* 42 */
    BAD_TRAP, /* 43 */
    BAD_TRAP, /* 44 */
    BAD_TRAP, /* 45 */
    BAD_TRAP, /* 46 */
    BAD_TRAP, /* 47 */
    BAD_TRAP, /* 48 */
    BAD_TRAP, /* 49 */
    BAD_TRAP, /* 4a */
    BAD_TRAP, /* 4b */
    BAD_TRAP, /* 4c */
    BAD_TRAP, /* 4d */
    BAD_TRAP, /* 4e */
    BAD_TRAP, /* 4f */
    BAD_TRAP, /* 50 */
    BAD_TRAP, /* 51 */
    BAD_TRAP, /* 52 */
    BAD_TRAP, /* 53 */
    BAD_TRAP, /* 54 */
    BAD_TRAP, /* 55 */
    BAD_TRAP, /* 56 */
    BAD_TRAP, /* 57 */
    BAD_TRAP, /* 58 */
    BAD_TRAP, /* 59 */
    BAD_TRAP, /* 5a */
    BAD_TRAP, /* 5b */
    BAD_TRAP, /* 5c */
    BAD_TRAP, /* 5d */
    BAD_TRAP, /* 5e */
    BAD_TRAP, /* 5f */
    BAD_TRAP, /* 60 */
    BAD_TRAP, /* 61 */
    BAD_TRAP, /* 62 */
    BAD_TRAP, /* 63 */
    BAD_TRAP, /* 64 */
    BAD_TRAP, /* 65 */
    BAD_TRAP, /* 66 */
    BAD_TRAP, /* 67 */
    BAD_TRAP, /* 68 */
    BAD_TRAP, /* 69 */
    BAD_TRAP, /* 6a */
    BAD_TRAP, /* 6b */
    BAD_TRAP, /* 6c */
    BAD_TRAP, /* 6d */
    BAD_TRAP, /* 6e */
    BAD_TRAP, /* 6f */
    BAD_TRAP, /* 70 */
    BAD_TRAP, /* 71 */
    BAD_TRAP, /* 72 */
    BAD_TRAP, /* 73 */
    BAD_TRAP, /* 74 */
    BAD_TRAP, /* 75 */
    BAD_TRAP, /* 76 */
    BAD_TRAP, /* 77 */
    BAD_TRAP, /* 78 */
    BAD_TRAP, /* 79 */
    BAD_TRAP, /* 7a */
    BAD_TRAP, /* 7b */
    BAD_TRAP, /* 7c */
    BAD_TRAP, /* 7d */
    BAD_TRAP, /* 7e */
    BAD_TRAP, /* 7f */
    BAD_TRAP, /* 70 */
    BAD_TRAP, /* 71 */
    BAD_TRAP, /* 72 */
    BAD_TRAP, /* 73 */
    BAD_TRAP, /* 74 */
    BAD_TRAP, /* 75 */
    BAD_TRAP, /* 76 */
    BAD_TRAP, /* 77 */
    BAD_TRAP, /* 78 */
    BAD_TRAP, /* 79 */
    BAD_TRAP, /* 7a */
    BAD_TRAP, /* 7b */
    BAD_TRAP, /* 7c */
    BAD_TRAP, /* 7d */
    BAD_TRAP, /* 7e */
    BAD_TRAP, /* 8f */
    BAD_TRAP, /* 70 */
    BAD_TRAP, /* 71 */
    BAD_TRAP, /* 72 */
    BAD_TRAP, /* 73 */
    BAD_TRAP, /* 74 */
    BAD_TRAP, /* 75 */
    BAD_TRAP, /* 76 */
    BAD_TRAP, /* 77 */
    BAD_TRAP, /* 78 */
    BAD_TRAP, /* 79 */
    BAD_TRAP, /* 7a */
    BAD_TRAP, /* 7b */
    BAD_TRAP, /* 7c */
    BAD_TRAP, /* 7d */
    BAD_TRAP, /* 7e */
    BAD_TRAP, /* 9f */
    BAD_TRAP, /* 70 */
    BAD_TRAP, /* 71 */
    BAD_TRAP, /* 72 */
    BAD_TRAP, /* 73 */
    BAD_TRAP, /* 74 */
    BAD_TRAP, /* 75 */
    BAD_TRAP, /* 76 */
    BAD_TRAP, /* 77 */
    BAD_TRAP, /* 78 */
    BAD_TRAP, /* 79 */
    BAD_TRAP, /* 7a */
    BAD_TRAP, /* 7b */
    BAD_TRAP, /* 7c */
    BAD_TRAP, /* 7d */
    BAD_TRAP, /* 7e */
    BAD_TRAP, /* af */
    BAD_TRAP, /* 70 */
    BAD_TRAP, /* 71 */
    BAD_TRAP, /* 72 */
    BAD_TRAP, /* 73 */
    BAD_TRAP, /* 74 */
    BAD_TRAP, /* 75 */
    BAD_TRAP, /* 76 */
    BAD_TRAP, /* 77 */
    BAD_TRAP, /* 78 */
    BAD_TRAP, /* 79 */
    BAD_TRAP, /* 7a */
    BAD_TRAP, /* 7b */
    BAD_TRAP, /* 7c */
    BAD_TRAP, /* 7d */
    BAD_TRAP, /* 7e */
    BAD_TRAP, /* bf */
    BAD_TRAP, /* 70 */
    BAD_TRAP, /* 71 */
    BAD_TRAP, /* 72 */
    BAD_TRAP, /* 73 */
    BAD_TRAP, /* 74 */
    BAD_TRAP, /* 75 */
    BAD_TRAP, /* 76 */
    BAD_TRAP, /* 77 */
    BAD_TRAP, /* 78 */
    BAD_TRAP, /* 79 */
    BAD_TRAP, /* 7a */
    BAD_TRAP, /* 7b */
    BAD_TRAP, /* 7c */
    BAD_TRAP, /* 7d */
    BAD_TRAP, /* 7e */
    BAD_TRAP, /* cf */
    BAD_TRAP, /* 70 */
    BAD_TRAP, /* 71 */
    BAD_TRAP, /* 72 */
    BAD_TRAP, /* 73 */
    BAD_TRAP, /* 74 */
    BAD_TRAP, /* 75 */
    BAD_TRAP, /* 76 */
    BAD_TRAP, /* 77 */
    BAD_TRAP, /* 78 */
    BAD_TRAP, /* 79 */
    BAD_TRAP, /* 7a */
    BAD_TRAP, /* 7b */
    BAD_TRAP, /* 7c */
    BAD_TRAP, /* 7d */
    BAD_TRAP, /* 7e */
    BAD_TRAP, /* df */
    BAD_TRAP, /* 70 */
    BAD_TRAP, /* 71 */
    BAD_TRAP, /* 72 */
    BAD_TRAP, /* 73 */
    BAD_TRAP, /* 74 */
    BAD_TRAP, /* 75 */
    BAD_TRAP, /* 76 */
    BAD_TRAP, /* 77 */
    BAD_TRAP, /* 78 */
    BAD_TRAP, /* 79 */
    BAD_TRAP, /* 7a */
    BAD_TRAP, /* 7b */
    BAD_TRAP, /* 7c */
    BAD_TRAP, /* 7d */
    BAD_TRAP, /* 7e */
    BAD_TRAP, /* ef */
    BAD_TRAP, /* 70 */
    BAD_TRAP, /* 71 */
    BAD_TRAP, /* 72 */
    BAD_TRAP, /* 73 */
    BAD_TRAP, /* 74 */
    BAD_TRAP, /* 75 */
    BAD_TRAP, /* 76 */
    BAD_TRAP, /* 77 */
    BAD_TRAP, /* 78 */
    BAD_TRAP, /* 79 */
    BAD_TRAP, /* 7a */
    BAD_TRAP, /* 7b */
    BAD_TRAP, /* 7c */
    BAD_TRAP, /* 7d */
    BAD_TRAP, /* 7e */
    BAD_TRAP, /* ff */
    /* TODO: Fix above and below comments */
    /* INTERRUPT VECTORS */
    PRIV_MODE_EXCEPTION, /* 100 */
    BAD_INT, /* 101 */
    BAD_INT, /* 102 */
    BAD_INT, /* 103 */
    BAD_INT, /* 104 */
    BAD_INT, /* 105 */
    BAD_INT, /* 106 */
    BAD_INT, /* 107 */
    BAD_INT, /* 108 */
    BAD_INT, /* 109 */
    BAD_INT, /* 10a */
    BAD_INT, /* 10b */
    BAD_INT, /* 10c */
    BAD_INT, /* 10d */
    BAD_INT, /* 10e */
    BAD_INT, /* 10f */
    BAD_INT, /* 100 */
    BAD_INT, /* 101 */
    BAD_INT, /* 102 */
    BAD_INT, /* 103 */
    BAD_INT, /* 104 */
    BAD_INT, /* 105 */
    BAD_INT, /* 106 */
    BAD_INT, /* 107 */
    BAD_INT, /* 108 */
    BAD_INT, /* 109 */
    BAD_INT, /* 10a */
    BAD_INT, /* 10b */
    BAD_INT, /* 10c */
    BAD_INT, /* 10d */
    BAD_INT, /* 10e */
    BAD_INT, /* 11f */
    BAD_INT, /* 100 */
    BAD_INT, /* 101 */
    BAD_INT, /* 102 */
    BAD_INT, /* 103 */
    BAD_INT, /* 104 */
    BAD_INT, /* 105 */
    BAD_INT, /* 106 */
    BAD_INT, /* 107 */
    BAD_INT, /* 108 */
    BAD_INT, /* 109 */
    BAD_INT, /* 10a */
    BAD_INT, /* 10b */
    BAD_INT, /* 10c */
    BAD_INT, /* 10d */
    BAD_INT, /* 10e */
    BAD_INT, /* 12f */
    BAD_INT, /* 100 */
    BAD_INT, /* 101 */
    BAD_INT, /* 102 */
    BAD_INT, /* 103 */
    BAD_INT, /* 104 */
    BAD_INT, /* 105 */
    BAD_INT, /* 106 */
    BAD_INT, /* 107 */
    BAD_INT, /* 108 */
    BAD_INT, /* 109 */
    BAD_INT, /* 10a */
    BAD_INT, /* 10b */
    BAD_INT, /* 10c */
    BAD_INT, /* 10d */
    BAD_INT, /* 10e */
    BAD_INT, /* 13f */
    BAD_INT, /* 100 */
    BAD_INT, /* 101 */
    BAD_INT, /* 102 */
    BAD_INT, /* 103 */
    BAD_INT, /* 104 */
    BAD_INT, /* 105 */
    BAD_INT, /* 106 */
    BAD_INT, /* 107 */
    BAD_INT, /* 108 */
    BAD_INT, /* 109 */
    BAD_INT, /* 10a */
    BAD_INT, /* 10b */
    BAD_INT, /* 10c */
    BAD_INT, /* 10d */
    BAD_INT, /* 10e */
    BAD_INT, /* 14f */
    BAD_INT, /* 100 */
    BAD_INT, /* 101 */
    BAD_INT, /* 102 */
    BAD_INT, /* 103 */
    BAD_INT, /* 104 */
    BAD_INT, /* 105 */
    BAD_INT, /* 106 */
    BAD_INT, /* 107 */
    BAD_INT, /* 108 */
    BAD_INT, /* 109 */
    BAD_INT, /* 10a */
    BAD_INT, /* 10b */
    BAD_INT, /* 10c */
    BAD_INT, /* 10d */
    BAD_INT, /* 10e */
    BAD_INT, /* 15f */
    BAD_INT, /* 100 */
    BAD_INT, /* 101 */
    BAD_INT, /* 102 */
    BAD_INT, /* 103 */
    BAD_INT, /* 104 */
    BAD_INT, /* 105 */
    BAD_INT, /* 106 */
    BAD_INT, /* 107 */
    BAD_INT, /* 108 */
    BAD_INT, /* 109 */
    BAD_INT, /* 10a */
    BAD_INT, /* 10b */
    BAD_INT, /* 10c */
    BAD_INT, /* 10d */
    BAD_INT, /* 10e */
    BAD_INT, /* 16f */
    BAD_INT, /* 100 */
    BAD_INT, /* 101 */
    BAD_INT, /* 102 */
    BAD_INT, /* 103 */
    BAD_INT, /* 104 */
    BAD_INT, /* 105 */
    BAD_INT, /* 106 */
    BAD_INT, /* 107 */
    BAD_INT, /* 108 */
    BAD_INT, /* 109 */
    BAD_INT, /* 10a */
    BAD_INT, /* 10b */
    BAD_INT, /* 10c */
    BAD_INT, /* 10d */
    BAD_INT, /* 10e */
    BAD_INT, /* 17f */
    BAD_INT, /* 100 */
    BAD_INT, /* 101 */
    BAD_INT, /* 102 */
    BAD_INT, /* 103 */
    BAD_INT, /* 104 */
    BAD_INT, /* 105 */
    BAD_INT, /* 106 */
    BAD_INT, /* 107 */
    BAD_INT, /* 108 */
    BAD_INT, /* 109 */
    BAD_INT, /* 10a */
    BAD_INT, /* 10b */
    BAD_INT, /* 10c */
    BAD_INT, /* 10d */
    BAD_INT, /* 10e */
    BAD_INT, /* 18f */
    BAD_INT, /* 100 */
    BAD_INT, /* 101 */
    BAD_INT, /* 102 */
    BAD_INT, /* 103 */
    BAD_INT, /* 104 */
    BAD_INT, /* 105 */
    BAD_INT, /* 106 */
    BAD_INT, /* 107 */
    BAD_INT, /* 108 */
    BAD_INT, /* 109 */
    BAD_INT, /* 10a */
    BAD_INT, /* 10b */
    BAD_INT, /* 10c */
    BAD_INT, /* 10d */
    BAD_INT, /* 10e */
    BAD_INT, /* 19f */
    BAD_INT, /* 100 */
    BAD_INT, /* 101 */
    BAD_INT, /* 102 */
    BAD_INT, /* 103 */
    BAD_INT, /* 104 */
    BAD_INT, /* 105 */
    BAD_INT, /* 106 */
    BAD_INT, /* 107 */
    BAD_INT, /* 108 */
    BAD_INT, /* 109 */
    BAD_INT, /* 10a */
    BAD_INT, /* 10b */
    BAD_INT, /* 10c */
    BAD_INT, /* 10d */
    BAD_INT, /* 10e */
    BAD_INT, /* 1af */
    BAD_INT, /* 100 */
    BAD_INT, /* 101 */
    BAD_INT, /* 102 */
    BAD_INT, /* 103 */
    BAD_INT, /* 104 */
    BAD_INT, /* 105 */
    BAD_INT, /* 106 */
    BAD_INT, /* 107 */
    BAD_INT, /* 108 */
    BAD_INT, /* 109 */
    BAD_INT, /* 10a */
    BAD_INT, /* 10b */
    BAD_INT, /* 10c */
    BAD_INT, /* 10d */
    BAD_INT, /* 10e */
    BAD_INT, /* 1bf */
    BAD_INT, /* 100 */
    BAD_INT, /* 101 */
    BAD_INT, /* 102 */
    BAD_INT, /* 103 */
    BAD_INT, /* 104 */
    BAD_INT, /* 105 */
    BAD_INT, /* 106 */
    BAD_INT, /* 107 */
    BAD_INT, /* 108 */
    BAD_INT, /* 109 */
    BAD_INT, /* 10a */
    BAD_INT, /* 10b */
    BAD_INT, /* 10c */
    BAD_INT, /* 10d */
    BAD_INT, /* 10e */
    BAD_INT, /* 1cf */
    BAD_INT, /* 100 */
    BAD_INT, /* 101 */
    BAD_INT, /* 102 */
    BAD_INT, /* 103 */
    BAD_INT, /* 104 */
    BAD_INT, /* 105 */
    BAD_INT, /* 106 */
    BAD_INT, /* 107 */
    BAD_INT, /* 108 */
    BAD_INT, /* 109 */
    BAD_INT, /* 10a */
    BAD_INT, /* 10b */
    BAD_INT, /* 10c */
    BAD_INT, /* 10d */
    BAD_INT, /* 10e */
    BAD_INT, /* 1df */
    BAD_INT, /* 100 */
    BAD_INT, /* 101 */
    BAD_INT, /* 102 */
    BAD_INT, /* 103 */
    BAD_INT, /* 104 */
    BAD_INT, /* 105 */
    BAD_INT, /* 106 */
    BAD_INT, /* 107 */
    BAD_INT, /* 108 */
    BAD_INT, /* 109 */
    BAD_INT, /* 1ea */
    BAD_INT, /* 1eb */
    BAD_INT, /* 1ec */
    BAD_INT, /* 1ed */
    BAD_INT, /* 1ee */
    BAD_INT, /* 1ef */
    BAD_INT, /* 1f0 */
    BAD_INT, /* 1f1 */
    BAD_INT, /* 1f2 */
    BAD_INT, /* 1f3 */
    BAD_INT, /* 1f4 */
    BAD_INT, /* 1f5 */
    BAD_INT, /* 1f6 */
    BAD_INT, /* 1f7 */
    BAD_INT, /* 1f8 */
    BAD_INT, /* 1f9 */
    BAD_INT, /* 1fa */
    BAD_INT, /* 1fb */
    BAD_INT, /* 1fc */
    BAD_INT, /* 1fd */
    BAD_INT, /* 1fe */
    BAD_INT, /* 1ff */
    /* TRAP METHODS */
    /* BAD TRAP */
    LEA(0, 2), /* 200: LEA R0, +2 */
    TRAP(0x22), /* 201: TRAP 0x22 (PUTS) */
    TRAP(0x25), /* 202: TRAP 0x25 (HALT) */
    '\n', /* 203 */
    '\n', /* 204 */
    'B', /* 205 */
    'a', /* 206 */
    'd', /* 207 */
    ' ', /* 208 */
    'T', /* 209 */
    'r', /* 20a */
    'a', /* 20b */
    'p', /* 20c */
    ' ', /* 20d */
    'E', /* 20e */
    'x', /* 20f */
    'e', /* 210 */
    'c', /* 211 */
    'u', /* 212 */
    't', /* 213 */
    'e', /* 214 */
    'd', /* 215 */
    '!', /* 216 */
    '\n', /* 217 */
    '\n', /* 218 */
      0,  /* 219 */
    /* HALT TRAP */
    LEA(0, 8), /* 0x21a */
    TRAP(0x22), /* 0x21b */
    LDI(0, 4), /* 0x21c */
    LD(1, 4), /* 0x21d */
    ANDR(0, 0, 1), /* 0x21e */
    STI(0, 1), /* 0x21f */
    BR(0b111, -5), /* 0x220: Try to turn off the clock again :D */
    OS_MCR, /* 0x221 */
    MASK_HIGH, /* 0x222 */
    '\n', /* 0x223 */
    '\n', /* 224 */
    'H', /* 225 */
    'a', /* 226 */
    'l', /* 227 */
    't', /* 228 */
    'i', /* 229 */
    'n', /* 22a */
    'g', /* 22b */
    '!', /* 22c */
    '\n', /* 22d */
    '\n', /* 22e */
     0, /* 22f */
    /* OS START */
    LD(6, 8), /* 230 */
    LD(0, 6), /* 231 */
    ADDIMM(6, 6, -1), /* 232 */
    STR(0, 6, 0), /* 233 */
    LD(0, 5), /* 234 */
    ADDIMM(6, 6, -1), /* 235 */
    STR(0, 6, 0), /* 236 */
    RTI(), /* 237 */
    0x8002, /* 238 initial user PSR (user mode + FLAG_Z) */
    0x3000, /* 239 supervisor stack base */
    0x3000, /* 23a program start */
    /* PUTS TRAP */
    ADDIMM(6, 6, -1), /* push 23b */
    STR(0, 6, 0), /* save r0 23c */
    ADDIMM(6, 6, -1), /* push 23d */
    STR(1, 6, 0), /* save r1 23e */
    ADDIMM(1, 0, 0), /* r1=r0 23f */
    LDR(0, 1, 0), /* r0=*r1 240 */
    BR(0x2, 3), /* 241 */
    TRAP(0x21), /* OUT 242 */
    ADDIMM(1, 1, 1), /* r1++ 243 */
    BR(0x7, -5), /* 244 */
    LDR(1, 6, 0), /* 245 restore r1 */
    ADDIMM(6, 6, 1), /* 246 pop */
    LDR(0, 6, 0), /* 247 restore r0 */
    ADDIMM(6, 6, 1), /* 248 pop */
    RTI(), /* 249 */
    /* OUT TRAP */
    ADDIMM(6, 6, -1), /* push 24a */
    STR(1, 6, 0), /* save r1 24b */
    LDI(1, 5), /* load DSR 24c */
    BR(0x3, -2), /* 24d */
    STI(0, 4), /* 24e */
    LDR(1, 6, 0), /* restore r1 24f */
    ADDIMM(6, 6, 1), /* pop 250 */
    RTI(), /* 251 */
    OS_DSR, /* 252 */
    OS_DDR, /* 253 */
    /* GETC TRAP */
    LDI(0, 3), /* 254 */
    BR(0b011, -2), /* 255 */
    LDI(0, 2), /* 256 */
    RTI(), /* 257 */
    OS_KBSR, /* 258 */
    OS_KBDR, /* 259 */
    /* IN TRAP */
    LEA(0, 11), /* 25a */
    TRAP(0x22), /* PUTS 25b */
    TRAP(0x20), /* GETC 25c */
    TRAP(0x21), /* OUT 25d */
    /* write newline */
    ADDIMM(6, 6, -1), /* push 25e */
    STR(0, 6, 0), /* save r0 25f */
    ANDIMM(0, 0, 0), /* 260 */
    ADDIMM(0, 0, 10), /* R0 = newline, 261 */
    TRAP(0x21), /* OUT 262 */
    LDR(0, 6, 0), /* restore r0 263 */
    ADDIMM(6, 6, 1), /* pop 264 */
    RTI(), /* 265 */
    'E', /* 266 */
    'n', /* 267 */
    't', /* 268 */
    'e', /* 269 */
    'r', /* 270 */
    ' ', /* 271 */
    'a', /* 272 */
    ' ', /* 273 */
    'C', /* 274 */
    'h', /* 275 */
    'a', /* 276 */
    'r', /* 277 */
    'a', /* 278 */
    'c', /* 279 */
    't', /* 27a */
    'e', /* 27b */
    'r', /* 27c */
    ':', /* 27d */
    ' ', /* 27e */
     0, /* 27f */
    /* PUTSP TRAP */

    RTI(), /* 280 */
    /* INTERRUPT METHODS */
    /* Priv mode Exception */
    LEA(0, 2),
    TRAP(0x22),
    TRAP(0x25),
    '\n',
    '\n',
    'P',
    'r',
    'i',
    'v',
    'i',
    'l',
    'e',
    'g',
    'e',
    ' ',
    'm',
    'o',
    'd',
    'e',
    ' ',
    'e',
    'x',
    'c',
    'e',
    'p',
    't',
    'i',
    'o',
    'n',
    '!',
    '\n',
    '\n',
    0,
    /* illegal instruction exception */
    LEA(0, 2),
    TRAP(0x22),
    TRAP(0x25),
    '\n',
    '\n',
    'I',
    'l',
    'l',
    'e',
    'g',
    'a',
    'l',
    ' ',
    'i',
    'n',
    's',
    't',
    'r',
    'u',
    'c',
    't',
    'i',
    'o',
    'n',
    ' ',
    'e',
    'x',
    'c',
    'e',
    'p',
    't',
    'i',
    'o',
    'n',
    '!',
    '\n',
    '\n',
    0,
    /* Access Violation Exception */
    LEA(0, 2),
    TRAP(0x22),
    TRAP(0x25),
    '\n',
    '\n',
    'A',
    'c',
    'e',
    's',
    's',
    ' ',
    'V',
    'i',
    'o',
    'l',
    'a',
    't',
    'i',
    'o',
    'n',
    ' ',
    'E',
    'x',
    'c',
    'e',
    'p',
    't',
    'i',
    'o',
    'n',
    '!',
    '\n',
    '\n',
    0
};

#undef BAD_TRAP
#undef PUTS_TRAP
#undef HALT_TRAP

#undef RET
#undef TRAP
#undef JMP
#undef LEA
#undef ADDIMM
#undef ADDR
#undef ANDR
#undef ANDIMM

static int16_t sext5(uint16_t input)
{
    if (input & (1 << 4))
        return input | ~0b11111;
    else
        return input;
}

static int16_t sext9(uint16_t input)
{
    if (input & (1 << 8))
        return input | ~0b111111111;
    else
        return input;
}

static int16_t sext11(uint16_t input)
{
    if (input & (1 << 10))
        return input | ~0b11111111111;
    else
        return input;
}

static int16_t sext6(uint16_t input)
{
    if (input & (1 << 5))
        return input | ~0b111111;
    else
        return input;
}

static void dump_registers(uint16_t *registers, uint16_t psr, uint16_t pc)
{
    printf("R0=%#x R1=%#x R2=%#x R3=%#x R4=%#x R5=%#x R6=%#x R7=%#x\n",
           registers[0], registers[1], registers[2], registers[3],
           registers[4], registers[5], registers[6], registers[7]);
    printf("psr=%#x pc=%#x\n\n", psr, pc);
}

static void dump_instr(uint16_t instr)
{
    const char* Rnames[8] = {
        "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7"
    };
    const char* nNames[2] = {
        "", "n"
    };
    const char* zNames[2] = {
        "", "z"
    };
    const char* pNames[2] = {
        "", "p"
    };


    printf("instr: %#x\n", instr);

    uint8_t opcode = instr >> 12;
    switch (opcode)
    {
        case 0b1111:
        {
            printf("instr: TRAP %#x\n", instr & 0xff);
            return;
        }
        case 0b0001:
        {
            uint16_t sr2, sr1, dr;

            dr = (instr & (0b111 << 9)) >> 9;
            sr1 = (instr & (0b111 << 6)) >> 6;

            if (instr & (1 << 5))
            {
                /* imm5 */
                sr2 = instr & 0b11111;
                printf("instr: %s = %s + %d\n", Rnames[dr], Rnames[sr1], sext5(sr2));
            } else {
                /* SR2 */
                sr2 = instr & 0b111;
                printf("instr: %s = %s + %s\n", Rnames[dr], Rnames[sr1], Rnames[sr2]);
            }

            return;
        }
        case 0b0101:
        {
            uint16_t sr2, sr1, dr;

            dr = (instr & (0b111 << 9)) >> 9;
            sr1 = (instr & (0b111 << 6)) >> 6;

            if (instr & (1 << 5))
            {
                /* imm5 */
                sr2 = instr & 0b11111;
                printf("instr: %s = %s & %d\n", Rnames[dr], Rnames[sr1], sext5(sr2));
            } else {
                /* SR2 */
                sr2 = instr & 0b111;
                printf("instr: %s = %s & %s\n", Rnames[dr], Rnames[sr1], Rnames[sr2]);
            }

            return;
        }
        case 0b1001:
        {
            uint16_t sr1, dr;

            dr = (instr & (0b111 << 9)) >> 9;
            sr1 = (instr & (0b111 << 6)) >> 6;

            printf("instr: %s = ~%s\n", Rnames[dr], Rnames[sr1]);

            return;
        }
        case 0b1110:
        {
            uint16_t dr;

            dr = (instr & (0b111 << 9)) >> 9;

            printf("instr: %s = pc + %d\n", Rnames[dr], sext9(instr & 0b111111111));

            return;
        }
        case 0b0000:
        {
            uint8_t nzp = (instr & (0b111 << 9)) >> 9;

            printf("instr: BR%s%s%s %d\n", nNames[nzp >> 2], zNames[(nzp >> 1) & 1],
                   pNames[nzp & 1], sext9(instr & 0b111111111));

            return;
        }
        case 0b0010:
        {
            printf("instr: %s = *(pc + (%d))\n", Rnames[(instr & (0b111 << 9)) >> 9], sext9(instr & 0b111111111));
            return;
        }
        case 0b0011:
        {
            printf("instr: *(pc + (%d)) = %s\n", sext9(instr & 0b111111111), Rnames[(instr & (0b111 << 9)) >> 9]);
            return;
        }
        case 0b1010:
        {
            printf("instr: %s = **(pc + (%d))\n", Rnames[(instr & (0b111 << 9)) >> 9], sext9(instr & 0b111111111));
            return;
        }
        case 0b1011:
        {
            printf("instr: **(pc + (%d)) = %s\n", sext9(instr & 0b111111111), Rnames[(instr & (0b111 << 9)) >> 9]);
            return;
        }
        case 0b0110:
        {
            printf("instr: %s = *(%s + (%d))\n", Rnames[(instr & (0b111 << 9)) >> 9],
                   Rnames[(instr & (0b111 << 6)) >> 6], sext6(instr & 0b111111));
            return;
        }
        case 0b0111:
        {
            printf("instr: *(%s + (%d)) = %s\n", Rnames[(instr & (0b111 << 6)) >> 6], sext6(instr & 0b111111),
                                                 Rnames[(instr & (0b111 << 9)) >> 9]);
            return;
        }
        case 0b0100:
        {
            if (instr & (1 << 11))
            {
                printf("instr: JSR %d\n", sext11(instr & 0b11111111111));
            } else {
                printf("instr: JSRR %s\n", Rnames[(instr & (0b111 << 6)) >> 6]);
            }

            return;
        }
        case 0b1100:
        {
            printf("instr: JMP %s\n", Rnames[(instr & (0b111 << 6)) >> 6]);

            return;
        }
        case 0b1000:
        {
            printf("instr: RTI\n");

            return;
        }
        default:
        {
            return;
        }
    }
}

static uint16_t *parse_program_from_bin(const char *path, uint16_t *memory)
{
    struct stat sb;
    uint8_t *addr;
    uint16_t *base;
    int fd = open(path, O_RDONLY | O_CLOEXEC);

    if (fd == -1) return 0;

    if (fstat(fd, &sb) == -1)
    {
        close(fd);
        return 0;
    }

    addr = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    if (addr == MAP_FAILED)
    {
        close(fd);
        return 0;
    }

    /* FIXME: work around for endianness ? */
    base = memory + (addr[1] | (uint16_t)(addr[0] << 8));
    for (unsigned i = 0; i < sb.st_size - 3; i+=2)
    {
        base[i>>1] = (addr[i+3] | (uint16_t)(addr[i+2] << 8));
    }


    munmap(addr, sb.st_size);
    close(fd);

    return base;
}

static void interrupt(uint16_t *memory, uint16_t *registers, uint16_t **pc, uint8_t code)
{
    /* update PC */
    *pc = memory + memory[0x100 + code] - 1;
    /* save USP if in user mode */
    if (memory[OS_PSR] & (1u << 15))
    {
        /* setup the supervisor stack */
        memory[OS_USP] = registers[6];
        registers[6] = memory[OS_SSP];
        /* supervisor mode */
        memory[OS_PSR] &= ~(1u << 15);
    }
}

static int check_user_address(uint16_t psr, uint16_t address)
{
    /* check if in supervisor mode */
    if (~psr & (1u << 15)) return 0;

    /* out of bounds for user mode */
    return address < 0x3000 || address >= 0xfe00;
}

static void update_cond_code(int16_t value, uint16_t *memory)
{
    /* regenerate condition codes */
    memory[OS_PSR] &= ~0b111;
    if (value < 0) memory[OS_PSR] |= FLAG_N;
    if (value == 0) memory[OS_PSR] |= FLAG_Z;
    if (value > 0) memory[OS_PSR] |= FLAG_P;
}

//#define LC3_EXTENDED

#ifdef LC3_EXTENDED

/* LC-3e: parse 2 word extended instructions */
static void parse_extended(uint16_t instr1, uint16_t instr2, uint16_t *memory, uint16_t *registers)
{
    uint16_t op = ((instr1 & 0x3) << 4) | (instr2 & (0xf << 12)) >> 12;
    uint16_t dst = (instr2 & (0b111 << 9)) >> 9;
    uint16_t sr1 = (instr2 & (0b111 << 6)) >> 6;
    uint16_t sr2 = instr2 & 0b111;
    int16_t imm5 = sext5(instr2 & 0b11111);

    switch (op)
    {
        case 0b000000: /* MUL (signed) */
        {
            if (instr2 & (1 << 5))
            {
                registers[dst] = registers[sr1] * imm5;
            } else {
                registers[dst] = (int16_t)registers[sr1] * (int16_t)registers[sr2];
            }

            break;
        }
        case 0b000001: /* DIV (signed) */
        {
            /* divide by 0 -> divide by 1 to avoid creating new exceptions */
            if (instr2 & (1 << 5))
            {
                if (imm5 == 0) imm5 = 1;
                registers[dst] = registers[sr1] / imm5;
            } else {
                int16_t v = registers[sr2];
                if (v == 0) v = 1;
                registers[dst] = (int16_t)registers[sr1] / v;
            }

            break;
        }
        case 0b000010: /* RSHIFT +/- imm6 */
        {
            break;
        }
        case 0b000100: /* XCHG (exchange) */
        {

            break;
        }
        case 0b0010000: /* OR */
        {

            break;
        }
        case 0b0010001: /* XOR */
        {

            break;
        }
        default:
        {
            /* TODO: throw invalid instruction exception */
            break;
        }
    }
}

#endif

int main(int argc, const char **argv)
{
    uint16_t *pc;
    /* LC-3 can only address [0, 0xffff] but we have extra few values for ssp, usp, pc, registers */
    uint16_t *memory = calloc(0x10000 + 2, sizeof(uint16_t));
    uint16_t registers[8] = {0};

    int ddrsize = 0x100;
    char *buffer = calloc(ddrsize, sizeof(char));
    int ddrct = 0;

    memcpy(memory, OSProgram, sizeof(OSProgram));

    /* parse additional programs/data */
    for (int i = 1; i < argc-1; i++)
    {
        if (!parse_program_from_bin(argv[i], memory))
        {
            fprintf(stderr, "Failed to load %s\n", argv[i]);
            continue;
        }
    }

    /* last program is what we set PC to */
    if (!(argc >= 2 && (pc = parse_program_from_bin(argv[argc-1], memory))))
    {
        fprintf(stderr, "No program specified!\n");
        return 1;
    }

    /* overwrite the OS memory to use the start address of the given program */
    /* FIXME: is this correct? */
    memory[USER_PC] = pc - memory;
    pc = memory + OS_START;

    /* enable the clock */
    memory[OS_MCR] |= (1u << 15);
    /* we are ready for some data */
    memory[OS_DSR] |= (1u << 15);
    /* reset ddr */
    memory[OS_DDR] = 0;

    /* terminate the emulator if clock gets disabled */
    while (memory[OS_MCR] & (1u << 15))
    {
        dump_instr(*pc);
        dump_registers(registers, memory[OS_PSR], pc - memory);

        switch ((*pc & 0xf000) >> 12)
        {
            case 0b0001: /* ADD */
            {
                uint16_t sr2;
                if (*pc & (1 << 5))
                {
                    /* imm5 */
                    sr2 = sext5(*pc & 0b11111);
                } else {
                    /* SR2 */
                    sr2 = registers[*pc & 0b111];
                }

                registers[(*pc & (0b111 << 9)) >> 9] = registers[(*pc & (0b111 << 6)) >> 6] + sr2;

                update_cond_code(registers[(*pc & (0b111 << 9)) >> 9], memory);

                break;
            }
            case 0b0101: /* AND */
            {
                uint16_t sr2;
                if (*pc & (1 << 5))
                {
                    /* imm5 */
                    sr2 = sext5(*pc & 0b11111);
                } else {
                    /* SR2 */
                    sr2 = registers[*pc & 0b111];
                }

                registers[(*pc & (0b111 << 9)) >> 9] = registers[(*pc & (0b111 << 6)) >> 6] & sr2;

                update_cond_code(registers[(*pc & (0b111 << 9)) >> 9], memory);

                break;
            }
            case 0b1001: /* NOT */
            {
                registers[(*pc & (0b111 << 9)) >> 9] = ~registers[(*pc & (0b111 << 6)) >> 6];

                update_cond_code(registers[(*pc & (0b111 << 9)) >> 9], memory);

                break;
            }
            case 0b1111: /* TRAP */
            {
                uint16_t temp = memory[OS_PSR];
                /* check user mode */
                if (memory[OS_PSR] & (1u << 15))
                {
                    memory[OS_USP] = registers[6];
                    registers[6] = memory[OS_SSP];
                    memory[OS_PSR] &= ~(1u << 15);
                }

                /* push old PSR and PC */

                registers[6]--; /* push */
                memory[registers[6]] = temp;
                registers[6]--; /* push */
                memory[registers[6]] = pc - memory + 1;

                pc = memory + memory[*pc & 0xff] - 1; /* pc will be incremented later */

                break;
            }
            case 0b1110: /* LEA */
            {
                registers[(*pc & (0b111 << 9)) >> 9] = (pc - memory) + sext9(*pc & 0b111111111) + 1;
                update_cond_code(registers[(*pc & (0b111 << 9)) >> 9], memory);
                break;
            }
            case 0b1100: /* JMP */
            {
                pc = memory + (int16_t)registers[(*pc & (0b111 << 6)) >> 6] - 1;
                break;
            }
            case 0b0000: /* BR */
            {
                if (((*pc & (0b111 << 9)) >> 9) & (memory[OS_PSR] & 0b111))
                    pc += sext9(*pc & 0b111111111);
                break;
            }
            case 0b0100: /* JSR(R) */
            {
                registers[7] = pc - memory + 1;
                if (*pc & (1 << 11))
                {
                    /* imm11 */
                    pc += sext11(*pc & 0b11111111111) - 1;
                } else {
                    /* R */
                    pc += (int16_t)registers[(*pc & (0b111 << 6)) >> 6] - 1;
                }
                break;
            }
            case 0b0011: /* ST */
            {
                pc[sext9(*pc & 0b111111111) + 1] = registers[(pc[1] & (0b111 << 9)) >> 9];
                break;
            }
            case 0b1011: /* STI */
            {
                /* TODO check pc + offset address */
                uint16_t address = pc[sext9(*pc & 0b111111111) + 1];
                if (check_user_address(memory[OS_PSR], address))
                    interrupt(memory, registers, &pc, 0x2);
                else
                {
                    memory[address] = registers[(*pc & (0b111 << 9)) >> 9];
                    if (address == OS_DDR)
                    {
                        buffer[ddrct++] = memory[OS_DDR];
                        if (ddrct >= ddrsize)
                        {
                            ddrsize += 0x100;
                            buffer = realloc(buffer, ddrsize);
                        }
                    }
                }

                break;
            }
            case 0b0111: /* STR */
            {
                uint16_t address = registers[(*pc & (0b111 << 6)) >> 6] + sext6(*pc & 0b111111);
                if (check_user_address(memory[OS_PSR], address))
                    interrupt(memory, registers, &pc, 0x2);
                else
                    memory[address] = registers[(*pc & (0b111 << 9)) >> 9];
                break;
            }
            case 0b0010: /* LD */
            {
                /* TODO check pc + offset address */
                registers[(*pc & (0b111 << 9)) >> 9] = pc[sext9(*pc & 0b111111111) + 1];
                update_cond_code(registers[(*pc & (0b111 << 9)) >> 9], memory);
                break;
            }
            case 0b1010: /* LDI */
            {
                /* TODO check pc + offset address */
                uint16_t address = pc[sext9(*pc & 0b111111111) + 1];
                if (check_user_address(memory[OS_PSR], address))
                    interrupt(memory, registers, &pc, 0x2);
                else
                {
                    registers[(*pc & (0b111 << 9)) >> 9] = memory[address];
                    update_cond_code(registers[(*pc & (0b111 << 9)) >> 9], memory);
                }
                break;
            }
            case 0b0110: /* LDR */
            {
                uint16_t address = registers[(*pc & (0b111 << 6)) >> 6] + sext6(*pc & 0b111111);
                if (check_user_address(memory[OS_PSR], address))
                    interrupt(memory, registers, &pc, 0x2);
                else
                {
                    registers[(*pc & (0b111 << 9)) >> 9] = memory[address];
                    update_cond_code(registers[(*pc & (0b111 << 9)) >> 9], memory);
                }
                break;
            }
            case 0b1000: /* RTI */
            {
                if (~memory[OS_PSR] & (1 << 15))
                {
                    pc = memory + memory[registers[6]] - 1;
                    registers[6]++; /* pop */
                    memory[OS_PSR] = memory[registers[6]];
                    registers[6]++; /* pop */
                    if (memory[OS_PSR] & (1 << 15))
                    {
                        /* setup user stack */
                        memory[OS_SSP] = registers[6];
                        registers[6] = memory[OS_USP];
                    }
                } else {
                    /* throw exception */
                    interrupt(memory, registers, &pc, 0x0);
                }
                break;
            }
            case 0b1101:
            {
#ifndef LC3_EXTENDED
                /* illegal instruction exception */
                interrupt(memory, registers, &pc, 0x1);
#else
                parse_extended(*pc, *(pc+1), memory, registers);
                pc++;
#endif
                break;
            }
            default:
            {
                fprintf(stderr, "unimplemented instruction %x\n", *pc & 0xf000);
                free(memory);
                free(buffer);
                return 1;
            }
        }

        //printf("memory[0x4000]=%d\n", (int16_t)memory[0x4000]);
        //printf("memory[0x4001]=%d\n", (int16_t)memory[0x4001]);

        printf("buffer: %s \n\n --- buffer end --- \n\n", buffer);

        pc++;
        memory[OS_MCC]++;
        //getchar();
    }

    printf("The clock was disabled!\n\n");

finish:
    free(buffer);
    free(memory);
    return 0;
}
