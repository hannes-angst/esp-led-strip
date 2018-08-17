/*
 * Parses the RGB string array.
 */
#include <user_interface.h>
#include <osapi.h>
#include <c_types.h>
#include "user_config.h"
#include "parse.h"

static int ICACHE_FLASH_ATTR readDigit(char value) {
	if (value < 48) {
		return 0;
	}

	// 0...9
	if (value <= 57) {
		return value - 48;
	}

	// between digits
	if (value > 57 && value < 64) {
		return 9;
	}

	if (value > 70) {
		return 15;
	}

	return value - 55;
}

const uint32_t gammut[] = {
    0,   1,   1,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,   3,   3,   3,
    3,   3,   3,   3,   3,   3,   3,   4,   4,   4,   4,   4,   4,   4,   4,   4,
    4,   4,   4,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   6,   6,   6,
    6,   6,   6,   6,   6,   7,   7,   7,   7,   7,   7,   7,   8,   8,   8,   8,
    8,   8,   9,   9,   9,   9,   9,   9,   10,  10,  10,  10,  10,  11,  11,  11,
    11,  11,  12,  12,  12,  12,  12,  13,  13,  13,  13,  14,  14,  14,  14,  15,
    15,  15,  16,  16,  16,  16,  17,  17,  17,  18,  18,  18,  19,  19,  19,  20,
    20,  20,  21,  21,  22,  22,  22,  23,  23,  24,  24,  25,  25,  25,  26,  26,
    27,  27,  28,  28,  29,  29,  30,  30,  31,  32,  32,  33,  33,  34,  35,  35,
    36,  36,  37,  38,  38,  39,  40,  40,  41,  42,  43,  43,  44,  45,  46,  47,
    48,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,
    63,  64,  65,  66,  68,  69,  70,  71,  73,  74,  75,  76,  78,  79,  81,  82,
    83,  85,  86,  88,  90,  91,  93,  94,  96,  98,  99,  101, 103, 105, 107, 109,
    110, 112, 114, 116, 118, 121, 123, 125, 127, 129, 132, 134, 136, 139, 141, 144,
    146, 149, 151, 154, 157, 159, 162, 165, 168, 171, 174, 177, 180, 183, 186, 190,
    193, 196, 200, 203, 207, 211, 214, 218, 222, 226, 230, 234, 238, 242, 248, 255,
};

static void ICACHE_FLASH_ATTR readColor(char dataBuf[7], BOOL dry,  struct color_t *color) {
	int r = readDigit(dataBuf[0]) * 16 + readDigit(dataBuf[1]);
	int g = readDigit(dataBuf[2]) * 16 + readDigit(dataBuf[3]);
	int b = readDigit(dataBuf[4]) * 16 + readDigit(dataBuf[5]);

	uint32_t r_val = (uint32_t)(gammut[r > 255 ? 255 : r < 0 ? 0 : r] * MAX_PERIOD / 255);
	uint32_t g_val = (uint32_t)(gammut[g > 255 ? 255 : g < 0 ? 0 : g] * MAX_PERIOD / 255);
	uint32_t b_val = (uint32_t)(gammut[b > 255 ? 255 : b < 0 ? 0 : b] * MAX_PERIOD / 255);

	if (dry) {
		DEBUG("Setting (dry) r,g,b to %d, %d, %d\r\n", r_val, g_val, b_val);
	} else {
		DEBUG("Setting r,g,b to %d, %d, %d\r\n", r_val, g_val, b_val);
		color->g = g_val;
		color->b = b_val;
		color->r = r_val;
	}
}

/**
 *
 */
