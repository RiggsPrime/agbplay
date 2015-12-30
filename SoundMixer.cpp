#include <algorithm>
#include <cmath>
#include <cassert>

#include "SoundMixer.h"
#include "MyException.h"

using namespace std;
using namespace agbplay;

/*
 * public SoundMixer
 */

SoundMixer::SoundMixer(uint32_t sampleRate, uint32_t fixedModeRate, int reverb) : sq1(CGBType::SQ1), sq2(CGBType::SQ2), wave(CGBType::WAVE), noise(CGBType::NOISE)
{
    this->revdsp = new ReverbEffect(reverb, sampleRate, uint8_t(0x630 / (fixedModeRate / AGB_FPS)));
    this->sampleRate = sampleRate;
    this->samplesPerBuffer = sampleRate / (AGB_FPS * INTERFRAMES);
    this->sampleBuffer = vector<float>(N_CHANNELS * samplesPerBuffer);
    this->fixedModeRate = fixedModeRate;
    fill_n(this->sampleBuffer.begin(), this->sampleBuffer.size(), 0.0f);
    this->isShuttingDown = false;
}

SoundMixer::~SoundMixer()
{
    delete revdsp;
}

void SoundMixer::NewSoundChannel(void *owner, SampleInfo sInfo, ADSR env, Note note, uint8_t leftVol, uint8_t rightVol, int16_t pitch, bool fixed)
{
    sndChannels.emplace_back(owner, sInfo, env, note, leftVol, rightVol, pitch, fixed);
}

void SoundMixer::NewCGBNote(void *owner, CGBDef def, ADSR env, Note note, uint8_t leftVol, uint8_t rightVol, int16_t pitch, CGBType type)
{
    CGBChannel *nChn;
    switch (type) {
        case CGBType::SQ1: nChn = &sq1; break;
        case CGBType::SQ2: nChn = &sq2; break;
        case CGBType::WAVE: nChn = &wave; break;
        case CGBType::NOISE: nChn = &noise; break;
    }
    nChn->Init(owner, def, note, env);
    nChn->SetVol(leftVol, rightVol);
    nChn->SetPitch(pitch);
}

void SoundMixer::SetTrackPV(void *owner, uint8_t leftVol, uint8_t rightVol, int16_t pitch)
{
    for (SoundChannel& sc : sndChannels) {
        if (sc.GetOwner() == owner) {
            sc.SetVol(leftVol, rightVol);
            sc.SetPitch(pitch);
        }
    }
}

int SoundMixer::TickTrackNotes(void *owner)
{
    int active = 0;
    for (SoundChannel& chn : sndChannels) 
    {
        if (chn.GetOwner() == owner) {
            if (chn.TickNote())
                active++;
        }
    }
    return active;
}

void SoundMixer::StopChannel(void *owner, uint8_t key)
{
    for (SoundChannel& chn : sndChannels) 
    {
        if (chn.GetOwner() == owner && chn.GetMidiKey() == key && chn.GetState() < EnvState::REL) {
            chn.Release();
            return;
        }
    }
    if (sq1.GetOwner() == owner && sq1.GetMidiKey() == key && sq1.GetState() < EnvState::REL) {
        sq1.Release();
        return;
    }
    if (sq2.GetOwner() == owner && sq2.GetMidiKey() == key && sq2.GetState() < EnvState::REL) {
        sq2.Release();
        return;
    }
    if (wave.GetOwner() == owner && wave.GetMidiKey() == key && wave.GetState() < EnvState::REL) {
        wave.Release();
        return;
    }
    if (noise.GetOwner() == owner && noise.GetMidiKey() == key && noise.GetState() < EnvState::REL) {
        noise.Release();
        return;
    }
}

void *SoundMixer::ProcessAndGetAudio()
{
    if (isShuttingDown) {
        return nullptr;
    } else {
        clearBuffer();
        renderToBuffer();
        return (void *)&sampleBuffer[0];
    }
}

uint32_t SoundMixer::GetBufferUnitCount()
{
    return samplesPerBuffer;
}

void SoundMixer::Shutdown()
{
    this->isShuttingDown = true;
    for (SoundChannel& chn : sndChannels)
    {
        chn.Release();
    }
    sq1.Release();
    sq2.Release();
    wave.Release();
    noise.Release();
}

/*
 * private SoundMixer
 */

void SoundMixer::purgeChannels()
{
    sndChannels.remove_if([](SoundChannel& chn) { return chn.GetState() == EnvState::DEAD; });
}

void SoundMixer::clearBuffer()
{
    fill(sampleBuffer.begin(), sampleBuffer.end(), 0.0f);
}

void SoundMixer::renderToBuffer()
{
}
