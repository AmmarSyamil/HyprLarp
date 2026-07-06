from textual.app import App, ComposeResult
from textual.widgets import Header, Footer, Label, Static
from textual.containers import Container
from textual.widget import Widget
from textual import events
from textual.geometry import Size
import cv2
from hyprpy import Hyprland
# import pyautogui
import subprocess, json, os
from shutil import get_terminal_size


def GetCellAspec():
        try:
            result = subprocess.run(["hyprctl", "clients", "-j"], capture_output=True, text=True)
            clients = json.loads(result.stdout)
            pid = os.getpid()
            for client in clients:
                if client.get("pid") == pid:
                    w_px = client["size"]["width"]
                    h_px = client["size"]["height"]
                    break
            else:
                return 0.5  # fallback (typical 8x16 font)
            cols, rows = get_terminal_size()
            return (w_px / cols) / (h_px / rows)
        except Exception as e:
            print("Cell aspect fallback:", e)
            return 0.5

# Widget of the video rectangle area, which can be dragged around the screen
class VideoArea(Static):
    DEFAULT_CSS = """
    VideoArea {
        border: solid red;
        background: rgba(255, 0, 0, 0.1);

        position: absolute;
    }
    """

    def __init__(self, monitor_ratio, video_ratio, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.dragging = False
        self.mouse_start_x = 0
        self.mouse_start_y = 0
        self.widget_start_x = 0
        self.widget_start_y = 0


        self.cell_aspect = cell_aspect
        self.monitor_ratio = monitor_ratio
        self.video_ratio = video_ratio
        # self.styles.width = displayRatio[0] * MULTIPLIER
        # self.styles.height = displayRatio[1] * MULTIPLIER

        # Track centerized and launch
        # self.initialized_position = False

        # Set temporary
        self.styles.width = 10
        self.styles.height = 10

    def _update_size(self):
        parent = self.parent
        if parent == None:
            return
        
        pw = parent.size.width
        ph = parent.size.height

        monitor_w, monitor_h = self.monitor_ratio
        video_w, video_h = self.video_ratio

        width = int(pw *  (video_w / monitor_w))
        height = int(width * (video_h / monitor_h) * self.cell_aspect)

        


    def on_resize(self, event) -> None:
        if not self.initialized_position and self.parent:
            tw = self.parent.content_size.width
            th = self.parent.content_size.height

            offset_x = (tw - self.size.width) // 2
            offset_y = (th - self.size.height) // 2

            self.styles.offset = (offset_x, offset_y)
            self.initialized_position = True

        self._update_size()

    def on_mouse_down(self, event) -> None:
        self.dragging = True
        self.capture_mouse() #if mouse move fast

        # initial mouse position
        self.mouse_start_x = event.screen_x
        self.mouse_start_y = event.screen_y

        # initial widget position
        self.widget_start_x = int(self.styles.offset.x.value)
        self.widget_start_y = int(self.styles.offset.y.value)

    def on_mouse_move(self, event) -> None:
        if (self.dragging):
            delta_x = event.screen_x - self.mouse_start_x
            delta_y = event.screen_y - self.mouse_start_y
            
            self.styles.offset = (
                self.widget_start_x + delta_x, 
                self.widget_start_y + delta_y
            )

    def on_mouse_up(self, event) -> None:
        if self.dragging:
            self.dragging = False
            self.release_mouse()

    def on_mount(self, event):
        self._update_size()

class WorkspaceArea(Widget):
    DEFAULT_CSS = """
    WorkspaceArea {
        border: solid green;
        background: rgba(0, 255, 0, 0.1);
        
        position: absolute;
    }
    """
    def __init__(self, displayRatio, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.displayRatio = displayRatio
        self.cell_aspect = GetCellAspec()

    def _update_size(self):
        container = self.parent
        if container is None:
            return
        tw = container.content_size.width
        th = container.content_size.height
        ratio_w, ratio_h = self.displayRatio

        # Desired size (with 0.9 factor)
        width = int(tw * 0.9)
        height = int(width * (ratio_h / ratio_w) * self.cell_aspect)

        # Prevent overflowing vertically
        if height > th * 0.9:
            height = int(th * 0.9)
            width = int(height * (ratio_w / ratio_h) / self.cell_aspect)

        # print("on resize test")
        # print(width, height)



        self.styles.width = width
        self.styles.height = height

        offset_x = (tw - width) // 2
        offset_y = (th - height) // 2
        self.styles.offset = (offset_x, offset_y)

        self.refresh()

    def on_mount(self, event):
        """Called when the widget is first added to the DOM."""
        self._update_size()

    def on_resize(self, event):
        self._update_size()

class LayoutApp(App):
    def GetDisplayData(self):
        instance = Hyprland()
        monitor = instance.get_monitor_by_id(0)

        if monitor:
            width = monitor.width
            height = monitor.height
            print(width, height)
            return (width, height)

    def __init__(self, videoPath):
        super().__init__()
        self.videoPath = videoPath

        # Get monitor ratio
        self.monitorRatio = self.GetDisplayData()

        # Get video resolution
        cap = cv2.VideoCapture(self.videoPath)
        if not cap.isOpened():
            self.exit(message="Failed to read video.")
            return

        width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))

        self.videoRatio = (width, height)
        cap.release()



    def compose(self) -> ComposeResult:
        # yield Header()
        yield WorkspaceArea(displayRatio=self.monitorRatio)
        # yield VideoArea(displayRatio=self.videoRatio)
        # yield Footer()

if __name__ == "__main__":
    app = LayoutApp("/home/sp/code/hyprlarp/video.mp4")
    app.run()