RTE_SDK = $(DPDK_PATH)
RTE_TARGET = $(DPDK_TARGET)

include $(RTE_SDK)/mk/rte.vars.mk

SRCS-y += main.c
SRCS-y += dpdk_env.c

CFLAGS += -I .

include $(RTE_SDK)/mk/rte.extapp.mk
