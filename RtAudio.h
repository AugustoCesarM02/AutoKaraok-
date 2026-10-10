#ifndef RTAUDIO_H
#define RTAUDIO_H

#include <string>
#include <vector>
#include <exception>

typedef unsigned long RtAudioFormat;
static const RtAudioFormat RTAUDIO_FLOAT32 = 0x10;

typedef unsigned int RtAudioStreamStatus;

struct RtAudioStreamParameters {
    unsigned int deviceId;
    unsigned int nChannels;
    unsigned int firstChannel;
    RtAudioStreamParameters() : deviceId(0), nChannels(0), firstChannel(0) {}
};

class RtAudioError : public std::exception {
public:
    RtAudioError(const std::string& message) : message_(message) {}
    virtual ~RtAudioError() throw() {}
    virtual const char* what() const throw() { return message_.c_str(); }
    std::string getMessage() const { return message_; }
private:
    std::string message_;
};

typedef int (*RtAudioCallback)(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
                               double streamTime, RtAudioStreamStatus status, void *userData);

class RtAudio {
public:
    enum Api { UNSPECIFIED, WINDOWS_WASAPI, WINDOWS_DS, LINUX_ALSA, MACOSX_CORE };
    RtAudio(RtAudio::Api api=UNSPECIFIED);
    ~RtAudio();
    unsigned int getDeviceCount(void);
    unsigned int getDefaultOutputDevice(void);
    unsigned int getDefaultInputDevice(void);
    void openStream(RtAudioStreamParameters *outputParameters, RtAudioStreamParameters *inputParameters,
                    RtAudioFormat format, unsigned int sampleRate, unsigned int *bufferFrames,
                    RtAudioCallback callback, void *userData=NULL, void *options=NULL);
    void closeStream(void);
    void startStream(void);
    void stopStream(void);
    bool isStreamOpen(void);
};

#endif
