# Kitty Escape Sequence Fix Guide

## What Was Wrong

The Kitty graphics protocol doesn't reliably support the `O` (offset) parameter for shared memory (`t=s`). Your code was trying to skip a VideoHeader in the SHM using `O=videoHeaderSize`, but Kitty was likely ignoring it and reading from offset 0 anyway, getting corrupted data.

## What Was Fixed

1. **SHM Structure Separation**
   - Main SHM (`/vp_static`): Contains VideoHeader + pixel data (for internal metadata)
   - Display SHMs (`/HyprLarp:PID:FRAME`): Contains ONLY pixel data (for Kitty)

2. **Escape Sequence Simplification**
   - Removed `O=videoHeaderSize` parameter
   - Now only uses `S=total_pixel_size`
   - SHM is read from offset 0 as expected

3. **Code Changes**
   - `createSHM()` now has optional `create_header` parameter
   - Display SHMs created with `createSHM(..., false)`
   - Escape sequence follows official Kitty protocol exactly

## Testing Steps

### 1. Build the project
```bash
cd /home/sp/code/hyprlarp
mkdir -p build && cd build && cmake .. && make
```

### 2. Run with debug output
```bash
cd /home/sp/code/hyprlarp
./main  # Or whatever your executable is
```

### 3. Expected Output in Kitty Terminal
- Image should display without errors
- You should see hex dump of escape sequence in stderr
- No "ENOENT" or "EINVAL" responses from Kitty

### 4. If Still Not Working

Check these items in order:
1. **Are you running in Kitty terminal?**
   - Test: `echo $TERM` should show `xterm-kitty`
   
2. **Is Kitty graphics protocol enabled?**
   - Test: Run `echo -e '\x1b_Gi=1,s=1,v=1,a=q,t=d,f=24;AAAA\x1b\\\x1b[c'`
   - If Kitty responds with graphics query answer, protocol is working
   
3. **Check SHM file permissions**
   - Test: `ls -la /dev/shm/` and verify your SHM file exists and is readable
   
4. **Verify pixel data quality**
   - Test: Check `test_frame.ppm` output to see if pixel data is correct
   - May need: `file test_frame.ppm` and `hexdump -C test_frame.ppm | head`

## Key Files Modified

- `shm.hpp` - Added `create_header` parameter
- `shm.cpp` - Conditional header storage and fixed access patterns
- `renderer.cpp` - Removed offset parameter from escape sequence
- `consumer.hpp` - Pass `false` for display SHM creation

## Kitty Protocol Reference

The escape sequence now matches the official protocol:
```
<ESC>_Ga=T,f=32,s=WIDTH,v=HEIGHT,t=s,S=SIZE;BASE64_SHMNAME<ESC>\
```

Where:
- `f=32` = RGBA 32-bit format
- `s=WIDTH` = image width in pixels
- `v=HEIGHT` = image height in pixels
- `t=s` = transmission medium is shared memory
- `S=SIZE` = total byte size to read (WIDTH × HEIGHT × 4)
- `BASE64_SHMNAME` = base64-encoded SHM name (without leading `/`)

## No More Offset Parameter!

The critical fix: **REMOVED** `O=offset` parameter. Kitty ignores this for shared memory anyway.
