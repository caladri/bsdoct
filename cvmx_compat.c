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

#include <stdbool.h>

#include <cvmx.h>

#include "cvmx_compat.h"
#include "target.h"

static struct target *current_target;

void
cvmx_select_target(struct target *t)
{
	if (t != NULL) {
		assert(current_target == NULL);
		current_target = t;
	} else {
		assert(current_target != NULL);
		current_target = NULL;
	}
}

void
cvmx_warn(const char *fmt, ...)
{
	va_list ap;

	assert(current_target != NULL);

	fprintf(stderr, "target%u: WARNING ", current_target->t_unit);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fflush(stderr);
}

uint32_t
cvmx_get_proc_id(void)
{
	assert(current_target != NULL);
	return (current_target->t_chip_id);
}

uint64_t
cvmx_read_csr(uint64_t addr)
{
	assert(current_target != NULL);
	return (target_read_csr(current_target, addr));
}

void
cvmx_write_csr(uint64_t addr, uint64_t data)
{
	assert(current_target != NULL);
	target_write_csr(current_target, addr, data);
}

uint32_t
cvmx_pop(uint32_t w)
{
	return (__builtin_popcount(w));
}

uint8_t
cvmx_fuse_read_byte(int addr)
{
	cvmx_mio_fus_rcmd_t mfr;

	assert(current_target != NULL);

	mfr.u64 = 0;
	mfr.s.pend = 1;
	mfr.s.addr = addr;
	target_write_csr(current_target, CVMX_MIO_FUS_RCMD, mfr.u64);

	for (;;) {
		mfr.u64 = target_read_csr(current_target, CVMX_MIO_FUS_RCMD);
		if (!mfr.s.pend)
			break;
	}

	return (mfr.s.dat);
}

int
cvmx_fuse_read(int fuse)
{
	uint8_t b;

	b = cvmx_fuse_read_byte(fuse / 8);
	fuse %= 8;
	return ((b & (1 << fuse)) != 0);
}

void
cvmx_wait(uint64_t cycles)
{
	cvmx_wait_usec(cycles / 1000);
}

void
cvmx_wait_usec(uint64_t usec)
{
	usleep(usec);
}
