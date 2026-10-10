#include "RtAudio.h"
#include <windows.h>
#include <mmsystem.h>
#include <vector>
#include <iostream>

#define NUM_BUFFERS 2
#define BUFFER_SIZE 512

// Definimos o formato padrão de 16 bits inteiro (PCM) do Windows
#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_PCM 1
#endif

struct StreamInfo {
    RtAudioCallback callback;
    void* userData;
    bool running;
    HANDLE thread;
    HWAVEIN hWaveIn;
    HWAVEOUT hWaveOut;
    WAVEHDR waveInHdr[NUM_BUFFERS];
    WAVEHDR waveOutHdr[NUM_BUFFERS];
    std::vector<short> inBuffers[NUM_BUFFERS];  // Usamos short para 16 bits
    std::vector<short> outBuffers[NUM_BUFFERS]; // Usamos short para 16 bits
};

static StreamInfo g_stream;

static DWORD WINAPI audioThread(LPVOID lpParam) {
    StreamInfo* info = (StreamInfo*)lpParam;
    
    WAVEFORMATEX wfx;
    wfx.wFormatTag = WAVE_FORMAT_PCM; // Força o formato de 16 bits que ativamos no Windows
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = 44100;
    wfx.wBitsPerSample = 16;           // Alinhado com a qualidade de CD de 16 bits
    wfx.nBlockAlign = 2;              // 1 canal * 2 bytes por amostra
    wfx.nAvgBytesPerSec = 44100 * 2;
    wfx.cbSize = 0;

    // Tenta abrir o Microfone
    MMRESULT resIn = waveInOpen(&info->hWaveIn, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    if (resIn != MMSYSERR_NOERROR) {
        std::cout << "\n❌ Erro Crítico: O Windows recusou abrir o seu Microfone! Codigo: " << resIn << std::endl;
        info->running = false;
        return 0;
    }

    // Tenta abrir o Fone de Ouvido
    MMRESULT resOut = waveOutOpen(&info->hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    if (resOut != MMSYSERR_NOERROR) {
        std::cout << "\n❌ Erro Crítico: O Windows recusou abrir o seu Fone de Ouvido! Codigo: " << resOut << std::endl;
        waveInClose(info->hWaveIn);
        info->running = false;
        return 0;
    }

    // Prepara os buffers temporários de conversão
    std::vector<float> floatIn(BUFFER_SIZE, 0.0f);
    std::vector<float> floatOut(BUFFER_SIZE, 0.0f);

    for (int i = 0; i < NUM_BUFFERS; i++) {
        info->inBuffers[i].assign(BUFFER_SIZE, 0);
        info->outBuffers[i].assign(BUFFER_SIZE, 0);

        info->waveInHdr[i].lpData = (LPSTR)info->inBuffers[i].data();
        info->waveInHdr[i].dwBufferLength = BUFFER_SIZE * 2;
        info->waveInHdr[i].dwFlags = 0;
        waveInPrepareHeader(info->hWaveIn, &info->waveInHdr[i], sizeof(WAVEHDR));
        waveInAddBuffer(info->hWaveIn, &info->waveInHdr[i], sizeof(WAVEHDR));

        info->waveOutHdr[i].lpData = (LPSTR)info->outBuffers[i].data();
        info->waveOutHdr[i].dwBufferLength = BUFFER_SIZE * 2;
        info->waveOutHdr[i].dwFlags = 0;
        waveOutPrepareHeader(info->hWaveOut, &info->waveOutHdr[i], sizeof(WAVEHDR));
    }

    waveInStart(info->hWaveIn);
    int bufferIdx = 0;

    while (info->running) {
        while (!(info->waveInHdr[bufferIdx].dwFlags & WHDR_DONE) && info->running) {
            Sleep(1);
        }
        if (!info->running) break;

        info->waveInHdr[bufferIdx].dwFlags &= ~WHDR_DONE;

        // Converte os dados do microfone (short de 16 bits) para float (decimal) para rodar o Auto-Tune
        for (int s = 0; s < BUFFER_SIZE; s++) {
            floatIn[s] = (float)info->inBuffers[bufferIdx][s] / 32768.0f;
        }

        // Executa a matemática do Auto-Tune do seu main.cpp
        info->callback(floatOut.data(), floatIn.data(), BUFFER_SIZE, 0.0, 0, info->userData);

        // Converte o som afinado (float) de volta para short de 16 bits para tocar no seu fone
        for (int s = 0; s < BUFFER_SIZE; s++) {
            float v = floatOut[s] * 32768.0f;
            if (v > 32767.0f) v = 32767.0f;
            if (v < -32768.0f) v = -32768.0f;
            info->outBuffers[bufferIdx][s] = (short)v;
        }

        waveOutWrite(info->hWaveOut, &info->waveOutHdr[bufferIdx], sizeof(WAVEHDR));
        waveInAddBuffer(info->hWaveIn, &info->waveInHdr[bufferIdx], sizeof(WAVEHDR));

        bufferIdx = (bufferIdx + 1) % NUM_BUFFERS;
    }

    waveInStop(info->hWaveIn);
    for (int i = 0; i < NUM_BUFFERS; i++) {
        waveInUnprepareHeader(info->hWaveIn, &info->waveInHdr[i], sizeof(WAVEHDR));
        waveOutUnprepareHeader(info->hWaveOut, &info->waveOutHdr[i], sizeof(WAVEHDR));
    }
    waveInClose(info->hWaveIn);
    waveOutClose(info->hWaveOut);
    return 0;
}

RtAudio::RtAudio(RtAudio::Api api) {}
RtAudio::~RtAudio() { closeStream(); }
unsigned int RtAudio::getDeviceCount(void) { return 1; }
unsigned int RtAudio::getDefaultOutputDevice(void) { return 0; }
unsigned int RtAudio::getDefaultInputDevice(void) { return 0; }
bool RtAudio::isStreamOpen(void) { return g_stream.running; }

void RtAudio::openStream(RtAudioStreamParameters *outputParameters, RtAudioStreamParameters *inputParameters,
                        RtAudioFormat format, unsigned int sampleRate, unsigned int *bufferFrames,
                        RtAudioCallback callback, void *userData, void *options) {
    g_stream.callback = callback;
    g_stream.userData = userData;
    g_stream.running = false;
}

void RtAudio::startStream(void) {
    if (!g_stream.running) {
        g_stream.running = true;
        g_stream.thread = CreateThread(NULL, 0, audioThread, &g_stream, 0, NULL);
        SetThreadPriority(g_stream.thread, THREAD_PRIORITY_TIME_CRITICAL);
    }
}

void RtAudio::stopStream(void) {
    if (g_stream.running) {
        g_stream.running = false;
        WaitForSingleObject(g_stream.thread, INFINITE);
        CloseHandle(g_stream.thread);
    }
}

void RtAudio::closeStream(void) { stopStream(); }
