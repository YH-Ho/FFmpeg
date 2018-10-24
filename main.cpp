#include "ffmpeg.h"
#include <iostream>
#include <stdio.h>
#include <thread>

#include "blockingqueue.h"

using namespace std;

static bool g_quit = false;

void *decode_video(FFmpeg* &ffmpeg, BlockingQueue<AVPacket *> &queue){
    AVPacket *pkt;
    while(!g_quit){
        queue.wait_and_pop(pkt);
        ffmpeg->decode_video_frame(pkt);
        cout<<"test"<<endl;
    }
    return nullptr;
}

void *decode_audio(FFmpeg* &ffmpeg, BlockingQueue<AVPacket *> &queue){
    AVPacket *pkt;
    while(!g_quit){
        queue.wait_and_pop(pkt);
        ffmpeg->decode_audio_frame(pkt);
    }
    return nullptr;
}

int main(int argc, char *argv[]){
    BlockingQueue<AVPacket *> videoqueue(1000);
    BlockingQueue<AVPacket *> audioqueue(1000);
    char url[] = "F:/test.mp4";
    FFmpeg *ffmpeg_test = new FFmpeg;
    ffmpeg_test->init();
    ffmpeg_test->open_input_source(url);

    thread t1(&FFmpeg::data_subcontracting,ffmpeg_test,ref(videoqueue),ref(audioqueue));
    thread thread_video_decode(decode_video, ref(ffmpeg_test), ref(videoqueue));
    thread thread_audio_decode(decode_audio, ref(ffmpeg_test), ref(audioqueue));

    t1.join();
    thread_video_decode.join();
    thread_audio_decode.join();
    return 0;
}

