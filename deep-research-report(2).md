# Architecture Overview  

Design the system as a **single-decoder, multi-client pipeline**: one *producer* process (using FFmpeg or similar) decodes the video into raw frames and writes them to shared memory; multiple *consumer* processes (one per terminal/Kitty window) independently map that same memory to read each new frame, apply their own filters, and send the result to the terminal’s graphics engine.  This “1-to-many” shared-memory pattern avoids redundant decoding and copying【46†L339-L347】.  The key layers are: 

- **Decoder process:** opens the video (e.g. via libavcodec/FFmpeg), decodes frames to raw pixel buffers, and writes them into a shared memory buffer.  
- **Shared-memory transport:** a POSIX shared-memory region (`shm_open`/`mmap` or `/dev/shm` file) holds the latest frame(s) and some control metadata (frame index, size). A lightweight synchronization mechanism (atomic flags, semaphores or futexes) notifies clients of new frames. This layer handles buffer ownership and mutual exclusion.  
- **Client processes:** each Kitty window runs a client that `mmap`s the shared memory, polls or waits for new frames, then applies its own effect (ASCII art, grayscale, crop, etc.) to the raw pixel data. Finally, the client outputs the image to its terminal using Kitty’s graphics protocol.  

This decoupling means the video is decoded *only once*, and all clients see the same source frames. Each client can then process and render independently, at its own speed.  In effect, the shared memory acts like a “frame conveyor belt” that multiple workers can tap into. This is analogous to the Shmdata/Sh4lt model: one writer making data available to many followers via shared memory【46†L339-L347】【44†L262-L270】.

## Data Flow and Component Responsibilities  

- **Decoder (Producer)**: Reads video packets, decodes to raw RGBA (or RGB) frames, and writes frames into shared memory. After writing a frame, it updates a shared atomic “frame counter” or flips a buffer index to signal a new frame is available. It then repeats for the next frame, using either the same memory area (circularly) or alternate buffers. The decoder may also resize/downscale or pre-convert to RGBA if needed, to match the desired output format (Kitty supports 24‑bit RGB or 32‑bit RGBA【42†L1029-L1038】).  
- **Shared-Memory Transport Layer**: Manages the POSIX shared memory region(s). It defines the memory layout: typically a small header (width, height, frame number, possibly a generation flag or lock) plus the raw pixel array. It ensures memory is sized to hold a full frame (e.g. width×height×4 bytes for RGBA). The transport layer also handles synchronization: e.g. using an atomic frame index, condition flag, or POSIX semaphore to notify clients when a new frame is ready, and possibly a mutex to prevent readers from seeing a half-written frame. The goal is minimal overhead: ideally zero-copy delivery, with only a few bytes of metadata updated per frame.  
- **Clients (Consumers)**: Each client opens the same shared-memory region. It repeatedly checks for a new frame (polling an atomic frame counter or waiting on a semaphore). When a new frame is detected, the client reads the raw pixels from the buffer, applies its filter/effect, and sends the final image to the terminal. For Kitty terminals, the client outputs the image via the Kitty graphics protocol (see “Kitty Rendering” below). Each client is independent: it can lag behind (skipping frames if slow) or run faster, without blocking the producer or other clients.  

In summary, data flows from decoder → shared memory → each client → terminal output. This clean separation (Figure below) lets us optimize each part independently.  

- *Optional Architecture Diagram:* A single shared buffer (or circular buffer) is written by the decoder; multiple readers tap it. Metadata (frame index, dims) synchronizes reads/writes. Each client then does local post-processing and renders.

## Shared-Memory Buffering Strategy  

**Single vs. Double vs. Ring Buffer:** A simple approach is to use **double buffering**: allocate two equal-size frame buffers in shared memory and alternate which one is written. The decoder writes into buffer A while clients read from B, then swap. This avoids readers seeing a half-updated frame and doesn’t require locking beyond a pointer swap or atomic flag. A slight variant is a small *ring* of N buffers (circular buffer), allowing the producer to run slightly ahead of the slowest client (but at the cost of more memory and index management). For most terminal playback, two buffers suffice and minimize RAM usage.

