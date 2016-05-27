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

#ifndef	TARGET_H
#define	TARGET_H

#define	TARGET_BARS	(2)

struct target_bar {
	bool tb_enabled;

	uint64_t tb_base;
	uint64_t tb_length;

	uintptr_t tb_virtual;
};

struct target {
	const char *t_model;
	unsigned t_unit;

	uint32_t t_pci_domain;
	uint8_t t_pci_bus;
	uint8_t t_pci_slot;
	uint8_t t_pci_function;

	struct target_bar t_pci_bar[TARGET_BARS];

	uint8_t t_pcie_port;

	uint32_t t_chip_id;
	uint64_t t_core_mask;

	uint16_t t_board_type;
};

struct target_selector {
	uint32_t ts_mask;
};

#define	TARGET_SELECT(ts, n)		((ts)->ts_mask |= 1u << (n))
#define	TARGET_SELECTED(ts, n)		(((ts)->ts_mask & 1u << (n)) != 0)
#define	TARGET_SELECTED_COUNT(ts)	(__builtin_popcount((ts)->ts_mask))
#define	TARGET_SELECTOR_EMPTY(ts)	((ts)->ts_mask == 0)
#define	TARGET_SELECTOR_CLEAR(ts)	((ts)->ts_mask = 0)
#define	TARGET_SELECTOR_COUNT		(32)

/* Configuration.  */
struct target_selector target_identify(void);

/* High-level operations.  */
void target_boot(const struct target_selector *);
void target_reset(const struct target_selector *);
void target_show(const struct target_selector *);

/* Low-level operations.  */
uint64_t target_read_csr(const struct target *, uint64_t);
void target_write_csr(const struct target *, uint64_t, uint64_t);

#endif /* !TARGET_H */
