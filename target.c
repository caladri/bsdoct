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
#include <sys/mman.h>
#include <sys/pciio.h>
#include <dev/pci/pcireg.h>
#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <cvmx.h>
#include <cvmx-ciu-defs.h>

#include "cvmx_compat.h"
#include "eeprom.h"
#include "target.h"

#ifndef	howmany
#define	howmany(a)	(sizeof (a) / sizeof *(a))
#endif

#define	MAX_TARGET_UNITS	(16)
#define	MAX_TARGET_MATCHES	(1024)

static struct target target_units[MAX_TARGET_UNITS];
static unsigned target_unit_next;

#define	TARGET_SELECTED_EACH(ts, a)					\
	do {								\
		unsigned n;						\
									\
		for (n = 0; n < TARGET_SELECTOR_COUNT; n++) {		\
			struct target *t;				\
									\
			if (!TARGET_SELECTED((ts), (n)))		\
				continue;				\
			t = &target_units[n];				\
			assert(t->t_model != NULL);			\
									\
			cvmx_select_target(t);				\
			a;						\
			cvmx_select_target(NULL);			\
		}							\
	} while (0)

/*
 * For some reason Cavium's tools refer to BAR2 as BAR1.
 * Maintain that fiction here for ease of making-sense by
 * users.  Map logical BAR numbers to the actual registers.
 */
static const unsigned target_pci_bar_regs[TARGET_BARS] = {
	PCIR_BAR(0),
	PCIR_BAR(2)
};

/*
 * Provide 32-bit little-endian access to resources in
 * BAR0.
 */
static inline uint64_t
target_bar0_read8(const struct target *t, uint64_t addr)
{
	volatile uint32_t *p;
	uint64_t hi, lo;

	assert(addr + 8 <= t->t_pci_bar[0].tb_length);

	p = (void *)(t->t_pci_bar[0].tb_virtual + addr);

	hi = le32toh(p[1]);
	lo = le32toh(p[0]);

	return (hi << 32 | lo);
}

static inline void
target_bar0_write8(const struct target *t, uint64_t addr, uint64_t data)
{
	volatile uint32_t *p;
	uint32_t hi, lo;

	assert(addr + 8 <= t->t_pci_bar[0].tb_length);

	p = (void *)(t->t_pci_bar[0].tb_virtual + addr);

	/*
	 * NB:
	 * We always write the low word last, since typically
	 * it is write to the low address that triggers the
	 * result of this write, while write to the high
	 * address merely sets up data.
	 */
	hi = (uint32_t)(data >> 32);
	lo = (uint32_t)data;
	p[1] = htole32(hi);
	p[0] = htole32(lo);
}

struct target_pci_id {
	uint16_t tpi_vendor;
	uint16_t tpi_device;
	uint32_t tpi_chip_id_base;
	const char *tpi_model;
};

static const struct target_pci_id target_pci_ids[] = {
	{ 0x177d, 0x0092, OCTEON_CN66XX_PASS1_0, "Cavium Octeon CN66XX" }
};

static int target_pci_fd = -1;
static int target_mem_fd = -1;

static void target_boot_one(struct target *);
static void target_reset_one(struct target *);
static void target_show_one(struct target *);
static struct target *target_attach(const struct pci_conf *);

struct target_selector
target_identify(void)
{
	struct pci_match_conf pmc[howmany(target_pci_ids)];
	struct pci_conf pcs[MAX_TARGET_MATCHES];
	struct target_selector all;
	struct pci_conf_io pci;
	unsigned i;
	int rv;

	memset(&pci, 0, sizeof pci);

	if (target_pci_fd == -1) {
		target_pci_fd = open("/dev/pci", O_RDONLY);
		if (target_pci_fd == -1)
			err(1, "open /dev/pci");
	}

	pci.match_buf_len = sizeof pcs;
	pci.matches = pcs;

	for (i = 0; i < howmany(target_pci_ids); i++) {
		pmc[i].pc_vendor = target_pci_ids[i].tpi_vendor;
		pmc[i].pc_device = target_pci_ids[i].tpi_device;
		pmc[i].flags = PCI_GETCONF_MATCH_VENDOR | PCI_GETCONF_MATCH_DEVICE;
	}

	pci.num_patterns = howmany(pmc);
	pci.pat_buf_len = sizeof pmc;
	pci.patterns = pmc;

	rv = ioctl(target_pci_fd, PCIOCGETCONF, &pci);
	if (rv == -1)
		err(1, "ioctl PCIOCGETCONF");

	TARGET_SELECTOR_CLEAR(&all);

	for (i = 0; i < pci.num_matches; i++) {
		struct target *t;

		t = target_attach(&pci.matches[i]);
		if (t == NULL)
			continue;

		TARGET_SELECT(&all, t->t_unit);
	}

	return (all);
}

