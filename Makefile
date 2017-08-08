# Default target, can be overriden by command line or environment
CUR_DIR=$(shell pwd)
DPDK_PATH=/usr/share/dpdk
export DPDK_PATH

CC = gcc
export CC

DPDK_TARGET = x86_64-default-linuxapp-$(CC)
export DPDK_TARGET

APP = zen
export APP

ZEND_DIR = $(CUR_DIR)/src/zend

EXTRA_CFLAGS += -std=gnu99 -Wall -O0 -g
EXTRA_CFLAGS += -I $(CUR_DIR)/include -I $(ZEND_DIR)
export EXTRA_CFLAGS

all: zend
install: zend_install
clean: zend_clean

zend:
	@make -s -C $(ZEND_DIR)

zend_clean:
	@make -s -C $(ZEND_DIR) clean
	@rm -rf $(ZEND_DIR)/build

zend_install:
	@make -s -C $(ZEND_DIR) install
