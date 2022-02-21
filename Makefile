
#export RTE_SDK ?= /home/wanry/work/cavium/SDK-10.3.3.0-PR8/base-sdk/cn96xx-release-output/build/dpdk
export RTE_SDK ?= /home/cavium/SDK-10.3.3.0-PR8/cn96xx-release-output/build/dpdk
#export RTE_SDK ?= /home/cavium/SDK-10.3.5.0-PR2/cn96xx-release-output/build/dpdk
export RTE_TARGET ?= $(notdir $(abspath $(dir $(firstword $(wildcard $(RTE_SDK)/*/.config)))))

export CROSS = /home/cavium/SDK-10.3.3.0-PR8/cn96xx-release-output/host/bin/aarch64-marvell-linux-gnu-
#export CROSS = /home/wanry/work/cavium/SDK-10.3.3.0-PR8/base-sdk/cn96xx-release-output/host/bin/aarch64-marvell-linux-gnu-
#export CROSS = /home/cavium/SDK-10.3.5.0-PR2/cn96xx-release-output/host/bin/aarch64-marvell-linux-gnu-

include $(RTE_SDK)/mk/rte.vars.mk



APP = bzpp


SRCS-y += bzpp_init.c bzpp_cli.c libcli.c


CFLAGS += -g
#CFLAGS += $(WERROR_FLAGS) -Wno-strict-prototypes -Wno-missing-prototypes
LDFLAGS += -lcrypt

include $(RTE_SDK)/mk/rte.extapp.mk





