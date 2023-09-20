#include <iostream>
#include <SDL.h>
#include <SDL_thread.h>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

const char* VIDEO_FILE_PATH = "your_video.mp4";

// SDL callback to fill the audio buffer
void AudioCallback(void* userdata, Uint8* stream, int len) {
    // You can implement audio playback here if needed
    // This example does not include audio playback
    SDL_memset(stream, 0, len);
}

int main() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    // Initialize FFmpeg
    av_register_all();

    // Open the video file
    AVFormatContext* formatContext = nullptr;
    if (avformat_open_input(&formatContext, VIDEO_FILE_PATH, nullptr, nullptr) != 0) {
        std::cerr << "Could not open video file" << std::endl;
        return -1;
    }

    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        std::cerr << "Could not find stream information" << std::endl;
        return -1;
    }

    // Find the video stream
    int videoStreamIndex = -1;
    for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            break;
        }
    }

    if (videoStreamIndex == -1) {
        std::cerr << "Could not find a video stream in the file" << std::endl;
        return -1;
    }

    // Get video codec
    AVCodecParameters* codecParameters = formatContext->streams[videoStreamIndex]->codecpar;
    AVCodec* codec = avcodec_find_decoder(codecParameters->codec_id);
    if (!codec) {
        std::cerr << "Codec not found" << std::endl;
        return -1;
    }

    AVCodecContext* codecContext = avcodec_alloc_context3(codec);
    if (avcodec_parameters_to_context(codecContext, codecParameters) < 0) {
        std::cerr << "Failed to copy codec parameters to codec context" << std::endl;
        return -1;
    }

    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        std::cerr << "Could not open codec" << std::endl;
        return -1;
    }

    // Create SDL window
    SDL_Window* window = SDL_CreateWindow("Video Player", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, codecContext->width, codecContext->height, SDL_WINDOW_OPENGL);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, codecContext->width, codecContext->height);

    // Initialize audio subsystem (if needed)
    SDL_AudioSpec wanted_spec, obtained_spec;
    wanted_spec.freq = codecContext->sample_rate;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = codecContext->channels;
    wanted_spec.silence = 0;
    wanted_spec.samples = 1024;
    wanted_spec.callback = AudioCallback;

    if (SDL_OpenAudio(&wanted_spec, &obtained_spec) < 0) {
        std::cerr << "SDL audio initialization failed" << std::endl;
    }

    // Start playing video
    AVPacket packet;
    av_init_packet(&packet);

    while (av_read_frame(formatContext, &packet) >= 0) {
        if (packet.stream_index == videoStreamIndex) {
            if (avcodec_send_packet(codecContext, &packet) < 0) {
                break;
            }

            AVFrame* frame = av_frame_alloc();
            if (avcodec_receive_frame(codecContext, frame) < 0) {
                av_frame_free(&frame);
                break;
            }

            // Update the SDL texture with the new frame
            SDL_UpdateTexture(texture, nullptr, frame->data[0], frame->linesize[0]);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);

            av_frame_free(&frame);
        }

        av_packet_unref(&packet);
    }

    // Clean up resources
    avformat_close_input(&formatContext);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}