SRCS	= exectrace.c
KMOD	= exectrace
KO	= ${KMOD}.ko
KLDMOD	= t

.include <bsd.kmod.mk>
