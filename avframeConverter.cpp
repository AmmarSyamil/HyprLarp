#include <iostream>

// FFmpeg is a C library, so wrap includes in extern "C"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

int main() {
    const char* filename = "input.mp4";

    // =========================================================================
    // PART 1: Initialization & Opening the File
    // =========================================================================
    
    // Allocate the format context (holds container metadata)
    AVFormatContext* format_ctx = nullptr;
    
    // Open the video file and read the header information
    if (avformat_open_input(&format_ctx, filename, nullptr, nullptr) < 0) {
        std::cerr << "Error: Could not open the file." << std::endl;
        return -1;
    }

    // Read stream packets to look for stream info (codec type, frame rate, etc.)
    if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
        std::cerr << "Error: Could not find stream information." << std::endl;
        avformat_close_input(&format_ctx);
        return -1;
    }

    // =========================================================================
    // PART 2: Finding the Video Stream & Setting up Decoder
    // =========================================================================
    
    int video_stream_idx = -1;
    const AVCodec* codec = nullptr;

    // Loop through all streams (audio, video, subtitles) to find the video stream
    for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_idx = i;
            
            // Find the correct registered decoder for this specific codec ID (e.g., H.264)
            codec = avcodec_find_decoder(format_ctx->streams[i]->codecpar->codec_id);
            break;
        }
    }

    if (video_stream_idx == -1 || !codec) {
        std::cerr << "Error: Could not find a valid video stream/decoder." << std::endl;
        avformat_close_input(&format_ctx);
        return -1;
    }

    // Allocate a context for our decoder
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    
    // Copy the configuration parameters from the file stream to the decoder context
    avcodec_parameters_to_context(codec_ctx, format_ctx->streams[video_stream_idx]->codecpar);

    // Open the decoder
    if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
        std::cerr << "Error: Could not open the codec." << std::endl;
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        return -1;
    }

    // =========================================================================
    // PART 3: The Packet to Frame Loop
    // =========================================================================
    
    // Allocate memory for data structures
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    // Read demuxed compressed packets from the container file
    while (av_read_frame(format_ctx, packet) >= 0) {
        
        // Ensure this packet actually belongs to our video stream (and not audio)
        if (packet->stream_index == video_stream_idx) {
            
            // Push the compressed packet into the decoder
            int response = avcodec_send_packet(codec_ctx, packet);
            
            while (response >= 0) {
                // Pull the uncompressed raw image frame out of the decoder
                response = avcodec_receive_frame(codec_ctx, frame);
                if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
                    break; // Decoder needs more packets or we hit the end of the file
                } else if (response < 0) {
                    std::cerr << "Error during decoding." << std::endl;
                    break;
                }

                // SUCCESS: You now have the raw pixels!
                std::cout << "Decoded Frame successfully! Dimensions: " 
                          << frame->width << "x" << frame->height << std::endl;

                // Clear the frame's internal buffers so it can be safely reused next loop
                av_frame_unref(frame);
            }
        }
        // Clear the packet's internal buffers so it can be safely reused next loop
        av_packet_unref(packet);
    }

    // =========================================================================
    // PART 4: Clean up Memory (Crucial in C++ to prevent leaks)
    // =========================================================================
    av_frame_free(&frame);
    av_packet_free(&packet);
    av_codec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);

    std::cout << "Finished decoding successfully." << std::endl;
    return 0;
}