static enum parse_result_type ICACHE_FLASH_ATTR parseColorsFromInput(char *data, uint32_t length, struct color_slots *slots, BOOL dry) {
	int pos = 0;
	char buf[7];
	int bufPos = 0;
	int slotPos = 0;
	/**
	 * State:
	 * 0: start
	 * 1: block start
	 * 2: in block
	 * 3: block end
	 * 4: end
	 */
	int state = 0;
	while (pos < length) {
		if (data[pos] < 0x21) {
			//Ignore white spaces
			pos++;
			continue;
		}
		if (state == 0) {
			if (data[pos] == 0x5B) {
				// [
				DEBUG("s(%d) -> %d\r\n", state, state + 1);
				state++;
				bufPos = 0;
			} else {
				WARN("unexpected input at %d\r\n", pos);
				break;
			}
		} else if (state == 1) {
			if (data[pos] == 0x22 || data[pos] == 0x27) {
				// " or '
				DEBUG("s(%d) -> %d\r\n", state, state + 1);
				state++;
				bufPos = 0;
			} else if (data[pos] == 0x5D) {
				// ]
				DEBUG("s(%d) -> %d\r\n", state, 4);
				state = 4;
			} else {
				WARN("unexpected input at %d\r\n", pos);
				break;
			}
		} else if (state == 2) {
			if (data[pos] >= 0x30 && data[pos] <= 0x39) {
				//0...9
				buf[bufPos] = data[pos];
				DEBUG("+(%d)> %c\r\n", bufPos, data[pos]);
				bufPos++;
				if (bufPos > 6) {
					WARN("Too many chars");
					return ERR_TOO_MANY_CHARS;
				}
			} else if (data[pos] >= 0x41 && data[pos] <= 0x46) {
				//A...F
				buf[bufPos] = data[pos];
				DEBUG("+(%d)> %c\r\n", bufPos, data[pos]);
				bufPos++;
				if (bufPos > 6) {
					WARN("Too many chars");
					return ERR_TOO_MANY_CHARS;
				}
			} else if (data[pos] == 0x22 || data[pos] == 0x27) {
				// " or '
				if (bufPos != 6) {
					WARN("unexpected color value at %d\r\n", pos - bufPos);
					return ERR_PARSE;
				}
				buf[bufPos] = '\0';
				if (dry) {
					DEBUG("Value (dry): %s\r\n", buf);
				} else {
					DEBUG("Value: %s\r\n", buf);
				}
				readColor(buf, dry, &(slots->slot[slotPos]));
				slotPos++;

				if (slotPos > MAX_SLOTS) {
					WARN("Too many slots");
					return ERR_TOO_MANY_SLOTS;
				}
				DEBUG("s(%d) -> %d\r\n", state, state + 1);
				state++;
				bufPos = 0;
			} else {
				WARN("unexpected input at %d\r\n", pos);
				return ERR_PARSE;
			}
		} else if (state == 3) {
			if (data[pos] == 0x2C) {
				// ,
				DEBUG("s(%d) -> %d\r\n", state, 1);
				state = 1;
				bufPos = 0;
			} else if (data[pos] == 0x5D) {
				// ]
				DEBUG("s(%d) -> %d\r\n", state, state + 1);
				state++;
			} else {
				WARN("unexpected input at %d\r\n", pos);
				return ERR_PARSE;
			}
		}
		pos++;
	}
	if (state == 4) {
		if (dry) {
			DEBUG("New slot size would be %d.\r\n", slotPos);
		} else {
			slots->length = slotPos;
			DEBUG("New slot size: %d\r\n", slots->length);
		}
		DEBUG("Success\r\n");
		return SUCCESS;
	}

	WARN("Parse error.\r\n");
	return ERR_PARSE;
}

enum parse_result_type ICACHE_FLASH_ATTR readColors(char *input, uint16_t length, struct color_slots *slots) {
	DEBUG("> %s\r\n", input);

    //dry run
	enum parse_result_type result = parseColorsFromInput(input, strlen(input), slots, true);
	if (result == SUCCESS) {
		result = parseColorsFromInput(input, strlen(input), slots, false);
	}

	if (result == SUCCESS) {
		uint8_t i = 0;
		while(i < slots->length) {
			DEBUG("slot[%d]: %d, %d, %d\r\n", i, slots-> slot[i].r, slots-> slot[i].g, slots-> slot[i].b);
			i++;
		}
	}
	return result;
}