void
target_boot(const struct target_selector *ts)
{
	TARGET_SELECTED_EACH(ts, target_boot_one(t));
}

void
target_reset(const struct target_selector *ts)
{
	TARGET_SELECTED_EACH(ts, target_reset_one(t));
}

void
target_show(const struct target_selector *ts)
{
	TARGET_SELECTED_EACH(ts, target_show_one(t));
}

uint64_t
target_read_csr(const struct target *t, uint64_t addr)
{
	cvmx_sli_win_rd_addr_t swra;
	cvmx_sli_win_rd_data_t swrd;

	/*
	 * XXX
	 * In theory we should just be writing addr to rd_addr,
	 * but in practice this blows up the host!
	 */
	swra.u64 = addr;
	swra.s.ld_cmd = 3;
	target_bar0_write8(t, CVMX_SLI_WIN_RD_ADDR, swra.u64);

	/*
	 * XXX
	 * Is this wrong since we do two 32-bit reads, which
	 * would trigger two reads from memory?
	 *
	 * Cavium themselves use LAST_WIN_RDATA[01] to avoid
	 * the same.
	 */
	swrd.u64 = target_bar0_read8(t, CVMX_SLI_WIN_RD_DATA);
	return (swrd.s.rd_data);
}

void
target_write_csr(const struct target *t, uint64_t addr, uint64_t data)
{
	cvmx_sli_win_wr_addr_t swwa;
	cvmx_sli_win_wr_data_t swwd;
	cvmx_sli_win_wr_mask_t swwm;

	swwm.u64 = 0;
	swwm.s.wr_mask = 0xff; /* Write all 8 bytes.  */

	/*
	 * XXX
	 * Assume the same fault noted about for WIN_RD_ADDR
	 * is true of WIN_WR_ADDR!
	 */
	swwa.u64 = addr;
	target_bar0_write8(t, CVMX_SLI_WIN_WR_ADDR, swwa.u64);

	/*
	 * XXX
	 * It's the write to the low word that triggers the
	 * actual write, so we rely on the ordering of the
	 * writes in target_bar0_write8.
	 *
	 * Perhaps we should split the write here to make it
	 * more explicit, but so far not.
	 */
	swwd.u64 = 0;
	swwd.s.wr_data = data;
	target_bar0_write8(t, CVMX_SLI_WIN_WR_DATA, swwd.u64);
}

static struct target *
target_attach(const struct pci_conf *pc)
{
	const struct target_pci_id *tpi;
	struct eeprom_board_desc ebd;
	cvmx_mio_tws_sw_twsi_t mtst;
	cvmx_sli_ctl_status_t scs;
	cvmx_sli_mac_number_t smn;
	struct pci_bar_io pbi;
	cvmx_ciu_fuse_t cf;
	struct target *t;
	unsigned i;
	void *m;
	int rv;

	tpi = NULL;

	for (i = 0; i < howmany(target_pci_ids); i++) {
		if (pc->pc_vendor != target_pci_ids[i].tpi_vendor)
			continue;
		if (pc->pc_device != target_pci_ids[i].tpi_device)
			continue;
		tpi = &target_pci_ids[i];
		break;
	}

	assert(tpi != NULL);

	if (target_unit_next == MAX_TARGET_UNITS) {
		fprintf(stderr, "Already have %u units, cannot configure additional <%s>.\n", MAX_TARGET_UNITS, tpi->tpi_model);
		return (NULL);
	}

	t = &target_units[target_unit_next];
	t->t_model = tpi->tpi_model;
	t->t_unit = target_unit_next++;

	t->t_pci_domain = pc->pc_sel.pc_domain;
	t->t_pci_bus = pc->pc_sel.pc_bus;
	t->t_pci_slot = pc->pc_sel.pc_dev;
	t->t_pci_function = pc->pc_sel.pc_func;

	for (i = 0; i < TARGET_BARS; i++) {
		memset(&pbi, 0, sizeof pbi);

		pbi.pbi_sel = pc->pc_sel;
		pbi.pbi_reg = target_pci_bar_regs[i];

		assert(target_pci_fd != -1);

		rv = ioctl(target_pci_fd, PCIOCGETBAR, &pbi);
		if (rv == -1) {
			t->t_pci_bar[i].tb_enabled = false;
			continue;
		}

		t->t_pci_bar[i].tb_enabled = pbi.pbi_enabled != 0;
		if (!t->t_pci_bar[i].tb_enabled)
			continue;
		if (!PCI_BAR_MEM(pbi.pbi_base)) {
			fprintf(stderr, "target%u: BAR%u is not a memory BAR; disabling\n", t->t_unit, i);
			t->t_pci_bar[i].tb_enabled = false;
			continue;
		}
		t->t_pci_bar[i].tb_base = pbi.pbi_base & PCIM_BAR_MEM_BASE;
		t->t_pci_bar[i].tb_length = pbi.pbi_length;

		if (target_mem_fd == -1) {
			target_mem_fd = open("/dev/mem", O_RDWR);
			if (target_mem_fd == -1)
				err(1, "open /dev/mem");
		}

		m = mmap(NULL, t->t_pci_bar[i].tb_length, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_NOCORE, target_mem_fd, t->t_pci_bar[i].tb_base);
		if (m == MAP_FAILED) {
			fprintf(stderr, "target%u: BAR%u could not be mapped; disabling\n", t->t_unit, i);
			t->t_pci_bar[i].tb_enabled = false;
			continue;
		}

		t->t_pci_bar[i].tb_virtual = (uintptr_t)m;
	}

	smn.u64 = target_bar0_read8(t, CVMX_SLI_MAC_NUMBER);
	t->t_pcie_port = smn.s.num;

	scs.u64 = target_bar0_read8(t, CVMX_SLI_CTL_STATUS);
	t->t_chip_id = tpi->tpi_chip_id_base | scs.s.chip_rev;

	cf.u64 = target_read_csr(t, CVMX_CIU_FUSE);
	t->t_core_mask = cf.u64;

	/*
	 * Configure TWSI clock per Cavium code.
	 */
	for (i = 0; i < 2; i++) {
		mtst.u64 = 0;
		mtst.s.v = 1;
		mtst.s.op = 0x6;
		mtst.s.eop_ia = 0x3;
		mtst.s.d = 0xf << 3;
		target_write_csr(t, CVMX_MIO_TWSX_SW_TWSI(i), mtst.u64);
	}

	cvmx_select_target(t);
	if (!eeprom_board_desc_read(&ebd))
		t->t_board_type = CVMX_BOARD_TYPE_NULL;
	else
		t->t_board_type = ebd.ebd_board_type;
	cvmx_select_target(NULL);

	return (t);
}

