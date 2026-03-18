"""
Radar Scanner PC App - Phase 2
Arduino から '$angle,distance,millis' 形式の CSV を受信し、
レーダー風リアルタイム表示を行う。

使い方:
    pip install -r requirements.txt
    python main.py
"""

import sys
import math
import time
from collections import deque

import serial
import serial.tools.list_ports
import pyqtgraph as pg
from PyQt5.QtCore import Qt, QThread, pyqtSignal as Signal, QTimer
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget,
    QVBoxLayout, QHBoxLayout,
    QPushButton, QComboBox, QLabel,
)

# ── 定数 ────────────────────────────────────────────────────────────
BAUD_RATE    = 9600
MAX_DIST     = 100.0        # Arduino の DIST_MAX と合わせる (cm)
DIST_RINGS   = [25, 50, 75, 100]   # 距離リング (cm)
FADE_SEC     = 5.0          # 点がフェードアウトするまでの秒数
MAX_POINTS   = 500          # リングバッファ上限
FPS          = 30


# ── シリアル受信スレッド ─────────────────────────────────────────────
class SerialReader(QThread):
    """バックグラウンドでシリアルポートを読み続け、パース済みデータを emit する。"""

    data_received      = Signal(int, float)  # (angle_deg, distance_cm)  distance=-1 は OUT
    connection_changed = Signal(bool)         # True=接続, False=切断

    def __init__(self, port: str, baud: int):
        super().__init__()
        self._port    = port
        self._baud    = baud
        self._running = False
        self._ser     = None

    def run(self):
        self._running = True
        while self._running:
            try:
                self._ser = serial.Serial(self._port, self._baud, timeout=1)
                self.connection_changed.emit(True)

                while self._running:
                    raw = self._ser.readline()
                    line = raw.decode("utf-8", errors="ignore").strip()

                    # '$' で始まる行だけパース
                    if not line.startswith("$"):
                        continue

                    parts = line[1:].split(",")
                    if len(parts) < 2:
                        continue

                    try:
                        angle = int(parts[0])
                        dist  = float(parts[1])
                        self.data_received.emit(angle, dist)
                    except ValueError:
                        pass

            except serial.SerialException:
                self.connection_changed.emit(False)
                time.sleep(2)   # 再接続まで待機
            finally:
                if self._ser and self._ser.is_open:
                    self._ser.close()

    def stop(self):
        self._running = False
        if self._ser and self._ser.is_open:
            self._ser.close()
        self.wait()


