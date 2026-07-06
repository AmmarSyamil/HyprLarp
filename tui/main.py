# TUI is deprecated due to TUI limited capabilities


from textual.app import App, ComposeResult
from textual.widgets import Static
from textual.containers import Container
import cv2
from hyprpy import Hyprland
import subprocess, json, os
from shutil import get_terminal_size
import sys, tty, termios, select

def get_cell_aspect():
    def query_ansi(seq):
        try:
            fd = sys.stdin.fileno()
            old = termios.tcgetattr(fd)
            try:
                tty.setraw(fd)
                sys.stdout.write(seq)
                sys.stdout.flush()
                r, _, _ = select.select([sys.stdin], [], [], 0.2)
                if r:
                    res = ""
                    while True:
                        ch = sys.stdin.read(1)
                        if not ch:
                            break
                        res += ch
                        if ch == 't':
                            break
                    return res
            finally:
                termios.tcsetattr(fd, termios.TCSADRAIN, old)
        except:
            return None
        return None

    # 1) CSI 14 t
    resp = query_ansi("\x1b[14t")
    if resp and resp.startswith("\x1b[4;") and resp.endswith("t"):
        parts = resp[4:-1].split(";")
        if len(parts) == 2:
            h_px, w_px = int(parts[0]), int(parts[1])
            if h_px > 0 and w_px > 0:
                cols, rows = get_terminal_size()
                cell_w, cell_h = w_px / cols, h_px / rows
                aspect = cell_w / cell_h
                print(f"[CELL] CSI 14 t → text_area={w_px}x{h_px}px, cells={cols}x{rows}", flush=True)
                print(f"[CELL]   cell_width={cell_w:.2f}px, cell_height={cell_h:.2f}px, aspect={aspect:.3f}", flush=True)
                return aspect

    # 2) CSI 16 t
    resp = query_ansi("\x1b[16t")
    if resp and resp.startswith("\x1b[4;") and resp.endswith("t"):
        parts = resp[4:-1].split(";")
        if len(parts) == 2:
            h_px, w_px = int(parts[0]), int(parts[1])
            if h_px > 0 and w_px > 0:
                aspect = w_px / h_px
                print(f"[CELL] CSI 16 t → cell={w_px}x{h_px}px, aspect={aspect:.3f}", flush=True)
                return aspect

    # 3) fallback: hyprctl
    try:
        result = subprocess.run(["hyprctl", "activewindow", "-j"], capture_output=True, text=True)
        window = json.loads(result.stdout)
        w_px, h_px = window["size"]
        if h_px == 0:
            return 0.5
        cols, rows = get_terminal_size()
        cell_w, cell_h = w_px / cols, h_px / rows
        aspect = cell_w / cell_h
        print(f"[CELL] hyprctl fallback → window={w_px}x{h_px}px", flush=True)
        print(f"[CELL]   cell_width={cell_w:.2f}px, cell_height={cell_h:.2f}px, aspect={aspect:.3f}", flush=True)
        return aspect
    except Exception as e:
        print(f"[CELL] hyprctl fallback failed: {e}", flush=True)
        return 0.5


CELL_ASPECT = get_cell_aspect()
print(f"[MAIN] Using cell_aspect = {CELL_ASPECT:.3f}", flush=True)


