# AviUtl ガウスぼかし拡張編集フィルタプラグイン

拡張編集標準の三角分布のぼかしではなく，[正規分布](https://ja.wikipedia.org/wiki/%E6%AD%A3%E8%A6%8F%E5%88%86%E5%B8%83)によるぼかしのフィルタ効果を追加する拡張編集フィルタプラグインです．

[ダウンロードはこちら．](https://github.com/sigma-axis/aviutl_GaussianBlur_S/releases)

![使用例](https://github.com/user-attachments/assets/1a27c759-fd01-4e2d-bc34-e8db18f8cd26)

- 標準のぼかしとは見た目ではほとんど違いは見受けられませんが，回転対称なものに対してだったり，ぼかしの後に二値化をかけたりした場合で，回転対称性が保たれやすくなります．

  ![同心円の模様に適用した場合の比較](https://github.com/user-attachments/assets/d003971c-c74a-4454-8050-1da7878c3fd3)

  ![ぼかしてから二値化した場合の比較](https://github.com/user-attachments/assets/a4014c5d-71c9-4482-a57b-a1f4b4aa0eac)

- 小数点以下の小さいぼかし幅も扱えます．

##  動作要件

- AviUtl 1.10 + 拡張編集 0.92

  http://spring-fragrance.mints.ne.jp/aviutl
  - 拡張編集 0.93rc1 等の他バージョンでは動作しません．

- Visual C++ 再頒布可能パッケージ（\[2015/2017/2019/2022\] の x86 対応版が必要）

  https://learn.microsoft.com/ja-jp/cpp/windows/latest-supported-vc-redist

- patch.aul の `r43 謎さうなフォーク版58` (`r43_ss_58`) 以降

  https://github.com/nazonoSAUNA/patch.aul/releases/latest


##  導入方法

`aviutl.exe` と同階層にある `plugins` フォルダ内に `GaussianBlur_S.eef` ファイルをコピーしてください．

##  使い方

正しく導入できているとフィルタ効果とフィルタオブジェクトに「ガウスぼかしσ」が追加されています．対象のオブジェクト，または画面全体に正規分布によるぼかしを適用します．

![GUIの表示](https://github.com/user-attachments/assets/9fc54285-66be-4487-90c3-f3b09d2a7448)

### 各種パラメタ

- 標準偏差

  ぼかしの強さを決定します．大きくするとぼかしが強くなり，`0` だとぼかし効果が適用されません．1 軸当たりの分布関数の[標準偏差](https://ja.wikipedia.org/wiki/%E6%A8%99%E6%BA%96%E5%81%8F%E5%B7%AE)の値で指定します．

  最小値は `0.00`, 最大値は `500.00`, 初期値は `2.00`.

  - 拡張編集標準の「ぼかし」フィルタ効果は三角分布であるため，計算上標準偏差は `範囲` の $1/\sqrt{6} \approx 0.4082$ 倍に相当します．

- 範囲倍率

  ぼかしによって拡大する画像サイズを，`標準偏差` からの倍率で % 単位で指定します．

  最小値は `0.00`, 最大値は `400.00`, 初期値は `300.00`.

  - ぼかし処理で 1 つのピクセルが及ぼす影響範囲で，計算精度や処理速度に直結します．大きくすると計算精度も高くなりますが処理は遅くなり，画像サイズも大きくなります．

  - 初期値の `300.00`% で最大誤差 $1/370.4$ 程度になりますが，これは通常のディスプレイにおける RGB 精度 $1/255$ と比べて十分な精度が出ています．

    参考までに，計算精度と `範囲倍率` との関係です:

    |`範囲倍率`|最大誤差|備考|
    |:---:|---:|:---|
    |`250.00`%|$\approx 1/80.5$|トラックバーのドラッグで指定可能な最小値．|
    |`288.44`%|$\approx 1/255.0$|通常ディスプレイの RGB 精度と同等．|
    |`300.00`%|$\approx 1/370.4$|初期値．|
    |`350.00`%|$\approx 1/2149.3$|トラックバーのドラッグで指定可能な最大値．|
    |`366.83`%|$\approx 1/4096.0$|AviUtl 内部でのピクセル精度と同等．|
    |`400.00`%|$\approx 1/15787.2$|`範囲倍率` の最大値．|

- 縦横比

  X 軸，Y 軸で異なるぼかし幅を設定できます．

  正で X 軸方向のぼかし幅が変動し，`100.00` だと X 軸方向にはぼかしなし．負だと Y 軸方向のぼかし幅が変動し，`-100.00` で Y 軸方向のぼかしなし，となります．

  ![縦横比を変えた場合の例](https://github.com/user-attachments/assets/291b1b64-4aac-41ac-a03c-a462e7be611c)

  最小値は `-100.00`, 最大値は `100.00`, 初期値は `0.00`.

  - 拡張編集標準のぼかしの `縦横比` とは正負が逆な点に注意．

- 輝度γ補正

  輝度成分にガンマ補正をかけて明るい (または暗い) ピクセルが強調されたぼかしになります．拡張編集標準のぼかしフィルタ効果の，`光の強さ` に近い効果が得られます．

  輝度に適用するガンマ補正を，ガンマ値の $100$ 倍で指定します．`100.00` だとガンマ補正をかけません．

  最小値は `12.50`, 最大値は `800.00`, 初期値は `100.00`.

- サイズ固定

  最終的な画像のサイズを膨らませずにぼかし効果を適用します．透明度のない画像をぼかす場合，端の部分が欠けることなくぼかし効果を適用できます．

  初期値は OFF.

  - フィルタ効果にのみ表示されます．フィルタオブジェクトには表示されず，常にサイズ固定の扱いです．


##  スクリプトからの利用

スクリプト制御やアニメーション効果などで次のように「余白除去σ」の効果を適用できます:

```lua
obj.effect("ガウスぼかしσ", "標準偏差", 16, "範囲倍率", 350, "サイズ固定", 1)
```

##  改版履歴

- **v1.00** (2025-0?-??)

  - 初版．

##  ライセンス

このプログラムの利用・改変・再頒布等に関しては MIT ライセンスに従うものとします．

---

The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

https://mit-license.org


# Credits

##  aviutl_exedit_sdk

https://github.com/ePi5131/aviutl_exedit_sdk （利用したブランチは[こちら](https://github.com/sigma-axis/aviutl_exedit_sdk/tree/self-use)です．）

---

1条項BSD

Copyright (c) 2022
ePi All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

THIS SOFTWARE IS PROVIDED BY ePi “AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL ePi BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


# 連絡・バグ報告

- GitHub: https://github.com/sigma-axis
- Twitter: https://x.com/sigma_axis
- nicovideo: https://www.nicovideo.jp/user/51492481
- Misskey.io: https://misskey.io/@sigma_axis
- Bluesky: https://bsky.app/profile/sigma-axis.bsky.social
