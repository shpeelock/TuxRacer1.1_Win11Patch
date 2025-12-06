/*
 * smpeg.dll - Replacement wrapper for Tux Racer 1.1
 * Fixes crashes related to music playback on modern systems.
 */

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static CRITICAL_SECTION g_lock;
static BOOL g_initialized = FALSE;

// Define SMPEG struct
typedef struct {
    drmp3 mp3;
    drmp3_uint64 currentFrame;
    BOOL playing;
    BOOL audioEnabled;
    BOOL loaded;
    BOOL loop;
    char filename[MAX_PATH];
} SMPEG;

static SMPEG* g_currentMusic = NULL;

typedef struct {
    int freq;
    unsigned short format;
    unsigned char channels;
    unsigned char silence;
    unsigned short samples;
    unsigned short padding;
    unsigned int size;
    void (*callback)(void* userdata, unsigned char* stream, int len);
    void* userdata;
} SDL_AudioSpec;

void InitLib() {
    if (!g_initialized) {
        InitializeCriticalSection(&g_lock);
        g_initialized = TRUE;
    }
}

BOOL LoadMP3File(SMPEG* smpeg, const char* filename) {
    if (!drmp3_init_file(&smpeg->mp3, filename, NULL)) {
        return FALSE;
    }
    smpeg->currentFrame = 0;
    smpeg->loaded = TRUE;
    return TRUE;
}

__declspec(dllexport) SMPEG* SMPEG_new(const char* filename, void* info, int audio) {
    InitLib();
    
    SMPEG* smpeg = (SMPEG*)calloc(1, sizeof(SMPEG));
    if (!smpeg) return NULL;
    
    strncpy(smpeg->filename, filename, MAX_PATH - 1);
    smpeg->loop = TRUE;
    
    if (LoadMP3File(smpeg, filename)) {
        smpeg->loaded = TRUE;
        
        EnterCriticalSection(&g_lock);
        // Auto-play and set as current music
        smpeg->playing = TRUE;
        if (g_currentMusic) {
            g_currentMusic->playing = FALSE;
        }
        g_currentMusic = smpeg;
        LeaveCriticalSection(&g_lock);

        if (info) {
            typedef struct {
                int has_audio;
                int has_video;
                int width;
                int height;
                int current_frame;
                double current_fps;
                char audio_string[80];
                int audio_current_frame;
                unsigned int current_offset;
                unsigned int total_size;
                double current_time;
                double total_time;
            } SMPEG_Info_Internal;
            
            SMPEG_Info_Internal* sinfo = (SMPEG_Info_Internal*)info;
            memset(sinfo, 0, sizeof(SMPEG_Info_Internal));
            sinfo->has_audio = 1;
            sinfo->has_video = 0;
            sprintf(sinfo->audio_string, "MP3 %dHz %dch", smpeg->mp3.sampleRate, smpeg->mp3.channels);
        }
    } else {
        free(smpeg);
        return NULL;
    }
    
    return smpeg;
}

__declspec(dllexport) void SMPEG_delete(SMPEG* smpeg) {
    if (!smpeg) return;
    
    EnterCriticalSection(&g_lock);
    if (g_currentMusic == smpeg) {
        g_currentMusic = NULL;
    }
    if (smpeg->loaded) {
        drmp3_uninit(&smpeg->mp3);
    }
    LeaveCriticalSection(&g_lock);
    
    free(smpeg);
}

__declspec(dllexport) void SMPEG_play(SMPEG* smpeg) {
    if (!smpeg || !smpeg->loaded) return;
    EnterCriticalSection(&g_lock);
    smpeg->playing = TRUE;
    LeaveCriticalSection(&g_lock);
}

__declspec(dllexport) void SMPEG_stop(SMPEG* smpeg) {
    if (!smpeg) return;
    EnterCriticalSection(&g_lock);
    smpeg->playing = FALSE;
    LeaveCriticalSection(&g_lock);
}

__declspec(dllexport) void SMPEG_rewind(SMPEG* smpeg) {
    if (!smpeg || !smpeg->loaded) return;
    EnterCriticalSection(&g_lock);
    drmp3_seek_to_pcm_frame(&smpeg->mp3, 0);
    smpeg->currentFrame = 0;
    LeaveCriticalSection(&g_lock);
}

__declspec(dllexport) void SMPEG_seek(SMPEG* smpeg, int position) {
    if (!smpeg || !smpeg->loaded) return;
    EnterCriticalSection(&g_lock);
    drmp3_seek_to_pcm_frame(&smpeg->mp3, position);
    LeaveCriticalSection(&g_lock);
}

__declspec(dllexport) void SMPEG_setvolume(SMPEG* smpeg, int volume) {
}

__declspec(dllexport) void SMPEG_enableaudio(SMPEG* smpeg, int enable) {
    if (!smpeg) return;
    smpeg->audioEnabled = enable ? TRUE : FALSE;
}

__declspec(dllexport) void SMPEG_enablevideo(SMPEG* smpeg, int enable) {
}