class VideoArea(Static):
    DEFAULT_CSS = """
    VideoArea {
        border: solid red;
        background: rgba(255, 0, 0, 0.1);
        position: absolute;
    }
    """

    def __init__(self, monitor_ratio, video_ratio, cell_aspect, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.monitor_ratio = monitor_ratio
        self.video_ratio = video_ratio
        self.cell_aspect = cell_aspect

        self.dragging = False
        self.mouse_start_x = 0
        self.mouse_start_y = 0
        self.widget_start_x = 0
        self.widget_start_y = 0

        self.styles.width = 10
        self.styles.height = 10

    def update_size(self, parent_width, parent_height):
        if parent_width <= 0 or parent_height <= 0:
            return

        mw, mh = self.monitor_ratio
        vw, vh = self.video_ratio
        pixel_scale = parent_width / mw

        width = round(vw * pixel_scale)
        height = round(vh * pixel_scale * self.cell_aspect)

        if height > parent_height:
            height = parent_height
            width = round(height * (vw / vh) / self.cell_aspect)

        width = max(1, width)
        height = max(1, height)

        self.styles.width = width
        self.styles.height = height
        offset_x = (parent_width - width) // 2
        offset_y = (parent_height - height) // 2
        self.styles.offset = (offset_x, offset_y)
        self.refresh()

        return width, height, width / parent_width, height / parent_height

    def on_mount(self, event):
        pass

    def on_resize(self, event):
        pass

    # ----- drag handlers -----
    def on_mouse_down(self, event):
        self.dragging = True
        self.capture_mouse()
        self.mouse_start_x = event.screen_x
        self.mouse_start_y = event.screen_y
        self.widget_start_x = int(self.styles.offset.x.value)
        self.widget_start_y = int(self.styles.offset.y.value)

    def on_mouse_move(self, event):
        if self.dragging:
            dx = event.screen_x - self.mouse_start_x
            dy = event.screen_y - self.mouse_start_y
            self.styles.offset = (self.widget_start_x + dx, self.widget_start_y + dy)

    def on_mouse_up(self, event):
        if self.dragging:
            self.dragging = False
            self.release_mouse()

class WorkspaceArea(Container):
    DEFAULT_CSS = """
    WorkspaceArea {
        border: solid green;
        background: rgba(0, 255, 0, 0.1);
        position: absolute;
    }
    .debug {
        dock: bottom;
        background: rgba(0, 0, 0, 0.8);
        color: white;
        padding: 1;
        width: 100%;
        height: auto;
        text-style: bold;
    }
    """

    def __init__(self, display_ratio, video_ratio, cell_aspect, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.display_ratio = display_ratio
        self.video_ratio = video_ratio
        self.cell_aspect = cell_aspect
        self.video_area = None
        self.debug_label = None

        self.styles.width = 10
        self.styles.height = 10

    def _update_size(self):
        container = self.parent
        if container is None:
            self.set_timer(0.05, self._update_size)
            return

        tw = container.content_size.width
        th = container.content_size.height
        if tw <= 0 or th <= 0:
            self.set_timer(0.05, self._update_size)
            return

        mw, mh = self.display_ratio
        width = int(tw * 0.9)
        height = round(width * (mh / mw) * self.cell_aspect)

        if height > th * 0.9:
            height = int(th * 0.9)
            width = round(height * (mw / mh) / self.cell_aspect)

        width = max(1, width)
        height = max(1, height)

        # Apply workspace size
        self.styles.width = width
        self.styles.height = height
        offset_x = (tw - width) // 2
        offset_y = (th - height) // 2
        self.styles.offset = (offset_x, offset_y)
        self.refresh()

        # Update video area and get its stats
        video_stats = None
        if self.video_area:
            stats = self.video_area.update_size(width, height)
            if stats:
                video_stats = stats

        # Update debug label
        if self.debug_label:
            msg = (
                f"Terminal: {tw}×{th}  |  Workspace: {width}×{height}  ({width/tw:.3f}%, {height/th:.3f}%)\n"
                f"Video: "
            )
            if video_stats:
                vw, vh, vwp, vhp = video_stats
                msg += f"{vw}×{vh}  ({vwp:.3f}%, {vhp:.3f}%)"
            else:
                msg += "not computed"
            self.debug_label.update(msg)

    def compose(self):
        self.video_area = VideoArea(
            monitor_ratio=self.display_ratio,
            video_ratio=self.video_ratio,
            cell_aspect=self.cell_aspect
        )
        self.debug_label = Static("Initialising...", classes="debug")
        yield self.video_area
        yield self.debug_label

    def on_mount(self, event):
        self._update_size()
        self.set_timer(0.05, self._update_size)

    def on_resize(self, event):
        self._update_size()

class LayoutApp(App):
    def __init__(self, videoPath, cell_aspect):
        super().__init__()
        self.videoPath = videoPath
        self.cell_aspect = cell_aspect

        instance = Hyprland()
        monitor = instance.get_monitor_by_id(0)
        if monitor:
            self.monitorRatio = (monitor.width, monitor.height)
            print(f"Monitor: {monitor.width}x{monitor.height}", flush=True)
        else:
            self.monitorRatio = (1920, 1080)

        cap = cv2.VideoCapture(videoPath)
        if not cap.isOpened():
            self.exit(message="Failed to read video.")
            return
        width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        self.videoRatio = (width, height)
        cap.release()
        print(f"Video: {width}x{height}", flush=True)

    def compose(self) -> ComposeResult:
        yield WorkspaceArea(
            display_ratio=self.monitorRatio,
            video_ratio=self.videoRatio,
            cell_aspect=self.cell_aspect
        )


if __name__ == "__main__":
    app = LayoutApp("/home/sp/code/hyprlarp/video.mp4", CELL_ASPECT)
    app.run()