# Integrated into Portapack by Enrique Avalle (c) 2020
#
# This file is part of Mayhem firmware for portapack and HackRF.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.
#

enable_language(C CXX ASM)

cmake_minimum_required(VERSION 2.8)
set(CMAKE_TOOLCHAIN_FILE ../toolchain-arm-cortex-m.cmake)

project(sdmmc)

set(SRC_M4
    sdmmc_18xx_43xx.h
    usbd_dfuuser.h
    app_usbd_cfg.h
    usbd.h
    usbd_hid.h
    error.h
    usbd_adc.h
    usbd_hiduser.h
    lpc_types.h
    usbd_adcuser.h
    usbd_hw.h
    msc_desc.c
    usbd_cdc.h
    usbd_msc.h
    msc_disk.h
    usbd_cdcuser.h
    usbd_mscuser.h
    msc_main.c
    usbd_core.h
    usbd_rom_api.h
    msc_sdmmc.c
    usbd_desc.h
    sdmmc_18xx_43xx.c
    usbd_dfu.h
)
