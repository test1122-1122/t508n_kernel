#!/bin/bash

make BSP_KERNEL_PATH=~/Projects/open/jingpad-a1/tmp/downloads/kernel-jingpad-a1 BSP_BOARD_UNISOC_WCN_SOCKET=pcie BSP_MODULES_OUT=$(pwd)/out modules
