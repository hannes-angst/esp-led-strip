/*
 * Parses the RGB string array.
 */
#include <user_interface.h>
#include <osapi.h>
#include <c_types.h>
#include "parse.h"

static int ICACHE_FLASH_ATTR readHexDigit(char value) {
	if (value < 48) {
		return 0;
	}

	// 0...9
	if (value <= 57) {
		return value - 48;
	}

	// between digits
	if (value > 57 && value < 65) {
		return 9;
	}

	//a..f
	if (value > 96 && value < 103) {
		return value - 87;
	}

	//more than F
	if (value > 70) {
		return 15;
	}

	//A..F
	return value - 55;
}

static void ICACHE_FLASH_ATTR readColor(char dataBuf[7], BOOL dry, struct color_t *color) {
	int r = readHexDigit(dataBuf[0]) * 16 + readHexDigit(dataBuf[1]);
	int g = readHexDigit(dataBuf[2]) * 16 + readHexDigit(dataBuf[3]);
	int b = readHexDigit(dataBuf[4]) * 16 + readHexDigit(dataBuf[5]);

	uint8_t r_val = (int8_t) (r > 255 ? 255 : r < 0 ? 0 : r);
	uint8_t g_val = (int8_t) (g > 255 ? 255 : g < 0 ? 0 : g);
	uint8_t b_val = (int8_t) (b > 255 ? 255 : b < 0 ? 0 : b);

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
		while (i < slots->length) {
			DEBUG("slot[%d]: %d, %d, %d\r\n", i, slots->slot[i].r, slots->slot[i].g, slots->slot[i].b);
			i++;
		}
	}
	return result;
}
