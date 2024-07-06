#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_NO_DEVICE_IO
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_THREADING
#define MA_NO_ENGINE
#define MA_NO_GENERATION
#define MA_NO_SPATIAL

#ifndef INSOUND_DECODE_WAV
#define MA_NO_WAV
#endif

#ifndef INSOUND_DECODE_FLAC
#define MA_NO_FLAC
#endif

#ifndef INSOUND_DECODE_MP3
#define MA_NO_MP3
#endif

#define MA_NO_VORBIS // we'll use the libvorbis backend implementation
#define MA_NO_OPUS

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#ifdef MA_NO_LIBVORBIS
#undef MA_NO_LIBVORBIS
#endif

#include "miniaudio_libvorbis.h"
