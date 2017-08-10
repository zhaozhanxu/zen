SRC_DIR = $(shell ls -l | grep ^d | awk '{print $$9}')

RTE_SDK = $(DPDK_PATH)
export RTE_SDK
RTE_TARGET = $(DPDK_TARGET)
export RTE_TARGET

export EXTRA_FLAGS

all:
	@for dir in $(SRC_DIR); do make -C $$dir || exit 1;	done

clean:
	@for dir in $(SRC_DIR); do make -C $$dir clean || exit 1; rm -rf $$dir/build || exit 1; done
