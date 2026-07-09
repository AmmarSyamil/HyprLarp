import sys
import cv2
import math
import json
import os
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, 
    QLabel, QPushButton, QFrame, QSizeGrip, QFileDialog
)
from PyQt6.QtCore import Qt, QRect, QPoint, QSize
from PyQt6.QtGui import QPainter, QColor, QPen, QBrush, QCursor, QRegion, QFont

# Helper to get monitor resolution
def get_monitor_resolution():
    app = QApplication.instance()
    if app is None:
        app = QApplication(sys.argv)
    screen = app.primaryScreen()
    if screen:
        geometry = screen.availableGeometry()
        return geometry.width(), geometry.height()
    return 1920, 1080

# Helper to get video resolution
def get_video_resolution(path):
    cap = cv2.VideoCapture(path)
    if not cap.isOpened():
        print(f"Error: Failed to open video {path}")
        return 640, 480
    width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    cap.release()
    return width, height

def get_config_path():
    """Returns the absolute path for the config file."""
    config_dir = os.path.expanduser("~/.config")
    return os.path.join(config_dir, "HyprLarp.json")

class VideoArea(QWidget):
    """
    Represents the Video. 
    Draggable widget with resize handles at corners.
    """
    def __init__(self, monitor_ratio, video_ratio, parent=None):
        super().__init__(parent)
        self.monitor_ratio = monitor_ratio
        self.video_ratio = video_ratio
        
        # Visual style
        self.bg_color = QColor(255, 0, 0, 25)
        self.border_color = QColor(255, 0, 0)
        self.handle_color = QColor(255, 255, 0) # Yellow handles
        self.handle_radius = 5
        self.handle_radius_hover = 8
        
        # State
        self.dragging = False
        self.resizing = False
        self.active_corner = None
        self.start_pos = QPoint()
        self.widget_start_geo = QRect()
        
        # Scale tracking (For UI display only)
        self.current_scale = 1.0
        
        # Absolute geometry mapping
        self.real_rect = QRect(0, 0, 0, 0)
        
        # Hover state
        self.hovered_corner = None

        self.setMouseTracking(True)

    def update_real_geometry(self):
        """Updates the absolute monitor coordinates based on current widget geometry."""
        if not self.parent(): return
        ws = self.parent().current_workspace_rect
        mw, mh = self.parent().display_ratio
        
        if ws.width() <= 0 or ws.height() <= 0: return
        
        v_rect = self.geometry()
        
        rx = (v_rect.x() - ws.x()) / ws.width() * mw
        ry = (v_rect.y() - ws.y()) / ws.height() * mh
        rw = v_rect.width() / ws.width() * mw
        rh = v_rect.height() / ws.height() * mh
        
        self.real_rect = QRect(int(round(rx)), int(round(ry)), int(round(rw)), int(round(rh)))

    def get_corner_rects(self):
        r = self.rect()
        size = self.handle_radius_hover if self.hovered_corner else self.handle_radius
        return {
            'TL': QRect(r.topLeft() - QPoint(size, size), QSize(size*2, size*2)),
            'TR': QRect(r.topRight() - QPoint(-size, size), QSize(size*2, size*2)),
            'BL': QRect(r.bottomLeft() - QPoint(size, -size), QSize(size*2, size*2)),
            'BR': QRect(r.bottomRight() - QPoint(-size, -size), QSize(size*2, size*2))
        }

    def get_corner_center(self, name):
        r = self.rect()
        if name == 'TL': return r.topLeft()
        if name == 'TR': return r.topRight()
        if name == 'BL': return r.bottomLeft()
        if name == 'BR': return r.bottomRight()
        return QPoint()

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.fillRect(self.rect(), QBrush(self.bg_color))
        painter.setPen(QPen(self.border_color, 2))
        painter.drawRect(self.rect().adjusted(0, 0, -1, -1))

        corners = self.get_corner_rects()
        for name, rect in corners.items():
            radius = self.handle_radius
            if self.hovered_corner == name:
                radius = self.handle_radius_hover
            center = self.get_corner_center(name)
            painter.setBrush(QBrush(self.handle_color))
            painter.setPen(Qt.PenStyle.NoPen)
            painter.drawEllipse(center, radius, radius)

    def check_corner_hover(self, pos):
        corners = self.get_corner_rects()
        for name, rect in corners.items():
            center = self.get_corner_center(name)
            dist = (pos - center).manhattanLength()
            if dist <= self.handle_radius_hover * 1.5: 
                return name
        return None

    def mouseMoveEvent(self, event):
        pos = event.position().toPoint()
        
        if self.resizing and self.active_corner:
            self.perform_resize(pos)
            if self.active_corner in ['TL', 'BR']:
                self.setCursor(Qt.CursorShape.SizeFDiagCursor)
            else:
                self.setCursor(Qt.CursorShape.SizeBDiagCursor)
        elif self.dragging:
            self.setCursor(Qt.CursorShape.ClosedHandCursor)
            global_pos = event.globalPosition().toPoint()
            delta = global_pos - self.start_pos
            self.move(self.widget_start_geo.topLeft() + delta)
        else:
            corner = self.check_corner_hover(pos)
            if corner != self.hovered_corner:
                self.hovered_corner = corner
                self.update() 
                
            if corner:
                if corner in ['TL', 'BR']:
                    self.setCursor(Qt.CursorShape.SizeFDiagCursor)
                else:
                    self.setCursor(Qt.CursorShape.SizeBDiagCursor)
            else:
                self.setCursor(Qt.CursorShape.OpenHandCursor)

    def mousePressEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            pos = event.position().toPoint()
            corner = self.check_corner_hover(pos)
            
            if corner:
                self.resizing = True
                self.active_corner = corner
                self.start_pos = event.globalPosition().toPoint()
                self.widget_start_geo = self.geometry()
                self.grabMouse() 
            else:
                self.dragging = True
                self.start_pos = event.globalPosition().toPoint()
                self.widget_start_geo = self.geometry()
                self.grabMouse()
                self.setCursor(Qt.CursorShape.ClosedHandCursor)
                
            self.update()

    def mouseReleaseEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            self.releaseMouse()
            self.dragging = False
            self.resizing = False
            self.active_corner = None
            
            # Update the real absolute geometry when user finishes interacting
            self.update_real_geometry()
            
            self.parent().update_debug_info() 
            self.update()

    def perform_resize(self, current_pos):
        current_geo = self.geometry()
        center = current_geo.center()
        
        mouse_pos = self.mapToParent(current_pos)
        dx = abs(mouse_pos.x() - center.x())
        dy = abs(mouse_pos.y() - center.y())
        
        vw, vh = self.video_ratio
        
        w_from_dx = dx * 2
        h_from_dy = dy * 2
        
        if w_from_dx * vh >= h_from_dy * vw:
            new_w = w_from_dx
            new_h = new_w * vh / vw
        else:
            new_h = h_from_dy
            new_w = new_h * vw / vh
        
        new_w = max(20, int(new_w))
        new_h = max(20, int(new_h))
        
        base_w = self.parent().video_base_size[0]
        if base_w > 0:
            self.current_scale = new_w / base_w
            
        new_geo = QRect(0, 0, new_w, new_h)
        new_geo.moveCenter(center)
        self.setGeometry(new_geo)
        
        # Sync absolute geometry dynamically during resize
        self.update_real_geometry()
        
        self.parent().update_debug_info()

    def moveEvent(self, event):
        super().moveEvent(event)
        if self.parent():
            self.parent().update_video_mask()

    def resizeEvent(self, event):
        super().resizeEvent(event)
        if self.parent():
            self.parent().update_video_mask()

    def reset_geometry(self, rect):
        self.setGeometry(rect)
        self.current_scale = 1.0
        self.update_real_geometry()
        self.update()


