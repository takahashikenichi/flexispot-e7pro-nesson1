# Flexispot Web コントローラー (M5Unified / Arduino Nesso N1)

![IMG_1237](https://github.com/user-attachments/assets/2d6b620c-cdfd-4e2e-9b4c-b81906e4d92c)

> [!WARNING]
> 注意してください！電子機器の実験は危険を伴う場合があります。このガイドは自己責任でご利用ください。

## 概要

このプロジェクトは、Flexispot 製のスタンディングデスクをのコントローラーをエミューレーションした M5Unified 対応デバイス（Arduino nesso N1 など）から **物理ボタン** と **Web ブラウザ**の両方で操作できるようにするものです。

Flexispot 製のスタンディングデスクのほとんどのモデルは、LoctekMotion 社のコントローラーを利用しています。このプロジェクトでは、コントロールボックスに接続するRJ45ポートからシリアル通信を行い、スタンディングデスクを M5Unified 対応デバイスから操作することができます。

また、デスクの高さ情報（7セグ表示の値）をシリアル経由で取得し、

- M5Unified 対応本体のディスプレイ
- 同一ネットワーク内のブラウザ（Web UI）

の両方に表示することもできます。


本プロジェクトは **Arduino Nesso N1 上でのみ動作確認済み** です。

## 主な機能

- Flexispot 昇降デスク（E7 Pro）のシリアル制御による上昇・下降操作

- M5Unified 対応本体のボタン操作／表示
  - Button A: Wakeup（デスクのコントローラを起動）
  - Button B: Preset 4（着座位置など用途に応じて変更可能）
  - ディスプレイに現在の高さを表示
- Web ブラウザからの操作（HTTP）／表示
  - Wakeup （コントローラー起動／現在の高さの取得
  - Up / Down（押している間のみ連続昇降、約 108ms 間隔で送信）
  - Memory（cmd_mem）
  - Preset 1 / 2 / 3 / 4
  - 現在の高さを 1 秒ごとに自動更新
  - スリープ時は `Sleeping...` と表示

## ハードウェア構成

- Flexispot E7 Pro 昇降デスク
  - コントローラー型番: HS13M-1C0
- M5Unified 対応デバイス  
  - **Arduino Nesso N1（動作確認済み）**
- Flexispot E7 Pro と Arudino Nesso N1の接続

| Pin(RJ45) | ケーブルカラー(T-568B) | 説明 | Nesso N1 の接続ポート |
| --- | --- | --- | --- |
| 1	| White Orange | - | - |	
| 2	| Orange | - | - |
| 3	| White Green |	- | - |
| 4	| Blue | 1秒間 HIGH にするとコントローラが Wakeup（動作モード遷移）| D3 |
| 5	| White Blue |	RX (of remote) | D2 |
| 6	| Green |	TX (of remote)	| D1 |
| 7	| White Brown |	GND	|GND |
| 8	| Brown |	VDD (5V) |	VIN |

![wiring](https://github.com/user-attachments/assets/2cc1f20f-4027-4ce3-8875-643718602d34)

## ソフトウェア設定
Flexispot のシリアル設定は以下のようになります。
- ボーレート: 9600 bps
- データビット: 8
- ストップビット: 1
- パリティ: なし

設定は以下のようにしてください。
```
 Serial1.begin(9600, SERIAL_8N1, D2, D1);
```

## Web UI について

接続先環境のSSIDとパスワードを設定し、M5Unified 対応を起動させ Wi-Fi に接続されると、HTTP サーバがポート `80` で起動します。
Serial出力と M5デバイスのディスプレイ に M5Unified 対応のIPアドレスが表示されます。
このIPアドレスを利用して、ブラウザから `http://<デバイスのIP>/` にアクセスすると操作が可能になります。

- Wakeup ボタンを押すことで、Wake状態に移行して現在の高さが表示され操作が可能になります。
- Wake時は現在の高さが表示されます。
- Up/Down は押している間だけ動作します。
- Memory は、一度押すとプリセット設定状態に移行します。この状態でプリセットボタンを押すと、現在のテーブルの高さが記憶されます。
- Preset 1〜4 は、押すことで設定された高さに移動します。

## セットアップ

1. RJ45(普通のEtherケーブル)を加工し、E5デバイスに接続します
2. RJ45ケーブルをFlexispot デスクのコントローラに接続します （一部のFlexispot デスクのコントローラーにはRJ45の口が2つあり、既存のコントローラーと共存できます）
![IMG_1240](https://github.com/user-attachments/assets/9abdf41e-e906-438c-ade4-600a29a73171)
3. `flexispot_e7pro_nesson1.ino`を Arduino IDE などで編集できるようにします
4. コード内の `YOUR_SSID` / `YOUR_PASSWORD` を それぞれご自分の環境の WiFi の情報に書き換えます
5. Arduino IDE からビルドし、Arduino Nesso N1 に書き込む  
6. シリアルモニタもしくはデバイスの画面よりIPアドレス確認
   - シリアルモニターの場合は、`WiFi connected.`  `IP address: 192.168.x.y`というメッセージが表示されます
   - 画面表示
   ![IMG_1238](https://github.com/user-attachments/assets/34019079-53df-4635-a90c-db0bb567817c)
7. ブラウザで `http://192.168.x.y/` にアクセスすると、Web UIが表示されます。
<img width="631" height="434" alt="image" src="https://github.com/user-attachments/assets/09dcf8de-a8dd-4a82-9209-76c9e7a2f77c" />

## 注意事項

- 本プロジェクトは **非公式の個人開発コード** です。  
  Flexispot 等による公式サポートはありません。
- 配線ミスには特に注意ください。配線時には必ずテスターで適切な電圧が流れているかを確認してください。
- 昇降時は周囲に物体や人体がないことを確認し、自己責任で使用してください。

## ライセンス

本プロジェクトは **MIT License** で公開されています。
