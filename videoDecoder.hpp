#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include "shm.hpp"

// Video header struct
struct VideoHeader {
    int width =0;
    int height;
    int image_size;
};

// A struct to hold the exported raw pixel planes
struct VideoFrameData {
    int width = 0;
    int height = 0;
    int frame_index = 0;
    double timestamp_seconds = 0.0;
    
    // Separate vectors for Y, U, and V memory planes
    std::vector<uint8_t> y_plane;
    std::vector<uint8_t> u_plane;
    std::vector<uint8_t> v_plane;
};

// Main data struct for producer-
class VideoDecoder {
private:
    AVFormatContext* format_ctx = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    AVPacket* packet = nullptr;
    AVFrame* frame = nullptr;
    int numberFrame = 0;
    int video_stream_idx = -1;
    int frame_counter = 0;
    int frameRate;

    uint8_t* shm = nullptr;
    uint8_t RGBAData{};

    // Helper function to safely copy raw data out of FFmpeg's padded linesizes
    void copy_plane(const uint8_t* src, int src_linesize, uint8_t* dest, int width, int height) {
        for (int i = 0; i < height; ++i) {
            std::copy(src + (i * src_linesize), src + (i * src_linesize) + width, dest + (i * width));
        }
    }

    void cleanupSHM() {
        if (shm && codec_ctx) {
            int image_size = codec_ctx->width * codec_ctx->height * 4;
            exitSHM(shm, image_size);
            shm = nullptr;
        }
    }

public:

    VideoDecoder() = default;
    
    // Automatically close resources if the object falls out of scope
    ~VideoDecoder() { 
        close(); 
    }

    // Function to convert YUV420P to RGBA
    int converterNsendSHM(uint64_t frame_counter) {
        int width = codec_ctx->width;
        int height = codec_ctx->height;
        // int image_size = width * height * 4;
        uint8_t* dstData[4] = { nullptr };
        int dstLinesize[4] = { 0 };

        // Allocate local temporary buffer
        int data_length = av_image_alloc(dstData, dstLinesize, width, height, AV_PIX_FMT_RGBA, 1);

        std::cout << "Frame Width: " << width << ", Height: " << height << std::endl;

        if (data_length < 1) std::cerr << "Failed to allocate memory" << std::endl;
        
        // Set up context 
        SwsContext* dataContext = sws_getContext(
            width, height, (AVPixelFormat)frame->format, // Source details
            width, height, AV_PIX_FMT_RGBA,            // Target details
            SWS_BILINEAR, nullptr, nullptr, nullptr                  // Filters (Not used for color space swapping)
        );        
        
        if (!dataContext) {
            std::cerr << "Failed to create SwsContext" << std::endl;
            av_freep(&dstData[0]);
            return -1;
        };

        // Perform scaling conversion
        sws_scale(dataContext, frame->data, frame->linesize, 0, frame->height, dstData, dstLinesize);
        
        if (!shm) {
            std::cerr << "SHM pointer is null!" << std::endl;
            av_freep(&dstData[0]);
            sws_freeContext(dataContext);
            return -1;
        }
        
        // Put data inside the SHM
        // putSHM(shm, dstData[0], image_size); // Deprecated, replaced with writeFrameToSlot for ring buffer functionality
        writeFrameToSlot(shm, frame_counter % 8, dstData[0], frame_counter);

        //Test SHM
        // testSHM(data_length, width, height, "vp_static");

        // Cleanup
        av_freep(&dstData[0]);
        sws_freeContext(dataContext);

        return 0;
    } 

