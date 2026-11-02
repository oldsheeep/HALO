# halo-pcb-ch32v003

HALO のコントローラ基板(CH32V003J4M6)の回路図。KiCad 10 で作成。

- 回路図: [halo_pcb_ch32v003.kicad_sch](halo_pcb_ch32v003.kicad_sch)
- プロジェクト: [halo_pcb_ch32v003.kicad_pro](halo_pcb_ch32v003.kicad_pro)
- 通信の仕様: [docs/halo-link-spec.md](../docs/halo-link-spec.md)(3 節)
- ファームウェア: [halo-controller-ch32v003](../halo-controller-ch32v003/)(右の基板)、[halo-controller](../halo-controller/)(左の基板)

**状態: 回路図は ERC をエラー 0・警告 0 で通した**(`kicad-cli sch erc`、KiCad 10.0.8)。
フットプリントは一部が未割り当てで、基板(PCB)は未着手。実機での動作は未確認。

## 図の見方

1 枚のシートに、**同じ設計の基板 2 枚**(左と右)が、3.5mm ケーブルでつながった形で描かれている。中央の縦線が基板の境界。

| | 左の基板 | 右の基板 |
| --- | --- | --- |
| 役割 | B 側(マスター)。USB-C で給電する | A 側(スレーブ)。LED をつなぐ |
| MCU | U1 | U2 |
| レギュレータ | U3 | U4 |
| ジャック | J2(3.5mm ジャック) | J1(3.5mm プラグ) |
| 書き込みヘッダ | J4 | J5 |
| 3.3V のネット | `3V3_L` | `3V3_R` |
| PD6 の保護 | R5・D1 | R6・D2 |
| 5V の入力 | J3(USB-C) | ジャックの Ring から(左の基板の 5V) |

ネット `+5V` と `GND` は 2 枚で共通、`LINK`(通信線)は 2 枚の PD6 をつなぐ 1 本の線。

## 構成

```text
 左の基板(USB 給電)                                右の基板(LED)
 USB-C ─ +5V ─┬─ C1 1µF ─ U3(XC6206) ─ 3V3_L     +5V(Ring から)─ C5 1µF ─ U4 ─ 3V3_R
              ├─ C4 100µF                            ├─ J6(LED: +5V / DIN / GND)+ C8 100µF
              └─ Ring ──────────── 3.5mm ──────────── Ring
 U1.PD6 ─ R5 1kΩ ─ LINK(R1 10kΩ → 3V3_L)───────────── LINK(R2 10kΩ → 3V3_R)─ R6 1kΩ ─ U2.PD6
          D1 BAT54WS(PD6 → 3V3_L のクランプ)                  D2 BAT54WS(PD6 → 3V3_R のクランプ)
 U1.PD1/SWIO ─ J4                                     U2.PC4 ─ DIN ─ J6        U2.PD1/SWIO ─ J5
```

- **電源**: USB-C の VBUS(CC1・CC2 に 5.1kΩ を直接)が +5V。XC6206P332MR で 3.3V を作る(入力・出力に 1µF、MCU の VDD に 0.1µF)。
- **通信**: PD6 の 1 ピン・オープンドレイン。R1・R2(10kΩ)が H を作る。R5・R6(1kΩ)と D1・D2(BAT54WS、PD6 → 3.3V のクランプ)が、抜き差しの一瞬に Ring の 5V が PD6 に入るのを守る。
- **ジャック**: Tip = 通信(`LINK`)、Ring = +5V、Sleeve = GND。
- **LED**: SK6812mini-e の DIN は PC4(pin 7)から**直結**(3.3V のまま。実機で動作を確認済みとのこと)。LED の電源は +5V。
- **書き込み**: J4・J5 の 2 ピン(SWIO と GND)。WCH-LinkE を接続する。

## 部品表

| 参照 | 値 | 型 | フットプリント |
| --- | --- | --- | --- |
| U1, U2 | CH32V003J4M6 | `MCU_WCH_RiscV:CH32V003JxMx` | `Package_SO:JEITA_SOIC-8_3.9x4.9mm_P1.27mm` |
| U3, U4 | XC6206P332MR | `Regulator_Linear:XC6206PxxxMR` | `Package_TO_SOT_SMD:SOT-23-3` |
| J3 | USB-C(電源のみ) | `Connector:USB_C_Receptacle_USB2.0_14P` | **未割り当て** |
| J2 | 3.5mm ジャック | `Connector_Audio:AudioJack3` | **未割り当て** |
| J1 | 3.5mm プラグ(ケーブル側) | `Connector_Audio:AudioPlug3` | 基板には載らない |
| J4, J5 | SWIO 書き込み(2 ピン) | `Connector_Generic:Conn_01x02` | `PinHeader_1x02_P2.54mm_Vertical` |
| J6 | LED(5V / DIN / GND) | `Connector_Generic:Conn_01x03` | `PinHeader_1x03_P2.54mm_Vertical` |
| R1, R2 | 10kΩ | `Device:R` | `R_0603_1608Metric` |
| R3, R4 | 5.1kΩ | `Device:R` | `R_0603_1608Metric` |
| R5, R6 | 1kΩ | `Device:R` | `R_0603_1608Metric` |
| C1, C2, C5, C6 | 1µF | `Device:C` | `C_0603_1608Metric` |
| C3, C7 | 0.1µF | `Device:C` | `C_0603_1608Metric` |
| C4, C8 | 100µF | `Device:C_Polarized` | `CP_Elec_6.3x5.4` |
| D1, D2 | BAT54WS | `Device:D_Schottky` | `Diode_SMD:D_SOD-323` |

## 注意点

仕様書の 3.3 に詳しい。要点:

1. **pin 8(SWIO)を GPIO や USART にしない**。PD1 / PD4 / PD5 が同じピン。PD5 を触ると書き込めなくなる恐れ。
2. **pin 1 は PD6 と PA1 の共用**。クリスタルを付けない。
3. **PD6 は 5V トレラントではない**。Ring(+5V)の抜き差しで一瞬ショートするので、R5・R6 と D1・D2 で守る。
4. **オープンドレインにはプルアップ(R1・R2)が必須**。
5. **2 枚の USB 電源を並列にしない**。現状の回路では、右の基板の 5V は左の基板から Ring 経由で受ける前提。右の基板に USB-C を付けて両方を給電すると、2 つの 5V が衝突する。
6. **LED の電流を Ring に流さない**。3.5mm ジャックの定格はおおむね 1A で、LED(15 個で最大約 0.9A)を流すと近い。LED を付ける右の基板は、自分で給電するのが安全。

## 未決・要確認

- **Ring の 5V の向き**: 受け側(右の基板)の Ring に、ショットキーダイオードを直列に入れるか、ソルダージャンパを付けるか。いまは直結。
- **J2・J3 のフットプリント**: 実部品(USB-C レセプタクル、3.5mm ジャック)を決めて、割り当てる。
- **半二重モード(`HDSEL`)の動作、通信の L レベルと立ち上がり**は実機で確認する(仕様書の U5)。
- **BAT54WS(SOD-323)の向き**: 1 = カソード、2 = アノード。データシートで確認。
- **ERC の再実行**: KiCad で開いて保存し直したら、`kicad-cli sch erc halo-pcb-ch32v003/halo_pcb_ch32v003.kicad_sch` で確認する。
