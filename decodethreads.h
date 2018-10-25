#ifndef DECODETHREADS_H
#define DECODETHREADS_H

#pragma once
#include "ffmpeg.h"
#include "BlockingQueue.h"

#include <iostream>
#include <stdio.h>
#include <thread>

class DecodeThreads
{
public:
    DecodeThreads(char *url, size_t queue_max_size = 0);
    bool m_quit = false;

    void decode_video(FFmpeg* &ffmpeg, BlockingQueue<AVPacket *> &queue);
    void decode_audio(FFmpeg* &ffmpeg, BlockingQueue<AVPacket *> &queue);

private:
    FFmpeg *m_ffmpeg;
};

#endif // DECODETHREADS_H
