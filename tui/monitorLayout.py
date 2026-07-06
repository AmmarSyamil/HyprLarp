from textual.app import App, ComposeResult
from textual.containers import Container
from textual.widgets import Widget, Header, Footer
from textual import events
from textual.reactive import reactive

class Main(App):
    def compose(self) -> ComposeResult:
        yield Header()
        yield Footer()

if __name__ == "__main__":
    Main().run()