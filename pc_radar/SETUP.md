# Radar Scanner PC App セットアップ手順

WSL2 (Ubuntu) 環境で Arduino と接続してレーダー表示アプリを動かすまでの手順。

---

## 前提環境

- Windows 11
- WSL2 (Ubuntu 22.04)
- Arduino UNO R4 (または互換機) が USB 接続済み
- Arduino IDE でArduino用のスケッチを事前に入れること
 - スケッチを入れたらIDEは閉じておくこと（COM ポートを占有するため）
- Python 3.10 / uv インストール済み (WSL 側)

---

## 1. WSL 側: システムライブラリのインストール

PyQt5 の xcb プラグインが依存するライブラリを一括インストールする。

```bash
sudo apt update
sudo apt install -y \
  libxcb-cursor0 \
  libxcb-icccm4 \
  libxcb-keysyms1 \
  libxcb-randr0 \
  libxcb-render-util0 \
  libxcb-shape0 \
  libxcb-xinerama0 \
  libxcb-xkb1 \
  libxkbcommon-x11-0 \
  libgl1-mesa-glx
```

---

## 2. WSL 側: Python 仮想環境のセットアップ

```bash
cd ~/master/code-test/pc_radar

# uv で仮想環境を作成 (.venv/)
uv venv

# 仮想環境を有効化
source .venv/bin/activate

# 依存パッケージをインストール
uv pip install -r requirements.txt
```

以降の操作はすべて仮想環境が有効な状態 `(pc_radar)` で行う。

---

## 3. Windows 側: usbipd-win のインストール

WSL2 は Windows の COM ポートを直接見えないため、USB デバイスを WSL に転送するツールが必要。

**PowerShell (通常権限) で実行:**

```powershell
winget install usbipd
```

インストール後、PowerShell を**再起動**する。

---

## 4. Windows 側: Arduino を WSL2 にアタッチ

### 4-1. 接続中の USB デバイスを確認

```powershell
usbipd list
```

出力例:
```
BUSID  VID:PID    DEVICE                                      STATE
1-4    2341:1002  USB シリアル デバイス (COM4)                Not shared
```

Arduino の行の BUSID (例: `1-4`) を控える。

### 4-2. デバイスを共有登録 (初回のみ・管理者権限が必要)

**PowerShell を「管理者として実行」で開き直してから:**

```powershell
usbipd bind --busid 1-4
```

### 4-3. WSL2 にアタッチ

```powershell
usbipd attach --wsl --busid 1-4
```

> **エラー `Device busy (exported)` が出た場合**
> Arduino IDE やシリアルモニタなど、COM ポートを使っているアプリを閉じてから再実行する。
> それでも失敗する場合は `--force` オプションを追加する。
>
> ```powershell
> usbipd attach --wsl --busid 1-4 --force
> ```

---

## 5. WSL 側: デバイスの確認

```bash
ls /dev/ttyACM*
# → /dev/ttyACM0 などが表示されれば OK
```

---

## 6. アプリを起動

```bash
cd ~/master/code-test/pc_radar
source .venv/bin/activate   # 仮想環境が無効になっていた場合
python main.py
```

GUI が起動したら:
1. **Port** ドロップダウンから `/dev/ttyACM0` を選択
2. **Connect** ボタンをクリック
3. Arduino からのデータを受信するとレーダー表示が開始される

---

## トラブルシューティング

### `Could not load the Qt platform plugin "xcb"` エラー

手順 1 のライブラリが不足している。以下で原因ライブラリを特定できる:

```bash
QT_DEBUG_PLUGINS=1 python main.py 2>&1 | grep -i "cannot\|error"
```

表示された `libxxx.so` に対応するパッケージを `sudo apt install` で追加する。

### Port ドロップダウンが空

- usbipd でアタッチできているか確認 (`usbipd list` で STATE が `Attached` になっているか)
- `ls /dev/ttyACM* /dev/ttyUSB*` でデバイスファイルが存在するか確認
- Arduino IDE を閉じてから再アタッチする

### PC 再起動後に毎回アタッチが必要

`usbipd attach` はセッションごとに必要。自動化したい場合は Windows のタスクスケジューラで起動時に実行するよう設定する。