__declspec(dllexport) int SMPEG_status(SMPEG* smpeg) {
    if (!smpeg) return 0;
    return smpeg->playing ? 1 : 0;
}

__declspec(dllexport) void SMPEG_getinfo(SMPEG* smpeg, void* info) {
    if (!smpeg || !info) return;
    
    typedef struct {
        int has_audio;
        int has_video;
        int width;
        int height;
        int current_frame;
        double current_fps;
        char audio_string[80];
        int audio_current_frame;
        unsigned int current_offset;
        unsigned int total_size;
        double current_time;
        double total_time;
    } SMPEG_Info;
    
    SMPEG_Info* sinfo = (SMPEG_Info*)info;
    memset(sinfo, 0, sizeof(SMPEG_Info));
    
    if (smpeg->loaded) {
        sinfo->has_audio = 1;
        sinfo->has_video = 0;
        sprintf(sinfo->audio_string, "MP3 %dHz %dch", smpeg->mp3.sampleRate, smpeg->mp3.channels);
    }
}

__declspec(dllexport) void SMPEG_actualSpec(SMPEG* smpeg, SDL_AudioSpec* spec) {
}

__declspec(dllexport) int SMPEG_playAudio(SMPEG* smpeg, void* stream, int len) {
    if (!smpeg || !smpeg->loaded || !stream) {
        if (stream) memset(stream, 0, len);
        return 0;
    }

    if (!smpeg->playing || !smpeg->audioEnabled) {
        memset(stream, 0, len);
        return 0;
    }

    EnterCriticalSection(&g_lock);
    
    // Downsample 44100 Hz -> 22050 Hz (2:1)
    int outputFrames = len / (2 * smpeg->mp3.channels);
    int inputFrames = outputFrames * 2;
    
    drmp3_int16* tempBuffer = (drmp3_int16*)malloc(inputFrames * smpeg->mp3.channels * sizeof(drmp3_int16));
    if (!tempBuffer) {
        LeaveCriticalSection(&g_lock);
        return 0;
    }
    
    drmp3_uint64 framesRead = drmp3_read_pcm_frames_s16(&smpeg->mp3, inputFrames, tempBuffer);
    
    short* outStream = (short*)stream;
    int outIndex = 0;
    int channels = smpeg->mp3.channels;
    
    for (int i = 0; i < framesRead; i += 2) {
        for (int c = 0; c < channels; c++) {
            if (outIndex < (len/2)) {
                int s1 = tempBuffer[i*channels + c];
                int s2 = tempBuffer[(i+1)*channels + c];
                outStream[outIndex++] = (short)((s1 + s2) / 2);
            }
        }
    }
    
    int bytesWritten = outIndex * 2;
    free(tempBuffer);
    
    if (bytesWritten < len) {
        memset((char*)stream + bytesWritten, 0, len - bytesWritten);
    }
    
    LeaveCriticalSection(&g_lock);
    return bytesWritten;
}

__declspec(dllexport) void SMPEG_playAudioSDL(SMPEG* smpeg, void* stream, int len) {
    SMPEG_playAudio(smpeg, stream, len);
}

__declspec(dllexport) int SMPEG_wantedSpec(SMPEG* smpeg, SDL_AudioSpec* wanted) {
    if (!smpeg || !wanted || !smpeg->loaded) return -1;
    
    wanted->freq = smpeg->mp3.sampleRate;
    wanted->format = 0x8010;
    wanted->channels = smpeg->mp3.channels;
    wanted->samples = 4096;
    
    return 0;
}

// Stubs
__declspec(dllexport) void SMPEG_loop(SMPEG* smpeg, int repeat) {}
__declspec(dllexport) void SMPEG_scaleXY(SMPEG* smpeg, int width, int height) {}
__declspec(dllexport) void SMPEG_scale(SMPEG* smpeg, int scale) {}
__declspec(dllexport) void SMPEG_move(SMPEG* smpeg, int x, int y) {}
__declspec(dllexport) void SMPEG_setdisplay(SMPEG* smpeg, void* surface, void* lock, void* callback) {}
__declspec(dllexport) void SMPEG_setdisplayregion(SMPEG* smpeg, int x, int y, int w, int h) {}
__declspec(dllexport) void SMPEG_renderFrame(SMPEG* smpeg, int frame) {}
__declspec(dllexport) void SMPEG_renderFinal(SMPEG* smpeg, void* surface, int x, int y) {}
__declspec(dllexport) void* SMPEG_filter(SMPEG* smpeg, void* filter) { return NULL; }
__declspec(dllexport) char* SMPEG_error(SMPEG* smpeg) { return NULL; }
__declspec(dllexport) double SMPEG_frameRate(SMPEG* smpeg) { return 0.0; }

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            InitLib();
            break;
        case DLL_PROCESS_DETACH:
            if (g_initialized) {
                DeleteCriticalSection(&g_lock);
            }
            break;
    }
    return TRUE;
}