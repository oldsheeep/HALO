// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Copyright (c) 2026 sheeplab
// halo-bridge のスレーブは、I2C の割り込みとメインループの両方から状態を書き換える。
// 競合する短い区間を、割り込みの禁止・許可で囲む(Makefile の -include で、全ソースに入れる)。
#ifndef HALO_CRITICAL_H
#define HALO_CRITICAL_H

#include "ch32fun.h"

#define HALO_BRIDGE_CRITICAL_ENTER() __disable_irq()
#define HALO_BRIDGE_CRITICAL_EXIT() __enable_irq()

#endif
