PROG=	bsdoct
MAN=
WARNS=	6

SRCS+=	bsdoct.c

SRCS+=	cvmx_compat.c
SRCS+=	eeprom.c
SRCS+=	target.c

CFLAGS+=-include global.h

SYSDIR=	../freebsd-head/sys
SDKDIR=	${SYSDIR}/contrib/octeon-sdk

CFLAGS+=-I${SDKDIR}
CFLAGS+=-DUSE_RUNTIME_MODEL_CHECKS

.PATH: ${SDKDIR}
SRCS+=	cvmx-clock.c
SRCS+=	cvmx-twsi.c
SRCS+=	octeon-feature.c
SRCS+=	octeon-model.c

.include <bsd.prog.mk>

CFLAGS+=-Wno-parentheses-equality
