RTE_SDK = $(DPDK_PATH)
RTE_TARGET = $(DPDK_TARGET)

include $(RTE_SDK)/mk/rte.vars.mk

SRCS-y += main.c
SRCS-y += dpdk_env.c
SRCS-y += plugins.c
SRCS-y += init.c
SRCS-y += node.c
SRCS-y += cli.c
SRCS-y += mem_pool.c

CFLAGS += -I .

include $(RTE_SDK)/mk/rte.extapp.mk
