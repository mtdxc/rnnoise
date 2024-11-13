#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include "rnnoise.h"
#pragma comment(lib, "winmm.lib")
#define SMAPLE_RATE 48000
#define NUM_SAMPLES 480

volatile bool stop = false;
HWAVEOUT hWaveOut = NULL;
HWAVEIN hWaveIn = NULL;
DenoiseState* st = NULL;
void denoise(short* pcm) {
    int i = 0;
    float x[NUM_SAMPLES], y[NUM_SAMPLES];
    for (i = 0; i < NUM_SAMPLES; i++)
        x[i] = pcm[i];
    rnnoise_process_frame(st, y, x);
    for (i = 0; i < NUM_SAMPLES; i++)
        pcm[i] = y[i];
}

void freeWavHdr(WAVEHDR* hdr) {
    if (!hdr) return;
    delete[] hdr->lpData;
    delete hdr;
}

void Play(WAVEHDR* pwhOut) {
    auto result = waveOutPrepareHeader(hWaveOut, pwhOut, sizeof(WAVEHDR));
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "Failed to prepare output header." << std::endl;
        freeWavHdr(pwhOut);
        waveOutClose(hWaveOut);
        return;
    }

    result = waveOutWrite(hWaveOut, pwhOut, sizeof(WAVEHDR));
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "Failed to write output buffer." << std::endl;
        waveOutUnprepareHeader(hWaveOut, pwhOut, sizeof(WAVEHDR));
        freeWavHdr(pwhOut);
        waveOutClose(hWaveOut);
    }
}

void Record() {
    WAVEHDR* pwhIn = new WAVEHDR;
    memset(pwhIn, 0, sizeof(WAVEHDR));
    pwhIn->dwBufferLength = NUM_SAMPLES * 2;
    pwhIn->lpData = new char[pwhIn->dwBufferLength];

    auto result = waveInPrepareHeader(hWaveIn, pwhIn, sizeof(WAVEHDR));
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "Failed to prepare input header." << std::endl;
        freeWavHdr(pwhIn);
        waveInClose(hWaveIn);
        return;
    }

    result = waveInAddBuffer(hWaveIn, pwhIn, sizeof(WAVEHDR));
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "Failed to add input buffer." << std::endl;
        waveInUnprepareHeader(hWaveIn, pwhIn, sizeof(WAVEHDR));
        freeWavHdr(pwhIn);
        waveInClose(hWaveIn);
    }
}

void CALLBACK waveInProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    switch (uMsg)
    {
    case WIM_DATA:
      if (WAVEHDR* pwh = (WAVEHDR*)dwParam1) {
        if(stop) return ;
        waveInUnprepareHeader(hwi, pwh, sizeof(WAVEHDR));
        denoise((short*)pwh->lpData);
        Play(pwh);
        // record next buffer
        Record();
      }
      break;
    case WIM_OPEN:
        printf("WIM_OPEN\n");
        break;
    case WIM_CLOSE:
        printf("WIM_CLOSE\n");
        break;    
    default:
        break;
    }
}

void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    switch (uMsg)
    {
    case WOM_DONE:
        if (WAVEHDR* pwh = (WAVEHDR*)dwParam1) {
            if (stop) return;
            waveOutUnprepareHeader(hwo, pwh, sizeof(WAVEHDR));
            freeWavHdr(pwh);
        }
        break;
    case WOM_OPEN:
        printf("WOM_OPEN\n");
        break;
    case WOM_CLOSE:
        printf("WOM_CLOSE\n");
        break;
    default:
        break;
    }
}

int main(int argc, char** argv) {
    WAVEFORMATEX wfx;
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.wBitsPerSample = 16;
    wfx.nSamplesPerSec = SMAPLE_RATE;
    wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    // Recording
    MMRESULT result = waveInOpen(&hWaveIn, WAVE_MAPPER, &wfx, (DWORD_PTR)waveInProc, 0, CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "Failed to open waveform input device." << std::endl;
        return 1;
    }

    result = waveInStart(hWaveIn);
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "Failed to start recording." << std::endl;
        waveInClose(hWaveIn);
        return 1;
    }

    // Playback
    result = waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, (DWORD_PTR)waveOutProc, 0, CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "Failed to open waveform output device." << std::endl;
        return 1;
    }
    RNNModel* model = NULL;
    if(argc>1)
        model = rnnoise_model_from_filename(argv[1]);// "weights_blob.bin");
    st = rnnoise_create(model);

    Record();
    std::cout << "Playing... Press Enter to stop." << std::endl;
    std::cin.get();
    
    // ËÀËø±ÜÃâ: @see https://blog.csdn.net/weixin_34256074/article/details/90326418
    stop = true;
    Sleep(40);

    result = waveInReset(hWaveIn);
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "Failed to stop recording." << std::endl;
    }
    
    result = waveInClose(hWaveIn);
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "Failed to close waveform input device." << std::endl;
    }

    result = waveOutReset(hWaveOut);
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "Failed to reset waveform output device." << std::endl;
    }

    result = waveOutClose(hWaveOut);
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "Failed to close waveform output device." << std::endl;
    }

    if (st) {
        rnnoise_destroy(st);
        st = NULL;
    }
    if (model) {
        rnnoise_model_free(model);
        model = NULL;
    }
    return 0;
}