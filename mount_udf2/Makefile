# $FreeBSD: src/sbin/mount_udf/Makefile,v 1.6 2006/07/17 20:53:25 stefanf Exp $

PROG=	mount_udf2
SRCS=	mount_udf.c getmntopts.c
MAN=	mount_udf2.8
DPADD=	${LIBKICONV}
LDADD=	-lkiconv

#MOUNT=	${.CURDIR}/../mount
#CFLAGS+= -I${MOUNT} -I${.CURDIR}/../../sys
#.PATH:	${MOUNT}
WARNS?= 1

# Needs to be dynamically linked for optional dlopen() access to
# userland libiconv
NO_SHARED?=	NO

BINDIR=/sbin

CFLAGS+=-g
CXXFLAGS+=-g

.include <bsd.prog.mk>
