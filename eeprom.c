/*
 * Copyright (c) 2015-2016 Juli Mallett. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/endian.h>
#include <assert.h>
#include <stdbool.h>

#include <cvmx.h>
#include <cvmx-twsi.h>

#include "eeprom.h"
#include "target.h"

#ifndef	howmany
#define	howmany(a)	(sizeof (a) / sizeof *(a))
#endif

/*
 * EEPROM addresses to scan looking for tuples.
 */
static const uint8_t eeprom_tuple_scan[] = {
	0x52, 0x53, 0x56, 0x54
};

static bool eeprom_tuple_find(uint8_t, uint16_t, uint8_t, void *, size_t);

bool
eeprom_board_desc_read(struct eeprom_board_desc *ebd)
{
	unsigned i;

	for (i = 0; i < howmany(eeprom_tuple_scan); i++) {
		if (!eeprom_tuple_find(eeprom_tuple_scan[i], EEPROM_TUPLE_TYPE_BOARD_DESC, EEPROM_BOARD_DESC_MAJOR, ebd, sizeof *ebd))
			continue;

		ebd->ebd_board_type = be16toh(ebd->ebd_board_type);
		return (true);
	}
	return (false);
}

static bool
eeprom_tuple_find(uint8_t twsi, uint16_t type, uint8_t major, void *data, size_t len)
{
	struct eeprom_tuple_header eth;
	uint8_t eth_data[sizeof eth];
	uint8_t raw_data[256];
	uint16_t start;
	unsigned i;

	assert(sizeof raw_data >= len);

	start = 0;

	for (;;) {
		for (i = 0; i < sizeof eth_data; i++)
			eth_data[i] = (uint8_t)cvmx_twsix_read_ia16(0, twsi, start + i, 1);

		memcpy(&eth, eth_data, sizeof eth);
		eth.eth_type = be16toh(eth.eth_type);
		eth.eth_length = be16toh(eth.eth_length);
		eth.eth_checksum = be16toh(eth.eth_checksum);

		if (eth.eth_type == EEPROM_TUPLE_TYPE_END)
			return (false);

		start += sizeof eth;
		eth.eth_length -= sizeof eth;

		if (eth.eth_type == type && eth.eth_major == major &&
		    eth.eth_length == len) {
			for (i = 0; i < len; i++)
				raw_data[i] = (uint8_t)cvmx_twsix_read_ia16(0, twsi, start + i, 1);

			memcpy(data, raw_data, len);
			return (true);
		}

		start += eth.eth_length;
	}
}