**Buffer Layout:** The shared memory region can hold:  

- A header struct (written by producer) containing: `{ uint32_t width, height, stride, frame_id; }` and possibly a `bool ready` flag or two indices.  
- Two frame buffers of size `width×height×4` (for RGBA).  

The producer toggles a buffer index and increments `frame_id` atomically. Each client tracks the last `frame_id` it displayed; when it sees a new `frame_id`, it reads from the corresponding buffer. This “swap” or generation counter pattern ensures low-latency handoff. Buffer addresses are fixed (clients can compute offsets once after mmap).

Because Kitty’s protocol expects RGB/RGBA pixels【42†L1029-L1038】, we typically use 32-bit RGBA in-memory. Using YUV would save space but forces each client to do the expensive color conversion before rendering, so for simplicity we share pre-converted RGB(A) frames. No PNG or compression is used internally, since the goal is raw performance and terminals like Kitty can handle raw data directly.

## Synchronization Strategy  

With one producer and many consumers, the main challenge is letting clients know when a new frame is available *without* heavy locking. Possible strategies:  

- **Atomic Frame Counter:** Use an atomic (or volatile) integer `frame_id` in shared memory. The decoder increments it after writing each frame. Each client busy-waits (or sleeps) and polls this counter. When `frame_id` changes, a new frame is ready. A simple short sleep (e.g. 1–5 ms) between polls can reduce CPU spin. This avoids kernel calls but may waste cycles.  

- **Futex or Condition Variable:** For lower CPU use, one could put a futex or POSIX condition in shared memory to wake clients. The decoder would do `futex_wake` or signal a condition after writing. Clients do `futex_wait`. However, futexes are Linux-specific and more complex. A basic pthread mutex/cond could work with `PTHREAD_PROCESS_SHARED` in POSIX shared memory.  

- **POSIX Semaphores:** Named semaphores (`sem_open`) can broadcast an event. For example, the decoder `sem_post` on a “newframe” semaphore after each frame. Clients all `sem_wait`. The issue is that a POSIX semaphore decrements on wait, so you’d either need a separate semaphore per client or carefully re-post for each. This complicates scaling to an arbitrary number of clients.  

- **Eventfd or Pipe:** The decoder could write a byte to a named pipe or eventfd each frame; clients could select/poll that fd. This again requires one mechanism per client to avoid conflicts.  

Given complexity, **polling the atomic counter** is simplest and often sufficient for terminal playback (terminals don’t need 60fps smoothness anyway). For very low latency, one might combine busy-poll with a short nanosleep. The header can include a memory barrier (C++ `atomic_thread_fence`) around the `frame_id` update to ensure memory visibility.

Critically, **buffer ownership** must be clear: producer writes into one buffer while clients only read from the other (double-buffer scheme), avoiding simultaneous write/read on the same area. Alternatively, use a single buffer but protect it: e.g. producer sets `ready=false` before writing, writes entire frame, then sets `ready=true` and updates `frame_id`. Clients skip reading until `ready==true`. Either way, only final, fully-written frames are used by clients.

## Frame Format and Pixel Lifecycle  

Internally we use raw RGB(A) frames (8 bits per channel) in shared memory. Kitty’s graphics protocol requires uncompressed pixels (or PNG)【42†L1029-L1038】, so supplying raw pixels matches Kitty’s `f=32` or `f=24` modes. By using RGBA, we avoid any runtime decoding/encoding: all clients get immediate color data. (If desired, a variant could store only Luma or YUY2 frames to let clients do colored ASCII, but that’s extra work.)

### Pixel Format Choice  
- **RGBA (32-bit):** Client can easily produce full-color output. Kitty’s default is 32-bit RGBA【42†L1029-L1038】. The frame buffer size is `width*height*4` bytes.  
- **RGB (24-bit):** Slightly smaller buffer (`width*height*3`) if we drop alpha. Works too (`f=24`). However, many graphics libraries pad to 32 bits for speed. We’ll likely use RGBA and set the alpha to 255.  
- **YUV:** More compact (one plane per channel), but Kitty **cannot** render YUV directly; plus all clients would have to convert to RGB anyway. Hence we skip YUV.

