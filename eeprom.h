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

#ifndef	EEPROM_H
#define	EEPROM_H

#define	EEPROM_TUPLE_TYPE_BOARD_DESC	(0x0002)
#define	EEPROM_TUPLE_TYPE_MAC_ADDR	(0x0004)
#define	EEPROM_TUPLE_TYPE_END		(0xffff)

struct eeprom_tuple_header {
	uint16_t eth_type;
	uint16_t eth_length;
	uint8_t eth_minor;
	uint8_t eth_major;
	uint16_t eth_checksum;
};

/*
 * Board description.
 */
#define	EEPROM_BOARD_DESC_MAJOR	(1)

#define	EEPROM_BOARD_DESC_SERIAL_LEN	(20)
struct eeprom_board_desc {
	uint16_t ebd_board_type;
	uint8_t ebd_major;
	uint8_t ebd_minor;
	uint16_t ebd_unused1;
	uint8_t ebd_unused2;
	uint8_t ebd_unused3;
	uint8_t ebd_serial[EEPROM_BOARD_DESC_SERIAL_LEN];
};

bool eeprom_board_desc_read(struct eeprom_board_desc *);

/*
 * MAC address allocation.
 */
#define	EEPROM_MAC_ADDR_MAJOR	(1)

struct eeprom_mac_addr {
	uint8_t ebd_base[6];
	uint8_t ebd_count;
};

#endif /* !EEPROM_H */
