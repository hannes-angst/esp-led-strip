/*
 * RGB string array parser
 */
#ifndef PARSE_H_
#define PARSE_H_

#include <user_interface.h>
#include <osapi.h>
#include <c_types.h>
#include "user_config.h"

typedef struct color_t {
	uint32_t r;
	uint32_t g;
	uint32_t b;
} color_t;

typedef struct color_slots
{
	color_t slot[MAX_SLOTS];
	uint8_t length;
} color_slots;



enum parse_result_type {
	SUCCESS = 0,
	ERR_PARSE = 1,
	ERR_TOO_MANY_SLOTS = 2,
	ERR_TOO_MANY_CHARS = 3
};

enum parse_result_type ICACHE_FLASH_ATTR readColors(char *input, uint16_t length, struct color_slots *slots);

#endif /* PARSE_H_ */
