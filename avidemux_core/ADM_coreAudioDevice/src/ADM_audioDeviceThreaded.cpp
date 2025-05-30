/***************************************************************************
    \file ADM_audioDeviceThreaded.cpp
    \brief Base class for audio playback with a dedicated thread

    copyright            : (C) 2008 by mean
    email                : fixounet@free.fr
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "ADM_audioDeviceInternal.h"
#include "ADM_audiodevice.h"
#include "ADM_default.h"
#include "math.h"

const char *i2state(int a)
{
    switch (a)
    {
    case AUDIO_DEVICE_STARTED:
        return "DeviceStarted";
        break;
    case AUDIO_DEVICE_STOPPED:
        return "DeviceStopped";
        break;
    case AUDIO_DEVICE_STOP_REQ:
        return "DeviceStop_Requested";
        break;
    case AUDIO_DEVICE_STOP_GR:
        return "DeviceStop_Granted";
        break;
    default:
        return "????";
        break;
    }
}
#define CHANGE_STATE(x)                                                                                                \
    {                                                                                                                  \
        ADM_info("%s -> %s\n", i2state(stopRequest), i2state(x));                                                      \
        stopRequest = x;                                                                                               \
    }
/**
    \fn audioDeviceThreaded
    \brief constructor
*/
audioDeviceThreaded::audioDeviceThreaded(void)
{
    stopRequest = AUDIO_DEVICE_STOPPED;
}
/**
    \fn audioDeviceThreaded
    \brief destructor
*/
audioDeviceThreaded::~audioDeviceThreaded()
{
    // PURE! stop();
}
/**
    \fn bouncer
    \brief trampoline function
*/
static void *bouncer(void *in)
{
    audioDeviceThreaded *device = (audioDeviceThreaded *)in;
    device->Loop();
    return NULL;
}
/**

*/
void audioDeviceThreaded::Loop(void)
{
    printf("[AudioDeviceThreaded] Entering loop\n");
    while (stopRequest == AUDIO_DEVICE_STARTED)
    {

        sendData();
    }
    CHANGE_STATE(AUDIO_DEVICE_STOP_GR);
    printf("[AudioDeviceThreaded] Exiting loop\n");
}
/**
    \fn audioDeviceThreaded
    \brief destructor
*/
uint8_t audioDeviceThreaded::init(uint32_t channel, uint32_t fq, CHANNEL_TYPE *channelMapping)
{
    if (!channel || channel > MAX_CHANNELS)
    {
        ADM_error("Invalid number of channels %u, not trying to init audio device.\n", channel);
        return 0;
    }
    if (fq < MIN_SAMPLING_RATE || fq > MAX_SAMPLING_RATE)
    {
        ADM_error("Sampling frequency %u out of range %u - %u, not trying to init audio device.\n", fq,
                  MIN_SAMPLING_RATE, MAX_SAMPLING_RATE);
        return 0;
    }
    if (!channelMapping)
    {
        ADM_error("Channel mapping is NULL, not trying to init audio device.\n");
        return 0;
    }
    ADM_info("Initializing audioDeviceThreaded with channels=%d, fq=%d\n", (int)channel, (int)fq);
    // Allocate buffer
    memcpy(incomingMapping, channelMapping, sizeof(CHANNEL_TYPE) * MAX_CHANNELS);
    _channels = channel;

    std::string incoming, outgoing;

    const CHANNEL_TYPE *outgoingMapping = getWantedChannelMapping(_channels);
    if (!outgoingMapping)
    {
        ADM_error("No valid channel mapping from audio device.\n");
        return 0;
    }
    for (int i = 0; i < _channels; i++)
    {
        incoming =
            incoming + std::string("   ") + std::string(ADM_printChannel(incomingMapping[i])) + std::string("\n");
        outgoing = outgoing + std::string("   ") + std::string(ADM_printChannel(*outgoingMapping)) + std::string("\n");
        outgoingMapping++;
    }
    ADM_info("Incoming channel map:\n%s", incoming.c_str());
    ADM_info("Outgoing channel map:\n%s", outgoing.c_str());

    _frequency = fq;
    sizeOf10ms = (_channels * _frequency * 2) / 100;
    sizeOf10ms &= ~15; // make sure it is a multiple of 16
    silence.setSize(sizeOf10ms);
    memset(silence.at(0), 0, sizeOf10ms);
    audioBuffer.setSize(ADM_THREAD_BUFFER_SIZE);
    rdIndex = wrIndex = 0;
    CHANGE_STATE(AUDIO_DEVICE_STOPPED);
    //
    if (!localInit())
        return 0;
    // Spawn
    CHANGE_STATE(AUDIO_DEVICE_STARTED);
    ADM_assert(!pthread_create(&myThread, NULL, bouncer, this));

    return 1;
}
/**
    \fn getBufferFullness
    \brief Returns the number of 10 ms of audio in the buffer

*/
uint32_t audioDeviceThreaded::getBufferFullness(void)
{
    mutex.lock();
    float nbBytes = wrIndex - rdIndex;
    mutex.unlock();
    nbBytes /= sizeOf10ms;
    return 1 + (uint32_t)nbBytes;
}
/**
    \fn stop
    \brief stop
*/
uint8_t audioDeviceThreaded::stop()
{
    uint32_t maxWait = 3 * 1000; // Should be enough to drain
    ADM_info("[audioDevice] Stopping device...\n");
    if (stopRequest == AUDIO_DEVICE_STARTED)
    {
        CHANGE_STATE(AUDIO_DEVICE_STOP_REQ);
        while (stopRequest == AUDIO_DEVICE_STOP_REQ && maxWait)
        {
            ADM_usleep(1000);
            maxWait--;
        }
    }
    if (!maxWait)
    {
        ADM_error("Audio device did not stop cleanly\n");
    }
    localStop();
    audioBuffer.clean();
    silence.clean();
    CHANGE_STATE(AUDIO_DEVICE_STOPPED);
    return 1;
}
/**
    \fn write
    \brief We assume that we have full channels each time

*/
bool audioDeviceThreaded::writeData(uint8_t *data, uint32_t lenInByte)
{
    mutex.lock();
    if (wrIndex > ADM_THREAD_BUFFER_SIZE / 2 && rdIndex > ADM_THREAD_BUFFER_SIZE / 4)
    {
        memmove(audioBuffer.at(0), audioBuffer.at(rdIndex), wrIndex - rdIndex);
        wrIndex -= rdIndex;
        rdIndex = 0;
    }
    if (wrIndex + lenInByte > ADM_THREAD_BUFFER_SIZE)
    {
        printf("[AudioDevice] Overflow rd:%" PRIu32 "  start(wr):%u len%u limit%u\n", rdIndex, wrIndex, lenInByte,
               ADM_THREAD_BUFFER_SIZE);
        mutex.unlock();
        return false;
    }

    memcpy(audioBuffer.at(wrIndex), data, lenInByte);

    wrIndex += lenInByte;
    mutex.unlock();
    return true;
}
/**
    \fn read
*/
bool audioDeviceThreaded::readData(uint8_t *data, uint32_t lenInByte)
{
    mutex.lock();
    if (wrIndex - rdIndex < lenInByte)
    {
        printf("[AudioDevice] Underflow, wanted %u, only have %u\n", lenInByte, wrIndex - rdIndex);
        return false;
    }
    memcpy(data, audioBuffer.at(rdIndex), lenInByte);
    rdIndex += lenInByte;
    mutex.unlock();
    return true;
}
/**
    \fn play

*/
uint8_t audioDeviceThreaded::play(uint32_t len, float *data)
{
    // Reorder channels if needed...
    uint32_t samples = len;
    samples /= _channels;
    ADM_audioReorderChannels(_channels, (float *)(data), samples, incomingMapping,
                             (CHANNEL_TYPE *)getWantedChannelMapping(_channels));
    // dither to int16_t
    dither16(data, len, _channels);
    len *= 2;
    // Store in buffer
    return writeData((uint8_t *)data, len);
}
/**
    \fn getVolumeStats
    \brief Return stats about volume for each channel [8] as integer dBFS (valid range -100 to +3), mark inactive
   channel with +255 (> 3)
*/
bool audioDeviceThreaded::getVolumeStats(int32_t *vol)
{
    float f[MAX_CHANNELS], fsamp;
    int32_t raw[MAX_CHANNELS];
    for (int i = 0; i < 8; i++)
        vol[i] = 255;
    // 5 ms should be enough, i.e. fq/200
    uint32_t samples = _frequency / 200;
    mutex.lock();
    if (samples * _channels * 2 > (wrIndex - rdIndex))
        samples = (wrIndex - rdIndex) / (2 * _channels);
    for (int i = 0; i < MAX_CHANNELS; i++)
        f[i] = 0;
    if (!samples)
    {
        mutex.unlock();
        return true;
    }
    uint8_t *base8 = audioBuffer.at(rdIndex);
    int16_t *base = (int16_t *)(base8);
    for (int i = 0; i < samples; i++)
        for (int chan = 0; chan < _channels; chan++)
        {
            fsamp = base[0]; // multiply as float, to prevent theoretical overflow
            f[chan] += fsamp * fsamp;
            base++;
        }
    mutex.unlock();
    // Normalize
    for (int i = 0; i < MAX_CHANNELS; i++)
    {
        float d = f[i];
        d /= samples; // d is now between 0 and 32767^2
        d = sqrt(d);  // d is now the RMS between 0 and 32767
        if (d == 0.0) // log(0) is invalid
            d = -100.0;
        else
            d = 20.0 * log10(d / 32767.0) + 3.0; // dBFS = 20*log10(rms(signal)) + 3 dB
        if (d < -100.0)
            d = -100.0;
        d += 0.49; // rounding
        raw[i] = d;
    }
    // Assign mono to front center
    if (_channels == 1)
    {
        vol[1] = raw[0];
        return true;
    }
    // Move channels around so that they fit Rear Left / Side Left / Front Left / Center / LFE / Front Right / Side
    // Right / Rear Right
    const CHANNEL_TYPE *chans = this->getWantedChannelMapping(_channels);
    static const CHANNEL_TYPE output[8] = {ADM_CH_REAR_LEFT, ADM_CH_SIDE_LEFT,   ADM_CH_FRONT_LEFT, ADM_CH_FRONT_CENTER,
                                           ADM_CH_LFE,       ADM_CH_FRONT_RIGHT, ADM_CH_SIDE_RIGHT, ADM_CH_REAR_RIGHT};
    for (int i = 0; i < 8; i++)
    {
        CHANNEL_TYPE wanted = output[i];
        for (int j = 0; j < _channels; j++)
        {
            if (chans[j] == wanted)
            {
                vol[i] = raw[j];
                break;
            }
        }
    }
#if 0
#define zz vol
    printf("[%d] %02d %02d %02d %02d %02d %02d %02d %02d\n",samples,zz[0],zz[1],zz[2],zz[3],zz[4],zz[5],zz[6],zz[7]);
#endif
    return true;
}
//**
