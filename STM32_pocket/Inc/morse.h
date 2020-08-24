#ifdef INCLUDE_MORSE

// YACK - Yet Another CW Keyer
// Jan Lategahn DK3LJ jan@lategahn.com (C) 2011

//! Encoding: Each byte is read from the left. 0 stands for a dot, 1
//! stands for a dash. After each played element the content is shifted
//! left. Playback stops when the leftmost bit contains a "1" and the rest
//! of the bits are all zero.
//!
//! Example: A = .-
//! Encoding: 01100000
//!           .-
//!             | This is the stop marker (1 with all trailing zeros)

static const uint8_t morse[] =
{

	0b11111100, // 0
	0b01111100, // 1
	0b00111100, // 2
	0b00011100, // 3
	0b00001100, // 4
	0b00000100, // 5
	0b10000100, // 6
	0b11000100, // 7
	0b11100100, // 8
	0b11110100, // 9
	0b01100000, // A
	0b10001000, // B
	0b10101000, // C
	0b10010000, // D
	0b01000000, // E
	0b00101000, // F
	0b11010000, // G
	0b00001000, // H
	0b00100000, // I
	0b01111000, // J
	0b10110000, // K
	0b01001000, // L
	0b11100000, // M
	0b10100000, // N
	0b11110000, // O
	0b01101000, // P
	0b11011000, // Q
	0b01010000, // R
	0b00010000, // S
	0b11000000, // T
	0b00110000, // U
	0b00011000, // V
	0b01110000, // W
	0b10011000, // X
	0b10111000, // Y
	0b11001000, // Z
	0b00110010, // ?
	0b10001100, // =
	0b01010110, // .
	0b00010110, // SK
	0b01010100, // AR
	0b10010100  // /
};


// The special characters at the end of the above table can not be decoded
// without a small table to define their content. # stands for SK, $ for AR

// To add new characters, add them in the code table above at the end and below
// Do not forget to increase the legth of the array..

const char spechar[6] = "?=.#$/";

#endif
