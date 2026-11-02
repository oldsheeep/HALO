# Third-Party Notices

このリポジトリのビルド成果物(`halo-*` の release)は、次の第三者ソフトウェアを含む。
自作部分のライセンスは [LICENSE](LICENSE)(PolyForm Noncommercial 1.0.0)で、下記には適用されない。

## ch32fun

- 入手元: <https://github.com/cnlohr/ch32fun>(コミット `b28d285460ce052a179dba8f4a22c6f73ff8e328`)
- 用途: CH32V003 のスタートアップ・ペリフェラル定義・ランタイム。`halo-controller-ch32v003` のビルドで取得してリンクする
- ライセンス: MIT

```text
MIT License

Copyright (c) 2023-2024 CNLohr <lohr85@gmail.com>, et. al.
Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
Copyright (c) 2023-2024 E. Brombaugh
Copyright (c) 2023-2024 A. Mandera
Copyright (c) 2005-2020 Rich Felker, et al.
Copyright (c) 2013,2014 Michal Ludvig <michal@logix.cz>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## 追加するとき

取り込んだ第三者コードを成果物に含める場合は、ここに入手元・用途・ライセンス全文を追記する。
