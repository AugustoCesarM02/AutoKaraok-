#include <iostream>
#include <vector>
#include <cmath>
#include <windows.h>
#include <mmsystem.h>
#include "RtAudio.h"

const unsigned int SAMPLE_RATE = 44100;
unsigned int bufferFrames = 512;

const double NOTAS_PERMITIDAS[] = { 261.63, 293.66, 329.63, 349.23, 392.00, 440.00, 493.88 };
const int QTD_NOTAS = 7;

double descobrirPitch(const float* buffer, unsigned int tamanho, unsigned int sampleRate) {
    double maiorSoma = 0;
    int melhorLag = -1;
    int maxLag = sampleRate / 80;
    int minLag = sampleRate / 500;

    for (int lag = minLag; lag < maxLag; lag++) {
        double soma = 0;
        for (unsigned int i = 0; i < tamanho - lag; i++) {
            soma += buffer[i] * buffer[i + lag];
        }
        if (soma > maiorSoma) {
            maiorSoma = soma;
            melhorLag = lag;
        }
    }
    return (melhorLag > 0) ? (double)sampleRate / melhorLag : 0.0;
}

double encontrarNotaProxima(double freqAtual) {
    if (freqAtual < 80.0 || freqAtual > 600.0) return 0.0;
    double menorDistancia = std::abs(NOTAS_PERMITIDAS[0] - freqAtual);
    double notaProxima = NOTAS_PERMITIDAS[0];
    for (int i = 1; i < QTD_NOTAS; i++) {
        double distancia = std::abs(NOTAS_PERMITIDAS[i] - freqAtual);
        if (distancia < menorDistancia) {
            menorDistancia = distancia;
            notaProxima = NOTAS_PERMITIDAS[i];
        }
    }
    return notaProxima;
}

int audioCallback(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
                 double streamTime, RtAudioStreamStatus status, void *userData) {
    float *in = (float *)inputBuffer;
    float *out = (float *)outputBuffer;
    if (!in || !out) return 0;

    double freqCantada = descobrirPitch(in, nBufferFrames, SAMPLE_RATE);
    double notaAlvo = encontrarNotaProxima(freqCantada);
    double ratio = (freqCantada > 0 && notaAlvo > 0) ? notaAlvo / freqCantada : 1.0;

    double ponteiroVirtual = 0.0;
    for (unsigned int i = 0; i < nBufferFrames; i++) {
        unsigned int idxBase = (unsigned int)ponteiroVirtual;
        double fracao = ponteiroVirtual - idxBase;
        if (idxBase + 1 < nBufferFrames) {
            out[i] = (1.0 - fracao) * in[idxBase] + fracao * in[idxBase + 1];
        } else {
            out[i] = in[nBufferFrames - 1];
        }
        ponteiroVirtual += ratio;
        if (ponteiroVirtual >= nBufferFrames) ponteiroVirtual = nBufferFrames - 1;
    }
    return 0;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "🎵 ANALISADOR DE HARDWARE DE AUDIO DO WINDOWS 🎵" << std::endl;
    std::cout << "==================================================" << std::endl;

    // Teste de hardware direto na main para impedir o crash silencioso
    WAVEFORMATEX wfx;
    wfx.wFormatTag = 1; // WAVE_FORMAT_PCM
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = 44100;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 2;
    wfx.nAvgBytesPerSec = 44100 * 2;
    wfx.cbSize = 0;

    HWAVEIN testIn;
    MMRESULT resIn = waveInOpen(&testIn, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    if (resIn != MMSYSERR_NOERROR) {
        std::string erroMsg = "O Windows bloqueou o Microfone.\nCodigo de Erro: " + std::to_string(resIn) + 
                              "\n\nIsso significa que o Windows recusou criar a sessao de gravacao.";
        MessageBoxA(NULL, erroMsg.c_str(), "Erro de Hardware (Microfone)", MB_ICONERROR | MB_OK);
        return 1;
    }
    waveInClose(testIn);

    HWAVEOUT testOut;
    MMRESULT resOut = waveOutOpen(&testOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    if (resOut != MMSYSERR_NOERROR) {
        std::string erroMsg = "O Windows recusou o Fone de Ouvido.\nCodigo de Erro: " + std::to_string(resOut);
        MessageBoxA(NULL, erroMsg.c_str(), "Erro de Hardware (Fone)", MB_ICONERROR | MB_OK);
        return 1;
    }
    waveOutClose(testOut);

    // Se passou pelos testes, inicia a estrutura do programa
    RtAudio dac;
    RtAudioStreamParameters oParams, iParams;
    oParams.deviceId = dac.getDefaultOutputDevice();
    oParams.nChannels = 1;
    iParams.deviceId = dac.getDefaultInputDevice();
    iParams.nChannels = 1;

    try {
        std::cout << "✅ Hardware verificado! Inicializando o fluxo..." << std::endl;
        dac.openStream(&oParams, &iParams, RTAUDIO_FLOAT32, SAMPLE_RATE, &bufferFrames, &audioCallback);
        dac.startStream();
        
        std::cout << "\n🎤 AUTO-TUNE EM C++ EM EXECUCAO! 🎤" << std::endl;
        std::cout << "Pressione ENTER no seu teclado para fechar..." << std::endl;
        std::cin.get();

        dac.stopStream();
        if (dac.isStreamOpen()) dac.closeStream();
    }
    catch (RtAudioError& e) {
        std::cerr << "❌ Erro no RtAudio: " << e.getMessage() << std::endl;
        std::cin.get();
        return 1;
    }

    return 0;
}