class WorkspaceArea(QWidget):
    def __init__(self, display_ratio, video_ratio, parent=None):
        super().__init__(parent)
        self.display_ratio = display_ratio
        self.video_ratio = video_ratio
        
        self.video_area = None
        self.debug_label = None
        self.current_workspace_rect = QRect(0, 0, 0, 0)
        
        self.video_base_size = (0, 0)
        self._pending_config_corners = None 
        
        self.setMinimumSize(200, 150)

    def resizeEvent(self, event):
        self.update_layout()

    def update_layout(self):
        parent_w = self.width()
        parent_h = self.height()
        
        if parent_w <= 0 or parent_h <= 0: return

        mw, mh = self.display_ratio
        monitor_aspect = mw / mh
        
        max_w = parent_w * 0.9
        max_h = parent_h * 0.9

        target_w = int(max_w)
        target_h = int(target_w / monitor_aspect)

        if target_h > max_h:
            target_h = int(max_h)
            target_w = int(target_h * monitor_aspect)

        target_w = max(1, target_w)
        target_h = max(1, target_h)

        self.current_workspace_rect = QRect(0, 0, target_w, target_h)
        self.current_workspace_rect.moveCenter(self.rect().center())

        vw, vh = self.video_ratio
        scale = target_w / mw
        
        base_w = int(vw * scale)
        base_h = int(vh * scale)
        self.video_base_size = (base_w, base_h)
        
        if hasattr(self, 'no_video_label') and self.no_video_label:
            self.no_video_label.setGeometry(self.current_workspace_rect)

        if not self.video_area:
            self.update()
            return
            
        # If we have a valid absolute real_rect, map it perfectly onto the current workspace size
        if self.video_area.real_rect.width() > 0:
            ws = self.current_workspace_rect
            real_r = self.video_area.real_rect
            
            wx = int((real_r.x() / mw) * ws.width() + ws.x())
            wy = int((real_r.y() / mh) * ws.height() + ws.y())
            ww = int((real_r.width() / mw) * ws.width())
            wh = int((real_r.height() / mh) * ws.height())
            
            v_rect = QRect(wx, wy, max(20, ww), max(20, wh))
            self.video_area.setGeometry(v_rect)
            
            if vw > 0:
                self.video_area.current_scale = real_r.width() / vw
        else:
            # First spawn: Center it and initialize real_rect
            video_geo = QRect(0, 0, base_w, base_h)
            video_geo.moveCenter(self.current_workspace_rect.center())
            self.video_area.setGeometry(video_geo)
            self.video_area.current_scale = 1.0
            self.video_area.update_real_geometry()

        self.update_debug_info()
        self.update_video_mask()

    def update_video_mask(self):
        if not self.video_area:
            return
        
        video_rect = self.video_area.geometry()
        intersected = video_rect.intersected(self.current_workspace_rect)
        
        if intersected.width() <= 0 or intersected.height() <= 0:
            self.video_area.setMask(QRegion(0, 0, 0, 0))
        else:
            local_x = intersected.x() - video_rect.x()
            local_y = intersected.y() - video_rect.y()
            local_rect = QRect(local_x, local_y, intersected.width(), intersected.height())
            self.video_area.setMask(QRegion(local_rect))

    def reset_video(self):
        if self.video_area:
            # Clear real_rect so update_layout centers it again
            self.video_area.real_rect = QRect(0, 0, 0, 0)
            self.update_layout()
            
    def update_debug_info(self):
        if self.debug_label and self.video_area:
            vw, vh = self.video_ratio
            scale = self.video_area.current_scale
            msg = (
                f"Monitor: {self.display_ratio[0]}x{self.display_ratio[1]}\n"
                f"Video: {vw}x{vh}\n"
                f"Scale: x{scale:.2f}"
            )
            self.debug_label.setText(msg)
            self.debug_label.adjustSize()
            self.debug_label.move(5, self.height() - self.debug_label.height() - 5)

    def paintEvent(self, event):
        painter = QPainter(self)
        if hasattr(self, 'current_workspace_rect'):
            painter.fillRect(self.current_workspace_rect, QBrush(QColor(0, 255, 0, 20)))
            painter.setPen(QPen(QColor(0, 255, 0), 2))
            painter.drawRect(self.current_workspace_rect)

    def init_ui(self):
        self.no_video_label = QLabel("No Video Loaded\n\nClick 'Open Video' to select a file", self)
        self.no_video_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.no_video_label.setStyleSheet("""
            QLabel {
                color: #ffffff; font-size: 18px; font-weight: bold; 
                background-color: rgba(0, 0, 0, 120); border-radius: 10px; padding: 20px;
            }
        """)
        self.no_video_label.setVisible(False)
        
        self.debug_label = QLabel(self)
        self.debug_label.setStyleSheet("background-color: rgba(0,0,0,180); color: white; padding: 2px;")
        
        if self.video_ratio != (0, 0):
            self.create_video_area()
            
        self.update_layout()
        
    def create_video_area(self):
        if self.video_area: return
        
        self.video_area = VideoArea(self.display_ratio, self.video_ratio, self)
        self.video_area.setCursor(Qt.CursorShape.OpenHandCursor)
        self.video_area.show()
        
        # Apply loaded coordinates immediately
        if self._pending_config_corners is not None:
            tl, tr, bl, br = self._pending_config_corners
            w = br[0] - tl[0]
            h = br[1] - tl[1]
            self.video_area.real_rect = QRect(tl[0], tl[1], w, h)
            self._pending_config_corners = None
        
        if self.no_video_label:
            self.no_video_label.setVisible(False)

    def save_config(self, video_path):
        if not self.video_area:
            print("Cannot save: No video area loaded.")
            return
            
        # Ensure geometry is perfectly synced before saving
        self.video_area.update_real_geometry()
        real_r = self.video_area.real_rect
        
        tl = [real_r.x(), real_r.y()]
        tr = [real_r.x() + real_r.width(), real_r.y()]
        bl = [real_r.x(), real_r.y() + real_r.height()]
        br = [real_r.x() + real_r.width(), real_r.y() + real_r.height()]
        
        config = {
            "videoPath": video_path,
            "cornerLocation": [tl, tr, bl, br]
        }
        
        config_path = get_config_path()
        config_dir = os.path.dirname(config_path)
        
        if not os.path.exists(config_dir):
            os.makedirs(config_dir)
            
        with open(config_path, 'w') as f:
            json.dump(config, f, indent=4)
            
        print(f"Config saved to {config_path}")


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Video Layout Tool (Resizable)")
        self.resize(800, 600)

        self.monitor_ratio = get_monitor_resolution()
        self.video_path = ""
        self.video_ratio = (0, 0)
        self.config_corners = None
        
        self.load_config()
        
        print(f"Monitor: {self.monitor_ratio[0]}x{self.monitor_ratio[1]}")
        if self.video_ratio != (0, 0):
            print(f"Loaded Video: {self.video_ratio[0]}x{self.video_ratio[1]}")

        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)
        main_layout.setContentsMargins(20, 20, 20, 20)
        main_layout.setSpacing(10)

        self.workspace = WorkspaceArea(self.monitor_ratio, self.video_ratio)
        main_layout.addWidget(self.workspace)
        
        if self.config_corners:
            self.workspace._pending_config_corners = self.config_corners
        
        controls_layout = QHBoxLayout()
        
        self.open_btn = QPushButton("Open Video")
        self.open_btn.clicked.connect(self.open_video)
        controls_layout.addWidget(self.open_btn)
        
        self.save_btn = QPushButton("Save Config")
        self.save_btn.clicked.connect(self.save_config)
        controls_layout.addWidget(self.save_btn)
        
        self.reset_btn = QPushButton("Reset Position & Size")
        self.reset_btn.clicked.connect(self.workspace.reset_video)
        controls_layout.addStretch()
        controls_layout.addWidget(self.reset_btn)
        
        main_layout.addLayout(controls_layout)
        
        self.workspace.init_ui()
        
        if not self.video_path:
            self.workspace.no_video_label.setVisible(True)

    def load_config(self):
        config_path = get_config_path()
        if os.path.exists(config_path):
            try:
                with open(config_path, 'r') as f:
                    config = json.load(f)
                    
                v_path = config.get("videoPath", "")
                corners = config.get("cornerLocation", None)
                
                if v_path and os.path.exists(v_path) and corners and len(corners) == 4:
                    self.video_path = v_path
                    self.video_ratio = get_video_resolution(v_path)
                    self.config_corners = corners
                    print(f"Loaded config from {config_path}")
                else:
                    print(f"Config ignored: Video file missing or corners invalid.")
            except Exception as e:
                print(f"Error reading config: {e}")

    def open_video(self):
        filepath, _ = QFileDialog.getOpenFileName(self, "Open Video File", "", "Video Files (*.mp4 *.avi *.mkv *.mov)")
        if filepath:
            self.video_path = filepath
            self.video_ratio = get_video_resolution(filepath)
            
            self.workspace.video_ratio = self.video_ratio
            
            if not self.workspace.video_area:
                self.workspace.create_video_area()
            else:
                self.workspace.video_area.video_ratio = self.video_ratio
                # Force reset real_rect so it re-centers naturally
                self.workspace.video_area.real_rect = QRect(0, 0, 0, 0)
            
            self.workspace.update_layout()
            self.workspace.video_area.update()

    def save_config(self):
        if not self.video_path or not self.workspace.video_area:
            print("No video loaded to save.")
            return
            
        self.workspace.save_config(self.video_path)

def run_gui():
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())

if __name__ == "__main__":
    run_gui()