# Radar Scanner

Arduino（超音波 + サーボ）と PC アプリで動作するレーダー風プロジェクトです。  
詳細は各フォルダ内の Markdown を参照してください。

## 最短起動（Windows 11 ネイティブ）

WSL を使わずに `pc_radar/main.py` を動かす手順です。

```powershell
cd pc_radar
py -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
python main.py
```

起動後はアプリ上で COM ポートを選択し、`Connect` を押してください。  
より詳しい手順は `pc_radar/SETUP.md` を参照してください。