We avoid any compressed formats (PNG/JPEG) because encoding/decoding frames adds latency and CPU cost. Instead, raw bitmaps in shared memory give “zero-copy” semantics: the producer’s output is exactly what consumers read【46†L339-L347】.

## Producer-Consumer Model (1 Writer, Many Readers)  

This pipeline is a classic one-writer, many-readers IPC pattern. As cited in related projects, one producer process makes frames available and multiple follower processes consume them【46†L339-L347】. Each client “sees” every frame (unlike a work queue). We treat each new frame as a broadcast, not as a load-balanced message.

**Concurrency:** Since only one writer exists, we never have write-write conflicts. Multiple readers may read concurrently without locks if we ensure the producer has finished writing a frame first (via the swap/flag method above). Readers do not remove frames; they just observe the latest data. After reading, a client can mark it has processed that frame by remembering the `frame_id`; no need to write back. This non-destructive consumption fits video streams well (clients may miss frames if slow, but the stream continues). 

**Synchronization Simplified:** To avoid complex handshaking, design the pipeline for “best effort” delivery: Producer continuously writes frames at its own pace; if a client lags, it simply skips intermediate frames and shows the newest when it catches up. This is acceptable in video (some frame dropping is fine) and keeps latency low.

## Client Frame Detection  

Each client repeatedly does roughly: 
``` 
prev_id = initial_frame_id;
loop {
    if (shared_header.frame_id != prev_id) {
        prev_id = shared_header.frame_id;
        // new frame is ready
        read pixels from shared buffer
        process/render them
    }
    // small sleep or yield to avoid busy loop
}
``` 
This simple polling loop is easy to implement. For minimal latency, a short `usleep(1000)` or even no sleep might be used, at the expense of CPU usage. Because terminal output itself is slow, a modest sleep (1–5 ms) often doesn’t hurt visual smoothness. If needed, one could use `futex` waiting on `frame_id` to sleep until it changes, but that complexity can be added later.

## Kitty Rendering Integration  

