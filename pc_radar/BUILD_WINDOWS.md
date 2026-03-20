# Windows ビルド手順

## Windows 向けビルド手順（PyInstaller）

1. PyInstaller をインストールする。

   ```bash
   pip install pyinstaller
   ```

2. まずは **onedir** 形式でビルドする。

   ```bash
   pyinstaller --noconfirm --windowed --name RadarScanner main.py
   ```

3. 生成物を確認する。

   - 生成フォルダ: `dist/RadarScanner/`
   - 実行ファイル: `dist/RadarScanner/RadarScanner.exe`

## 配布運用

- 配布時は `dist/RadarScanner` フォルダを**フォルダごと**配布する。
- そのまま配布してもよいし、ZIP 化して配布してもよい。

## onefile 検証手順（任意）

onedir 版の動作確認が完了したら、必要に応じて onefile 版も検証する。

```bash
pyinstaller --noconfirm --windowed --onefile --name RadarScanner main.py
```

- 生成される実行ファイルの場所: `dist/RadarScanner.exe`
- 起動確認し、必要なリソース同梱や起動時間を評価する。
