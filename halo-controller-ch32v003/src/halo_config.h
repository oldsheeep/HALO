// variant ごとの固定情報を取り込み、整合性をビルド時に検査する。
// variant は `make VARIANT=<name>`(= -DHALO_VARIANT_HEADER) で選ぶ。variants/<name>.h を参照。
//
// variant ヘッダが定義するもの:
//   HALO_VARIANT_NAME       文字列
//   HALO_LED_COUNT          SK6812 の個数
//   HALO_LED_ANGLES_DEG      数珠つなぎの順の偏角[deg]のカンマ区切りリスト
//   halo_fixed_presets[3]   固定プリセット(const halo_preset_t)
#ifndef HALO_CONFIG_H
#define HALO_CONFIG_H

#include "halo_preset.h"

#ifndef HALO_VARIANT_HEADER
#error "HALO_VARIANT_HEADER が未定義。make VARIANT=<name> でビルドすること"
#endif
#include HALO_VARIANT_HEADER

#ifndef HALO_VARIANT_NAME
#error "variant に HALO_VARIANT_NAME が無い"
#endif
#ifndef HALO_LED_COUNT
#error "variant に HALO_LED_COUNT が無い"
#endif
#ifndef HALO_LED_ANGLES_DEG
#error "variant に HALO_LED_ANGLES_DEG が無い"
#endif

#define HALO_FIXED_PRESET_COUNT 3
#define HALO_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

// 偏角の並び(LED の接続順)。個数は HALO_LED_COUNT と一致させる。
static const uint16_t halo_led_angles_deg[] = {HALO_LED_ANGLES_DEG};

// 個数と偏角配列の長さの不一致はここでビルドエラーになる。
_Static_assert(HALO_LED_COUNT > 0, "HALO_LED_COUNT must be > 0");
_Static_assert(HALO_ARRAY_LEN(halo_led_angles_deg) == HALO_LED_COUNT,
               "HALO_LED_ANGLES_DEG length != HALO_LED_COUNT");
_Static_assert(HALO_ARRAY_LEN(halo_fixed_presets) == HALO_FIXED_PRESET_COUNT,
               "exactly 3 fixed presets required");

#endif