    // Opens file and allocates processing memory
    // kinda borked
    bool open(const std::string& filename) {
        if (avformat_open_input(&format_ctx, filename.c_str(), nullptr, nullptr) < 0) return false;
        if (avformat_find_stream_info(format_ctx, nullptr) < 0) return false;

        // Codec
        const AVCodec* codec = nullptr;
        for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
            if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                video_stream_idx = i;
                codec = avcodec_find_decoder(format_ctx->streams[i]->codecpar->codec_id);
                break;
            }
        }

        if (video_stream_idx == -1 || !codec) return false;

        codec_ctx = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(codec_ctx, format_ctx->streams[video_stream_idx]->codecpar);
        if (avcodec_open2(codec_ctx, codec, nullptr) < 0) return false;

        packet = av_packet_alloc();
        frame = av_frame_alloc();
        frame_counter = 0;
        numberFrame = format_ctx->streams[video_stream_idx]->nb_frames;

        // Setup SHM with header for metadata
        // imagesize deprecated, calculating size is fully implemented on createSHM function directly
        int width = codec_ctx->width;
        int height = codec_ctx->height;
        // int image_size = width * height * 4; // RGBA size'
        av_q2d(format_ctx->streams[video_stream_idx]->avg_frame_rate);


        // Setup producer SH<%
        shm = createSHM(width, height, "/vp_static", true);  // true = with header
        if (!shm) {
            std::cerr << "Failed to allocate initialization SHM" << std::endl;
            close(); // Clean up allocated FFmpeg resources
            return false;
        }
            return true;
    }

    // Pulls the next available frame out of the pipeline
    bool read_next_frame(VideoFrameData* out_data) {
        if (!format_ctx || !codec_ctx || !out_data) return false;

        while (av_read_frame(format_ctx, packet) >= 0) {
            if (packet->stream_index == video_stream_idx) {
                int response = avcodec_send_packet(codec_ctx, packet);
                
                while (response >= 0) {
                    response = avcodec_receive_frame(codec_ctx, frame);
                    if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
                        break; // Need more packets
                    } else if (response < 0) {
                        av_packet_unref(packet);
                        return false; // Error decoding
                    }

                    frame_counter++;
                    out_data->width = frame->width;
                    out_data->height = frame->height;
                    out_data->frame_index = frame_counter;
                    
                    AVRational time_base = format_ctx->streams[video_stream_idx]->time_base;
                    out_data->timestamp_seconds = frame->pts * av_q2d(time_base);

                    // Allocate memory tracking YUV420p dimensions
                    int uv_width = frame->width / 2;
                    int uv_height = frame->height / 2;
                    out_data->y_plane.resize(frame->width * frame->height);
                    out_data->u_plane.resize(uv_width * uv_height);
                    out_data->v_plane.resize(uv_width * uv_height);

                    
                    
                    // Strip padding out of linesize while transferring data to pointers
                    copy_plane(frame->data[0], frame->linesize[0], out_data->y_plane.data(), frame->width, frame->height);
                    copy_plane(frame->data[1], frame->linesize[1], out_data->u_plane.data(), uv_width, uv_height);
                    copy_plane(frame->data[2], frame->linesize[2], out_data->v_plane.data(), uv_width, uv_height);
                    
                    // Convert 
                    converterNsendSHM(frame_counter);
 
                    // Cleanup loop instances
                    av_frame_unref(frame);
                    av_packet_unref(packet);

                    return true;
                }
            }
            av_packet_unref(packet);
        }
        return false; // Reached end of file
    }

    // Return the number of frame there is in one video
    int numberofFrame() const {
        return this->numberFrame;
    };

    void close() {
        cleanupSHM(); // Always clean SHM first

        if (frame) { 
            av_frame_free(&frame); 
            frame = nullptr; 
        }
        
        if (packet) {
            av_packet_free(&packet); 
            packet = nullptr; 
        }
        
        if (codec_ctx) { 
            avcodec_free_context(&codec_ctx); 
            codec_ctx = nullptr; 
        }
        
        if (format_ctx) { 
            avformat_close_input(&format_ctx); 
            format_ctx = nullptr; 
        }
    }
};

int decodeVideo(VideoDecoder& decoder, VideoFrameData& frame);
int decodeVideo(VideoDecoder& decoder, VideoFrameData& frame, int desired_frame);