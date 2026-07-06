from textual.app import App, ComposeResult
from textual.containers import Container
from textual_image.widget import Image
import cv2
from PIL import Image as PILImage
import math

class Main(App):
    CSS = """
    #container { width: 100%; height: 100%; }
    #image { width: 100%; height: 100%; }
    """

    def compose(self) -> ComposeResult:
        with Container(id="container"):
            # Placeholder image (will be replaced on mount)
            dummy = PILImage.new("RGB", (1, 1), color="black")
            yield Image(dummy, id="image")

    def on_mount(self) -> None:
        # Extract 8 frames from the video
        self.frames = self.extract_frames(self.video_path, num_frames=8)
        if not self.frames:
            self.exit(message="Failed to read video or no frames.")
            return

        # Display the first frame immediately
        self.current_index = 0
        self.update_image(self.frames[self.current_index])

        # Start a timer to advance every second
        self.set_interval(1.0, self.advance_frame)

    def extract_frames(self, video_path, num_frames=8):
        cap = cv2.VideoCapture(video_path)
        if not cap.isOpened():
            return []

        total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
        if total_frames == 0:
            cap.release()
            return []

        # Choose frame indices: spread evenly across the video
        indices = [int((i + 0.5) * total_frames / num_frames) for i in range(num_frames)]
        # Clamp to valid range
        indices = [min(max(idx, 0), total_frames - 1) for idx in indices]

        frames = []
        for idx in indices:
            cap.set(cv2.CAP_PROP_POS_FRAMES, idx)
            ret, frame = cap.read()
            if not ret:
                break
            # Convert BGR -> RGB and make PIL Image
            frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            pil_img = PILImage.fromarray(frame_rgb)
            frames.append(pil_img)

        cap.release()
        return frames

    def update_image(self, pil_img):
        image_widget = self.query_one("#image", Image)
        image_widget.image = pil_img
        # Force a refresh (sometimes needed)
        image_widget.refresh()

    def advance_frame(self) -> None:
        # Move to next frame, loop back to start
        self.current_index = (self.current_index + 1) % len(self.frames)
        self.update_image(self.frames[self.current_index])

    def __init__(self, video_path):
        super().__init__()
        self.video_path = video_path
        # We don't need metadata now, but we could keep it
        self.fps = 1.0  # not used

if __name__ == "__main__":
    app = Main("/home/sp/code/hyprlarp/video.mp4")
    app.run()