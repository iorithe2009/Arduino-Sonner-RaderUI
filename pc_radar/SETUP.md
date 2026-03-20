# Radar Scanner PC App セットアップ手順

このドキュメントは、`pc_radar/main.py` を動かすための手順です。  
**推奨は Windows 11 ネイティブ実行**で、WSL は代替手段として後半に記載します。

---

## 1. Windows 11 ネイティブ実行（推奨）

### 前提環境

- Windows 11
- Python 3.10 以上（`py` コマンドが使える状態）
- Arduino UNO R4（または互換機）が USB 接続済み
- Arduino IDE で `arduino/radar_scanner/radar_scanner.ino` を書き込み済み
- Arduino IDE / シリアルモニタを閉じている（COM ポート占有回避）

### 手順

PowerShell で以下を実行:

```powershell
cd <このリポジトリ>\pc_radar
py -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
python main.py
```

> `Activate.ps1` 実行時に実行ポリシーでブロックされた場合は、PowerShell で一時的に  
> `Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass` を実行してから再試行してください。

### アプリ操作

1. **Port** ドロップダウンから `COMx` を選択
2. **Connect** をクリック
3. 角度・距離が更新されれば接続成功

> `Auto Connect` を使うと、Arduino らしい COM ポートを自動選択して接続できます。

### COM ポート確認（必要時）

- デバイス マネージャー → 「ポート (COM と LPT)」で Arduino の `COMx` を確認
- ポートが見つからない場合は USB ケーブル・ドライバ・Arduino IDE の占有を確認

---

## 2. WSL2 実行（代替）

WSL2 (Ubuntu) 環境で Arduino と接続してレーダー表示アプリを動かす手順。

### 前提環境

- Windows 11
- WSL2 (Ubuntu 22.04)
- Arduino UNO R4 (または互換機) が USB 接続済み
- Arduino IDE でArduino用のスケッチを事前に入れること
 - スケッチを入れたらIDEは閉じておくこと（COM ポートを占有するため）
- Python 3.10 / uv インストール済み (WSL 側)

---

## 2-1. WSL 側: システムライブラリのインストール

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

## 2-2. WSL 側: Python 仮想環境のセットアップ

```bash
cd ~/master/code-test/pc_radar

# uv で仮想環境を作成 (.venv/)
uv venv

# 仮想環境を有効化
source .venv/bin/activate

# 依存パッケージをインストール
uv pip install -r requirements.txt

# インストール済みの場合は以下のコマンドで起動可能
source .venv/bin/activate
uv run python main.py


```

以降の操作はすべて仮想環境が有効な状態 `(pc_radar)` で行う。

---

## 2-3. Windows 側: usbipd-win のインストール

WSL2 は Windows の COM ポートを直接見えないため、USB デバイスを WSL に転送するツールが必要。

**PowerShell (通常権限) で実行:**

```powershell
winget install usbipd
```

インストール後、PowerShell を**再起動**する。

---

## 2-4. Windows 側: Arduino を WSL2 にアタッチ

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

## 2-5. WSL 側: デバイスの確認

```bash
ls /dev/ttyACM*
# → /dev/ttyACM0 などが表示されれば OK
```

---

## 2-6. アプリを起動

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