static void
target_boot_one(struct target *t)
{
	(void)t;
	errx(1, "not yet implemented.");
}

static void
target_reset_one(struct target *t)
{
	target_write_csr(t, CVMX_CIU_SOFT_BIST, 1);

	target_read_csr(t, CVMX_CIU_SOFT_RST);
	target_write_csr(t, CVMX_CIU_SOFT_RST, 1);
}

static void
target_show_one(struct target *t)
{
	cvmx_lmcx_reset_ctl_t lrc;
	uint64_t cores;
	unsigned i;

	printf("target%u <%s> at PCI %08x:%02x:%02x:%02x\n", t->t_unit, t->t_model, t->t_pci_domain, t->t_pci_bus, t->t_pci_slot, t->t_pci_function);

	for (i = 0; i < TARGET_BARS; i++) {
		if (!t->t_pci_bar[i].tb_enabled) {
			printf("target%u: BAR%u disabled\n", t->t_unit, i);
			continue;
		}
		printf("target%u: BAR%u %#jx-%#jx (%ju bytes) mapped %p\n", t->t_unit, i,
		       (uintmax_t)t->t_pci_bar[i].tb_base,
		       (uintmax_t)(t->t_pci_bar[i].tb_base + t->t_pci_bar[i].tb_length),
		       (uintmax_t)t->t_pci_bar[i].tb_length,
		       (void *)t->t_pci_bar[i].tb_virtual);
	}

	printf("target%u: PCIe port %u core model 0x%08x (%s)\n", t->t_unit, t->t_pcie_port, t->t_chip_id, octeon_model_get_string(t->t_chip_id));

	if (t->t_core_mask == 0)
		printf("target%u: no cores configured\n", t->t_unit);
	else {
		printf("target%u: core mask 0x%016jx\n", t->t_unit, (uintmax_t)t->t_core_mask);

		cores = target_read_csr(t, CVMX_CIU_PP_RST);
		if (cores == 0)
			printf("target%u: no cores in reset\n", t->t_unit);
		else if (cores == t->t_core_mask)
			printf("target%u: all cores in reset\n", t->t_unit);
		else
			printf("target%u: cores in reset 0x%016jx\n", t->t_unit, (uintmax_t)cores);

		cores = target_read_csr(t, CVMX_CIU_PP_DBG);
		if (cores == 0)
			printf("target%u: no cores in debug\n", t->t_unit);
		else if (cores == t->t_core_mask)
			printf("target%u: all cores in debug\n", t->t_unit);
		else
			printf("target%u: cores in debug 0x%016jx\n", t->t_unit, (uintmax_t)cores);


	}

	lrc.u64 = target_read_csr(t, CVMX_LMCX_RESET_CTL(0));
	printf("target%u: memory %savailable\n", t->t_unit, lrc.s.ddr3rst ? "" : "not ");

	if (t->t_board_type == 0)
		printf("target%u: unknown board type\n", t->t_unit);
	else
		printf("target%u: board type 0x%04hx (%s)\n", t->t_unit, t->t_board_type, cvmx_board_type_to_string(t->t_board_type));
}
