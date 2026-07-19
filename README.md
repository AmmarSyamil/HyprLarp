# HyprLarp
A Hyprland utility that transforms multiple Kitty terminal windows into a single synchronized video canvas.

HyprLarp automatically detects the layout of terminal windows on a workspace, calculates the visible region of each window, and renders only the appropriate portion of the video using the Kitty Graphics Protocol.

---
## Preview
https://github.com/user-attachments/assets/17e744c0-98ad-43b6-836a-fc83983d261c

---
## How It Works

HyprLarp consist of producer and consumer.

The producer process is responsible for

- Reading the video
- Monitoring Hyprland events
- Detecting terminal geometry
- Computing viewport layouts

The resulting layout will be given to each consumer.
Each consumer only renders the portion of the video visible inside its terminal window.

---

## Requirements


Currently supported on

- Arch Linux
- Hyprland
- Kitty Terminal

Dependencies include

- FFmpeg
- simdjson
- Qt6
- CMake

---


## Instalation
### AUR

```bash
yay -S hyprlarp
```

### Build from source

```bash
git clone https://github.com/AmmarSyamil/HyprLarp.git

cd HyprLarp

mkdir build
cd build

cmake ..
make

sudo make install
```

---
## Usage 
Launch the configuration utility

```bash
HyprLarp -s # Or HyprLarp --setting
```

Start the producer manually

```bash
HyprLarp -p # Or HyprLarp --producer
```

Normal usage

```bash
HyprLarp
```

Display help

```bash
HyprLarp -h # Or HyprLarp --help
```

## Configuration
Configuration file is set by the configuration utility (-s flag).

```text
~/.config/HyprLarp.json
```

Example

```json
{
    "videoPath": "/home/user/Videos/demo.mp4",

    "cornerLocation": [
        [0, 0],
        [1920, 0],
        [0, 1080],
        [1920, 1080]
    ]
}
```

## Limitations

Current limitations

- Kitty terminal only
- Hyprland only
- Single-workspace rendering
- Linux only
- Only maintained for Arch based distros

---

## AI Usage

Artificial intelligence was used as a development assistant for

- debugging difficult geometry calculations
- discussing viewport algorithms
- GUI prototyping (coloring and styling)
- documentation improvements

All architectural decisions, implementation, testing, and final code were completed and reviewed manually.

---


## License

MIT License