# ── レーダーウィジェット ─────────────────────────────────────────────
class RadarWidget(pg.PlotWidget):
    """
    PyQtGraph ベースのレーダー表示。
    - 半円グリッド（距離リング + 角度線）
    - 走査線（現在角度の緑ライン）
    - 検出点（時間経過でフェードアウト）
    """

    def __init__(self):
        super().__init__()
        self._points        = deque(maxlen=MAX_POINTS)  # (x, y, timestamp)
        self._current_angle = 90
        self._current_dist  = -1.0

        self._setup_view()
        self._draw_background()

        # 検出点
        self._scatter = pg.ScatterPlotItem(size=7, pen=None)
        self.addItem(self._scatter)

        # 走査線
        self._scan_line = pg.PlotCurveItem(
            pen=pg.mkPen("#00ff41", width=2)
        )
        self.addItem(self._scan_line)

        # 描画タイマー
        self._timer = QTimer()
        self._timer.timeout.connect(self._refresh)
        self._timer.start(1000 // FPS)

    def _setup_view(self):
        margin = MAX_DIST * 0.18
        self.setBackground("#060e06")
        self.setAspectLocked(True)
        self.setXRange(-MAX_DIST - margin, MAX_DIST + margin)
        self.setYRange(-margin, MAX_DIST + margin)
        self.hideAxis("left")
        self.hideAxis("bottom")
        self.setMouseEnabled(False, False)

    def _draw_background(self):
        grid_pen  = pg.mkPen("#0d3b0d", width=1)
        label_col = "#1a6b1a"

        # 距離リング（半円）
        theta = [math.radians(a) for a in range(0, 181)]
        for r in DIST_RINGS:
            xs = [r * math.cos(t) for t in theta]
            ys = [r * math.sin(t) for t in theta]
            self.addItem(pg.PlotCurveItem(xs, ys, pen=grid_pen))

            lbl = pg.TextItem(f"{r}cm", color=label_col, anchor=(0.5, 1.1))
            lbl.setPos(0, r)
            self.addItem(lbl)

        # 角度線（30° ごと）
        for a in range(0, 181, 30):
            rad = math.radians(a)
            ex  = MAX_DIST * math.cos(rad)
            ey  = MAX_DIST * math.sin(rad)
            self.addItem(pg.PlotCurveItem([0, ex], [0, ey], pen=grid_pen))

            # 角度ラベル（外縁に配置）
            lx = (MAX_DIST + 10) * math.cos(rad)
            ly = (MAX_DIST + 10) * math.sin(rad)
            lbl = pg.TextItem(f"{a}°", color=label_col, anchor=(0.5, 0.5))
            lbl.setPos(lx, ly)
            self.addItem(lbl)

        # ベースライン
        self.addItem(pg.PlotCurveItem(
            [-MAX_DIST * 1.1, MAX_DIST * 1.1], [0, 0], pen=grid_pen
        ))

        # 原点マーカー
        origin = pg.ScatterPlotItem(
            [{"pos": (0, 0), "size": 6, "brush": pg.mkBrush("#00ff41")}]
        )
        self.addItem(origin)

    def update_data(self, angle: int, dist: float):
        self._current_angle = angle
        self._current_dist  = dist

        # 有効距離のみバッファに追加
        if 0 < dist <= MAX_DIST:
            rad = math.radians(angle)
            x   = dist * math.cos(rad)
            y   = dist * math.sin(rad)
            self._points.append((x, y, time.monotonic()))

    def _refresh(self):
        now = time.monotonic()

        # 走査線更新
        rad = math.radians(self._current_angle)
        lx  = MAX_DIST * math.cos(rad)
        ly  = MAX_DIST * math.sin(rad)
        self._scan_line.setData([0, lx], [0, ly])

        # フェード付き散布図
        spots = []
        for x, y, t in self._points:
            age = now - t
            if age > FADE_SEC:
                continue
            alpha = int(255 * (1.0 - age / FADE_SEC))
            spots.append({
                "pos":   (x, y),
                "brush": pg.mkBrush(0, 255, 65, alpha),
            })
        self._scatter.setData(spots)


# ── メインウィンドウ ─────────────────────────────────────────────────
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Radar Scanner - Phase 2")
        self.resize(850, 700)
        self.setStyleSheet("background:#0a0f0a; color:#00ff41;")

        self._reader = None

        central = QWidget()
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)
        layout.setSpacing(6)

        # ── ツールバー ──
        bar = QHBoxLayout()

        bar.addWidget(self._styled_label("Port:"))
        self._port_combo = QComboBox()
        self._port_combo.setFixedWidth(140)
        self._refresh_ports()
        bar.addWidget(self._port_combo)

        refresh_btn = self._styled_button("Refresh")
        refresh_btn.clicked.connect(self._refresh_ports)
        bar.addWidget(refresh_btn)

        self._connect_btn = self._styled_button("Connect")
        self._connect_btn.clicked.connect(self._toggle_connect)
        bar.addWidget(self._connect_btn)

        self._conn_label = QLabel("● Disconnected")
        self._conn_label.setStyleSheet("color:#ff4444; font-size:13px;")
        bar.addWidget(self._conn_label)

        bar.addStretch()
        layout.addLayout(bar)

        # ── レーダー ──
        self._radar = RadarWidget()
        layout.addWidget(self._radar)

        # ── ステータスバー ──
        info = QHBoxLayout()
        self._angle_lbl = self._styled_label("Angle: ---°", size=14)
        self._dist_lbl  = self._styled_label("Distance: ---.-- cm", size=14)
        info.addWidget(self._angle_lbl)
        info.addSpacing(40)
        info.addWidget(self._dist_lbl)
        info.addStretch()
        layout.addLayout(info)

    # ── ヘルパー ──
    def _styled_label(self, text: str, size: int = 12) -> QLabel:
        lbl = QLabel(text)
        lbl.setStyleSheet(f"color:#00ff41; font-size:{size}px;")
        return lbl

    def _styled_button(self, text: str) -> QPushButton:
        btn = QPushButton(text)
        btn.setStyleSheet(
            "QPushButton { color:#00ff41; border:1px solid #00ff41;"
            " background:#0a0f0a; padding:4px 10px; }"
            "QPushButton:hover { background:#0d2b0d; }"
        )
        return btn

    def _refresh_ports(self):
        self._port_combo.clear()
        for p in serial.tools.list_ports.comports():
            self._port_combo.addItem(p.device)

    def _toggle_connect(self):
        if self._reader and self._reader.isRunning():
            self._reader.stop()
            self._reader = None
            self._connect_btn.setText("Connect")
        else:
            port = self._port_combo.currentText()
            if not port:
                return
            self._reader = SerialReader(port, BAUD_RATE)
            self._reader.data_received.connect(self._on_data)
            self._reader.connection_changed.connect(self._on_connection)
            self._reader.start()
            self._connect_btn.setText("Disconnect")

    def _on_data(self, angle: int, dist: float):
        self._radar.update_data(angle, dist)
        self._angle_lbl.setText(f"Angle: {angle}°")
        if dist > 0:
            self._dist_lbl.setText(f"Distance: {dist:.1f} cm")
        else:
            self._dist_lbl.setText("Distance: OUT OF RANGE")

    def _on_connection(self, connected: bool):
        if connected:
            self._conn_label.setText("● Connected")
            self._conn_label.setStyleSheet("color:#00ff41; font-size:13px;")
        else:
            self._conn_label.setText("● Disconnected")
            self._conn_label.setStyleSheet("color:#ff4444; font-size:13px;")

    def closeEvent(self, event):
        if self._reader:
            self._reader.stop()
        super().closeEvent(event)


# ── エントリーポイント ───────────────────────────────────────────────
if __name__ == "__main__":
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    win = MainWindow()
    win.show()
    sys.exit(app.exec_())