Once a client has a processed frame (in RGB/RGBA pixels), it needs to display it in its Kitty window. Kitty supports images via its **graphics protocol**, using sequences like `ESC _G ... ESC \`【42†L1029-L1038】【42†L1098-L1104】. Notably, Kitty can ingest pixel data from a POSIX shared memory object directly (`t=s` mode【42†L1098-L1104】), which fits our approach:

1. **Create a shared memory object for output:** The client can create a new POSIX shm name (e.g. `/myterm-frame123`). It then `mmap` or write the pixels to that object.  
2. **Send Kitty escape with `t=s`:** The client writes to its stdout (the terminal) an escape command: `<ESC>_Gf=32,s=<width>,v=<height>,t=s;<shm-name><ESC>\` (with any needed keys). This tells Kitty to read the pixel data from the given shm and display it. Kitty will then unlink the shm (as per protocol)【42†L1098-L1104】.  
3. **Cleanup:** After writing the escape code, the client can close/unlink the shared memory (Kitty should have already cleaned it up, but it’s safe to do).  

Kitty will display the new image at the current cursor position. Using `t=s` avoids base64 encoding overhead. (Kitty also supports sending raw pixel in the escape payload `t=d`, but that requires base64 and is slower; `t=s` is ideal when local shared memory is available【42†L1098-L1104】.)

Clients may also choose to use `--vo=kitty` via mpv for output, but since we already have raw data, directly emitting Kitty graphics sequences gives more control (especially since mpv would re-decode or compress the data). In essence, each client handles its own “presentation” by feeding Kitty’s API the pixels it computed.

## Memory Layout and Lifetime  

The POSIX shared memory (e.g. via `shm_open` or `mmap /dev/shm`) must live as long as any process needs it. Typical flow:

- **Initialization:** The producer creates the shm (with a name or anonymous if forked) and ftruncates it to the needed size. All clients open the same name or get the file descriptor.  
- **Mapping:** Each process `mmap`s the shm into its address space. The memory remains shared until all mappings and file handles are closed.  
- **Usage:** The producer writes to it each frame; clients read. This continues until termination.  
- **Cleanup:** On exit, the producer (or all processes collectively) should `munmap` and `shm_unlink` the object name. Clients likewise unmap. It’s wise to handle signals so that unexpected termination still cleans up (e.g. using atexit or SIGINT handler to unlink).  

Memory can be laid out as: `[Header|Buffer0|Buffer1]`. For example, if width=640, height=360, RGBA, each buffer is 921600 bytes. The header might be 16-32 bytes. Ensure proper alignment (e.g. align frame buffers to page boundaries if needed). If using one buffer, drop the second slot and use a “toggle” bit in the header.

## Latency and Performance Considerations  

This design minimizes latency by avoiding unnecessary copies or codecs. The raw frame path is: decoder → shared memory → client → terminal. No frame encoding (PNG, JPEG) is done, and no network stack is involved. Each consumer reads directly from memory, so there’s zero copy overhead in the OS (beyond the initial decode). As noted by similar libraries, “access data streams without the need for extra copies”【46†L339-L347】. 

However, some overhead remains: the CPU cost of decoding and per-client processing, the cost of writing the Kitty escape sequences, and any memory fencing between processes. To keep things smooth: 
- Use hardware decode if available (`--hwdec`) in the producer to reduce CPU usage.  
- Downscale or reduce FPS early if the client terminals cannot keep up (e.g. `--vf=fps=30,scale=854:-2` in mpv reduces data). A lower frame size drastically cuts memory bandwidth.  
- Disable unnecessary features (OSD, subtitles) in all components.  
- Since Kitty is itself a GPU-accelerated terminal, each client’s final draw is handled in hardware inside Kitty; our pipeline only needs to push the frames. 

Kitty’s own limitations also matter: its graphics protocol may have rate limits or resize latency. In practice, a few dozen FPS at moderate resolution is usually fine on modern machines.

## Scaling to Many Clients  

This architecture easily scales to *N* terminals. The producer’s workload does **not** increase when adding clients: it still decodes the video once. The shared memory is shared, so memory usage is constant (plus header overhead). Each additional client incurs only its own CPU/GPU cost for post-processing and rendering, not extra decoding. 

Possible scalability issues:  
- If clients run on different user sessions or containers, ensure they have permission to open the same shm object. On the same machine/user, it works by default.  
- The synchronization method should not bottleneck on many clients. Polling via atomic counters doesn’t slow with more readers. If semaphores or locks were used, one might worry about contention, but simple atomic flags avoid that.  
- The only shared resource contention is memory bus bandwidth (multiple CPUs reading the same large buffer simultaneously), but this is usually small compared to decoding cost.  

In short, many readers is cheap: it’s the decoding and client-specific work that are the limits. As noted, shared-memory IPC is “ultra-fast (zero-copy) data transfer” and “scales easily”【40†L29-L38】, so adding clients should not dramatically affect performance on the producer side. Each client’s latency depends only on its own processing, not on others.

## Debugging and Testing Strategy  

To verify and debug:  
- **Frame sanity:** For testing, have the producer write a color-bar or frame counter overlay on each frame. Clients should correctly display these. A sudden misalignment indicates sync issues.  
- **Logging:** Have clients log the frame_id they receive; if a client freezes, it won’t see increments.  
- **Shared-memory inspection:** Tools like `ipcs -m` or inspecting `/dev/shm` can confirm shm objects exist and their sizes.  
- **Isolation test:** Run a dummy producer that increments a counter every second; clients can watch this to verify polling/synchronization.  
- **Kitty output:** Kitty has a known behavior for large images; test with a static image before the video loop. The Kitty graphics protocol docs【42†L1098-L1104】 show examples to confirm correct escape codes.  

A useful approach is to start with a very small resolution and no filters. Ensure a single client can display the basic frames. Then scale up resolution and add a second client. This incremental testing catches issues early.

## Implementation Phases  

1. **Prototype Single-Buffer Pipeline:**  
   - Write a simple C/Python program that decodes one frame (e.g. using FFmpeg CLI with `-f rawvideo -pix_fmt rgba` to stdout) and writes to a shm.  
   - Write one client program that `mmap`s that shm, reads the frame, and uses the kitty protocol to display it. (Alternatively, pipe rawvideo into `mpv --vo=rawvideo`, but then you still need to feed Kitty.)  
   - Verify the one-producer, one-consumer works end-to-end.  

2. **Double Buffering and Frame Loop:**  
   - Extend the prototype so the producer writes frames in a loop, alternating buffers or toggling a flag. Update a frame_id. Clients loop to detect changes and re-render.  
   - Test with multiple clients (open multiple terminal tabs running the same client). Ensure each gets updated frames.  

3. **Add Client Processing:**  
   - Add example effects in the client code (convert to grayscale, ASCII art, edge detect). Confirm they operate correctly and independently on each terminal. Each client reads the **same** raw frame but produces a different output.  

4. **Synchronization Refinement:**  
   - If simple polling causes too much CPU, introduce an efficient wake-up (futex or semaphores). This can be done after correctness is confirmed.  

5. **Performance Tuning:**  
   - Profile CPU usage. Add scaling/downsampling in the producer if needed. Tune the FPS.  
   - Experiment with hardware decode.  
   - Check behavior under load (many clients, high-res video).  

6. **Cleanup & Robustness:**  
   - Implement signal handlers to clean up shared memory if interrupted.  
   - Add timeouts or error handling (e.g. if a client finds the frame dimensions changed unexpectedly).  
   - Validate color space and stride assumptions.  

7. **(Optional) Sh4lt Integration:**  
   - Once the manual shm pipeline is stable, consider re-implementing the shared-memory logic using the Sh4lt library for learning. See below.

## Risks and Constraints  

- **Race conditions:** Mis-managing the buffer toggle could cause clients to read while the producer is writing. Careful atomic updates and memory barriers are needed. A bug here would cause visual corruption.  
- **Shared memory limits:** Extremely large frames (4K RGBA = ~33 MB) could be heavy on `/dev/shm`. Linux typically allows plenty, but this should be watched.  
- **Termination/cleanup:** If a process crashes without unlinking, leftover shm objects may persist in `/dev/shm`. Ensure unlinking on exit.  
- **CPU usage:** Busy polling at high frame rates can spike CPU. Design to accept some frame-dropping rather than constant 100% CPU spin.  
- **Kitty behavior:** Not all terminal multiplexers or situations handle Kitty’s protocols perfectly. This setup assumes a direct Kitty window. Nested terminals (tmux) might lose images unless configured (`set-option -g allow-rename off` etc.).  
- **No Windows/macOS support:** We’re explicitly on Linux. This design won’t work on Windows (Kitty’s `t=s` mode has Windows semantics for named shared memory, but the rest is Unix-focused)【42†L1098-L1104】. That’s acceptable per the spec.

## What *Not* to Overengineer Early  

- **Skip external frameworks:** Don’t start by integrating GStreamer, MPI, or other IPC libraries. The goal is to **learn** the low-level pipeline. Use plain POSIX shm and simple sync.  
- **No immediate GPU rendering:** We assume CPU decode. GPU acceleration (OpenGL, Vulkan, or even NVDEC APIs) can complicate design; add only if CPU is a bottleneck.  
- **Don’t pre-optimize for thousands of clients:** Only test with a few terminals first. If dozens of clients are needed, revisit the sync strategy (but that’s later).  
- **No network or cluster:** Keep it single-host for now. Remote streaming adds latency and a whole new layer (e.g. using network sockets or NDI), which is out of scope initially.  

## Phased Implementation Milestones  

1. **Basic Pipeline:** Raw decode→shm→single client output. Confirm frame visible in Kitty.  
2. **Multi-client:** Add a second client; ensure both show (even if at different paces).  
3. **Client Effects:** Implement at least two different filters (e.g. ASCII and grayscale) and show them running simultaneously.  
4. **Robust Sync:** Add proper semaphores or futex if needed, and test with fast-moving video to check no tearing.  
5. **Performance Test:** Run a high-res, high-FPS video with, say, 3 clients. Measure CPU, memory, latency.  
6. **Documentation and Cleanup:** Write readme of usage, ensure graceful exit cleans resources.  

## Future Improvements  

- **GPU-accelerated Decoding:** Use FFmpeg’s `-hwaccel` for H.264/HEVC so the CPU is free. The decoded frame can still be copied to CPU memory for sharing.  
- **Client-side GPU Filters:** If clients become heavy, one could use OpenGL/GLSL shaders for effects (Kitty itself uses GPU to render images). For example, a client could send a vertex/fragment shader via an API to Kitty (if supported) instead of CPU processing.  
- **Using Sh4lt Protocol:** The Sh4lt library provides a ready-made “1-to-many shared stream” framework【44†L262-L270】. In the future, our producer could write to a Sh4lt channel (frame-by-frame), and clients use Sh4lt readers. This would handle connection/disconnection and metadata for us. The downside is added dependency and abstraction. But Sh4lt’s design (with labels and timecodes) might simplify managing multiple streams【44†L262-L270】【46†L339-L347】.  
- **Higher-Level Graphics:** Instead of terminal, add an OpenGL/Wayland client to render frames in a window (for non-terminal proof-of-concept). This diverges from the Kitty focus but could be a bonus demo.  
- **Networking:** Package frames over the network (e.g. to other Linux hosts using shared memory backed by RDMA or similar). This is complex and not in initial scope.  

## Sh4lt Integration (Optional)  

If you choose to leverage **Sh4lt** instead of coding the IPC manually, note the trade-offs:

- **Advantages:** Sh4lt is a battle-tested library for exactly this use-case【44†L262-L270】. It provides C/C++ and Python APIs to write and read “streams” of frames. It handles multiple readers, timecodes, dynamic resizing, and even monitoring tools (see Shmdata’s `sdflow` example in【46†L339-L347】). It also offers GStreamer elements (`sh4ltsink`/`sh4ltsrc`) so you could push video from `gst-launch`. Using Sh4lt could save development time on IPC boilerplate.

- **Constraints:** It adds an external dependency (you must install libsh4lt and link against it). It uses its own semantics (labels, groups). You’ll have to learn its API calls. There may be slight overhead (metadata, socket signalling) compared to a minimal custom shm solution. Also, Sh4lt’s licensing or update cycle could impact your project (check its license). And since you want to learn low-level mechanisms, using Sh4lt “out of the box” may obscure some details.

If not overly constrained by learning goals, you *can* implement Sh4lt by: having the decoder create a `Sh4lt::Writer` (or use the GStreamer `sh4ltsink` element) with a given label. Each client then creates a `Sh4lt::Reader` for that label. Under the hood, Sh4lt will manage a shared memory region and synchronization for you, delivering frames. Clients would still receive raw pixel buffers (Sh4lt uses RGBA by default) and then render via Kitty as before. In other words, Sh4lt replaces our manual shm+sync layer with its library calls【44†L262-L270】【46†L339-L347】. You would still need to handle the final display step using Kitty.

In summary, Sh4lt could simplify the IPC layer, but it’s optional. A manually built POSIX-shm pipeline keeps the implementation transparent and is educational, which seems to be your priority. 

*References:* The design and use of shared-memory video streams are inspired by Shmdata/Sh4lt【46†L339-L347】【44†L262-L270】, and Kitty’s graphics protocol documentation【42†L1029-L1038】【42†L1098-L1104】. These confirm that one writer-many-reader shared memory (no extra copy) is feasible, and that Kitty can ingest raw RGBA frames from shared memory efficiently.