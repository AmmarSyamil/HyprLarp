// videoDecoder.hpp
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
    bool flushing = false;
    SwsContext* sws_ctx = nullptr;  

    uint8_t* shm = nullptr;

    // Helper function to safely copy raw data out of FFmpeg's padded linesizes
    void copy_plane(const uint8_t* src, int src_linesize, uint8_t* dest, int width, int height) {
        for (int i = 0; i < height; ++i) {
            std::copy(src + (i * src_linesize), src + (i * src_linesize) + width, dest + (i * width));
        }
    }

    void cleanupSHM() {
        if (shm && codec_ctx) {
            // size_t stride = (codec_ctx->width * 4 + 63) & ~63ULL;
            size_t stride = codec_ctx->width * 4;
            size_t header_size = (sizeof(controlHeader) + 4095) & ~4095ULL;
            size_t image_size = stride * codec_ctx->height;
            size_t total_mapping_size = header_size + image_size * RING_BUFFER_SLOTS;
            exitSHM(shm, total_mapping_size);
            shm = nullptr;
        }
        if (sws_ctx) {
            sws_freeContext(sws_ctx);
            sws_ctx = nullptr;
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
        uint8_t* dstData[4] = { nullptr };
        int dstLinesize[4] = { 0 };

        // Allocate local temporary buffer
        int data_length = av_image_alloc(dstData, dstLinesize, width, height, AV_PIX_FMT_RGBA, 1);
        if (data_length < 1) {
            std::cerr << "Failed to allocate memory" << std::endl;
            return -1;
        }
        
        if (!sws_ctx) {
            std::cerr << "SWS context not initialized!" << std::endl;
            av_freep(&dstData[0]);
            return -1;
        }

        // Perform scaling conversion using cached context
        sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height, dstData, dstLinesize);
        // sws_scale(sws_ctx, frame->data, frame->linesize, 0, codec_ctx->height, dstData, dstLinesize);

        if (!shm) {
            std::cerr << "SHM pointer is null!" << std::endl;
            av_freep(&dstData[0]);
            return -1;
        }
        
        // Put data inside the SHM
        int slot = frame_counter % RING_BUFFER_SLOTS;
        // writeFrameToSlot(shm, slot, dstData[0], frame_counter);
        writeFrameToSlotStrided(shm, slot, dstData[0], dstLinesize[0], frame_counter);

        // Cleanup
        av_freep(&dstData[0]);

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

        int width = codec_ctx->width;
        int height = codec_ctx->height;
        av_q2d(format_ctx->streams[video_stream_idx]->avg_frame_rate);

        // Create and cache SwsContext for all frames (reused in converterNsendSHM)
        sws_ctx = sws_getContext(
            width, height, (AVPixelFormat)codec_ctx->pix_fmt,
            width, height, AV_PIX_FMT_RGBA,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
        if (!sws_ctx) {
            std::cerr << "Failed to create cached SwsContext" << std::endl;
            close();
            return false;
        }

        // Setup producer SHM with header for metadata
        shm = createSHM(width, height, "/Hyprlarp-Producer", true);  // true = with header
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

        while (true) {
            int response = 0;
            
            if (!flushing) {
                // Try to read next packet
                if (av_read_frame(format_ctx, packet) >= 0) {
                    if (packet->stream_index == video_stream_idx) {
                        response = avcodec_send_packet(codec_ctx, packet);
                    }
                    av_packet_unref(packet);
                } else {
                    // EOF reached, start flushing buffered frames
                    flushing = true;
                    avcodec_send_packet(codec_ctx, nullptr);  // Send NULL to flush buffer
                }
            }
            
            // Try to receive frame (works both during decoding and flushing)
            response = avcodec_receive_frame(codec_ctx, frame);
            
            if (response == AVERROR_EOF) {
                flushing = false;
                return false;  // No more frames
            } else if (response == AVERROR(EAGAIN)) {
                continue;
            } else if (response < 0) {
                return false;  // Error decoding
            }

            frame_counter++;
            out_data->width = frame->width;
            out_data->height = frame->height;
            out_data->frame_index = frame_counter;
            
            AVRational time_base = format_ctx->streams[video_stream_idx]->time_base;
            out_data->timestamp_seconds = frame->pts * av_q2d(time_base);

            // Convert to RGBA and write to SHM
            converterNsendSHM(frame_counter);
            
            av_frame_unref(frame);
            return true;
        }
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

    bool rewind() {
        if (!format_ctx || video_stream_idx < 0 || !codec_ctx) return false;

        avcodec_flush_buffers(codec_ctx);
        av_frame_unref(frame);
        av_packet_unref(packet);

        int64_t timestamp = 0;
        // int ret = av_seek_frame(format_ctx, video_stream_idx, timestamp, AVSEEK_FLAG_BACKWARD);
        int ret = avformat_seek_file(format_ctx, video_stream_idx, 0, 0, 0, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(codec_ctx);
        if (ret < 0) {
            std::cerr << "VideoDecoder::rewind: failed to seek to start" << std::endl;
            return false;
        }

        flushing = false;
        return true;
    }
};

int decodeVideo(VideoDecoder& decoder, VideoFrameData& frame);
int decodeVideo(VideoDecoder& decoder, VideoFrameData& frame, int desired_frame);