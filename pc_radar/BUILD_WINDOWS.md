# Windows ビルド手順

## 事前準備

- この手順は `pc_radar` ディレクトリで実行する。
- WSL ではなく Windows 側のターミナル（PowerShell / cmd）で実行する。

## Windows 向けビルド手順（pip + PyInstaller）

1. プロジェクト依存関係をインストールする。

   ```bash
   pip install -r requirements.txt
   ```

2. PyInstaller をインストールする。

   ```bash
   pip install pyinstaller
   ```

3. まずは **onedir** 形式でビルドする。

   ```bash
   pyinstaller --noconfirm --windowed --hidden-import serial --name RadarScanner main.py
   ```

4. 生成物を確認する。

   - 生成フォルダ: `dist/RadarScanner/`
   - 実行ファイル: `dist/RadarScanner/RadarScanner.exe`

## Windows 向けビルド手順（uv 版）

1. 仮想環境を作成する。

   ```bash
   uv venv
   ```

2. 仮想環境を有効化する（PowerShell の例）。

   ```bash
   .venv\Scripts\Activate.ps1
   ```

3. プロジェクト依存関係をインストールする。

   ```bash
   uv pip install -r requirements.txt
   ```

4. PyInstaller をインストールする。

   ```bash
   uv pip install pyinstaller
   ```

5. まずは **onedir** 形式でビルドする。

   ```bash
   uv run pyinstaller --noconfirm --windowed --hidden-import serial --name RadarScanner main.py
   ```

6. 生成物を確認する。

   - 生成フォルダ: `dist/RadarScanner/`
   - 実行ファイル: `dist/RadarScanner/RadarScanner.exe`

## 配布運用

- 配布時は `dist/RadarScanner` フォルダを**フォルダごと**配布する。
- そのまま配布してもよいし、ZIP 化して配布してもよい。

## onefile 検証手順（任意）

onedir 版の動作確認が完了したら、必要に応じて onefile 版も検証する。

```bash
pyinstaller --noconfirm --windowed --onefile --hidden-import serial --name RadarScanner main.py
```

uv 版で実施する場合:

```bash
uv run pyinstaller --noconfirm --windowed --onefile --hidden-import serial --name RadarScanner main.py
```

- 生成される実行ファイルの場所: `dist/RadarScanner.exe`
- 起動確認し、必要なリソース同梱や起動時間を評価する。
