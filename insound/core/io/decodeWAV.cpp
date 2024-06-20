#include "decodeWAV.h"

#include <map>
#include "../AudioSpec.h"
#include "../BufferView.h"
#include "../Error.h"

#include <SDL2/SDL_audio.h>

namespace insound {

#define WAVREAD_CHECK(bytesRead, expectedCount) do { if ((bytesRead) != (expectedCount)) \
    return WAVResult::InvalidFile; } while(0)

    static WAVResult::Enum parseUnknownChunk(BufferView &buf, uint32_t chunkSize)
    {
        buf.move(buf.position() + chunkSize);
        return WAVResult::Ok;
    }

    struct CuePoint {
        uint32_t id;               // unique cue id
        uint32_t playlistPosition; // ignored, since plist not supported
        char dataChunkID[5];       // either "data" or "slnt" depending on which chunk type it occurs in
        uint32_t chunkStart;       // start position of data chunk that contains the cue point. Only set when wavl available.
                                   // most DAWs do not export wavl, so this will almsot always be 0. In any case we're not supporting it.
        uint32_t blockStart;       // byte position of the "data" or "slnt" chunk the cue is in. If this is an uncompressed PCM, this is counted from the
                                   // beginning of the chunk's data. If compressed, the byte position can be counted from the last byte from which one can
                                   // start decompressing to find the cue. (Most likely points to the only data chunk found in most modern WAV)
        uint32_t sampleStart;      // The position of the cue in bytes from the start of the block.
    };

    /// "note" and "labl" sub-chunks
    struct CueStrings {
        uint32_t id{};
        std::string label{};
        std::string note{};
    };

    /// Populate labels and notes associated with cues
    static WAVResult::Enum parseLISTChunk(BufferView &buf, uint32_t chunkSize, std::map<uint32_t, CueStrings> *outCueStrings)
    {
        if (!outCueStrings)
        {
            return WAVResult::Ok;
        }

        auto &cueStrings = *outCueStrings;

        std::string header;
        WAVREAD_CHECK(buf.readFixedString(header, 4), 4);

        if (header == "adtl") // only adtl LIST sub-chunk supported
        {
            const auto dataStart = buf.position();
            const auto dataEnd   = dataStart + chunkSize - 4; // -4 for the listType id

            while(buf.position() < dataEnd)
            {
                WAVREAD_CHECK(buf.readFixedString(header, 4), 4);
                uint32_t subChunkSize;
                WAVREAD_CHECK(buf.read(subChunkSize), sizeof(uint32_t));
                if (header == "note" || header == "labl")
                {
                    uint32_t cueID;
                    WAVREAD_CHECK(buf.read(cueID), sizeof(uint32_t));

                    const auto stringLength = subChunkSize - sizeof(uint32_t);
                    const auto targetPos = buf.position() + stringLength; // in case string is padded, cache target pos to move to

                    if (header == "note")
                        buf.read(cueStrings[cueID].note);
                    else
                        buf.read(cueStrings[cueID].label);

                    buf.move(targetPos);
                }
                else
                {
                    buf.move(buf.position() + subChunkSize);
                }
            }
        }

        return WAVResult::Ok;
    }

    static WAVResult::Enum parseCueChunk(BufferView &buf, uint32_t chunkSize, std::map<uint32_t, CuePoint> *outPoints)
    {
        if (!outPoints)
        {
            return WAVResult::Ok;
        }

        auto &points = *outPoints;

        uint32_t nPoints;
        WAVREAD_CHECK(buf.read(nPoints), sizeof(uint32_t));

        for (uint32_t i = 0; i < nPoints; ++i)
        {
            // read each point
            CuePoint point{};
            WAVREAD_CHECK(buf.read(point.id), sizeof(point.id));
            WAVREAD_CHECK(buf.read(point.playlistPosition), sizeof(point.playlistPosition));
            WAVREAD_CHECK(buf.readRaw(point.dataChunkID, 4), 4);
            WAVREAD_CHECK(buf.read(point.chunkStart), sizeof(point.chunkStart));
            WAVREAD_CHECK(buf.read(point.blockStart), sizeof(point.blockStart));
            WAVREAD_CHECK(buf.read(point.sampleStart), sizeof(point.sampleStart));

            points[point.id] = point;
        }

        return WAVResult::Ok;
    }

    struct FormatData {
        uint16_t formatCode{};    ///< format type: pcm = 1, float = 3,
        uint16_t channels{};      ///< number of interleaved channels
        uint32_t samplerate{};    ///< sample rate - samples per second
        uint32_t avgBytesPerSecond{}; ///< data rate
        uint16_t blockAlign{};    ///< frame size (bytes in one sample frame)
        uint16_t bitsPerSample{}; ///< sample bits
        uint16_t cbSize{};        ///< size of the extension (0 or 22)
        uint16_t validBitsPerSample{}; ///< valid bits
        uint32_t channelMask{};   ///< speaker position mask
        std::string subFormat{};  ///< 16 bytes, GUID, including the data format code
    };

    static WAVResult::Enum parseFmtChunk(BufferView &buf, uint32_t chunkSize, FormatData *outData)
    {
        if (!outData)
        {
            buf.move(buf.position() + chunkSize);
            return WAVResult::Ok;
        }

        FormatData &data = *outData;
        if (chunkSize >= 16)
        {
            WAVREAD_CHECK(buf.read(data.formatCode), sizeof(uint16_t));
            WAVREAD_CHECK(buf.read(data.channels), sizeof(uint16_t));
            WAVREAD_CHECK(buf.read(data.samplerate), sizeof(uint32_t));
            WAVREAD_CHECK(buf.read(data.avgBytesPerSecond), sizeof(uint32_t));
            WAVREAD_CHECK(buf.read(data.blockAlign), sizeof(uint16_t));
            WAVREAD_CHECK(buf.read(data.bitsPerSample), sizeof(uint16_t));
        }

        if (chunkSize >= 18)
        {
            WAVREAD_CHECK(buf.read(data.cbSize), sizeof(uint16_t));
        }

        if (chunkSize >= 40)
        {
            WAVREAD_CHECK(buf.read(data.validBitsPerSample), sizeof(uint16_t));
            WAVREAD_CHECK(buf.read(data.channelMask), sizeof(uint32_t));
            WAVREAD_CHECK(buf.readFixedString(data.subFormat, 16), 16);
        }

        return WAVResult::Ok;
    }

    static WAVResult::Enum parseDataChunk(BufferView &buf, uint32_t chunkSize, const FormatData &fmt, uint8_t **outWavData, uint32_t *outDataSize)
    {
        if (!outWavData)
        {
            buf.move(buf.position() + chunkSize); // todo: this should return result, user may exceed bounds
            return WAVResult::Ok;
        }

        double sizeFactor = 1.0;
        if (fmt.bitsPerSample == 24) // find factor to increase buffer size to upscale 24 to 32-bit
        {
            sizeFactor = 32.0 / 24.0;
        }

        const uint32_t bufferSize = std::ceil((double)chunkSize * sizeFactor);

        auto wavData = (uint8_t *)std::malloc(bufferSize);
        if (!wavData)
        {
            pushError(Result::OutOfMemory, "while parsing WAV data chunk");
            return WAVResult::RuntimeErr;
        }

        if (fmt.bitsPerSample == 24) // upscale to 32-bit
        {
            const auto sampleCount = chunkSize / 3;
            for (auto i = 0; i < sampleCount; ++i)
            {
                uint8_t sample[3];
                if (buf.readRaw(sample, 3) != 3)
                {
                    std::free(wavData);
                    return WAVResult::RuntimeErr;
                }

                const auto o = i * 4;
                wavData[o] = 0;
                wavData[o + 1] = sample[0];
                wavData[o + 2] = sample[1];
                wavData[o + 3] = sample[2];
            }
        }
        else
        {
            if (buf.readRaw(wavData, chunkSize) != chunkSize)
            {
                std::free(wavData);
                return WAVResult::RuntimeErr;
            }
        }

        *outWavData = wavData;
        *outDataSize = bufferSize;

        return WAVResult::Ok;
    }

    /// Load a WAV file into PCM data
    /// Slower, since it streams WAV from disk instead of loading it all at once, but saves on RAM.
    /// It may be good to run this in another thread.
    WAVResult::Enum decodeWAV_v2(const uint8_t *memory, const uint32_t size,
        AudioSpec *outSpec, uint8_t **outBuffer, uint32_t *outBufferSize, std::map<uint32_t, Marker> *outCues)
    {
        // Read the header for format info
        BufferView buf(memory, size, endian::little);

        std::string header;
        header.reserve(4);

        // "RIFF"
        WAVREAD_CHECK(buf.readFixedString(header, 4), 4);
        if (header != "RIFF")
        {
            pushError(Result::UnexpectedData, "Invalid WAV file: missing \"RIFF\" header");
            return WAVResult::InvalidFile;
        }

        uint32_t fileSize = 0;
        WAVREAD_CHECK(buf.read(fileSize), sizeof(uint32_t));

        // "WAVE"
        WAVREAD_CHECK(buf.readFixedString(header, 4), 4);
        if (header != "WAVE")
        {
            pushError(Result::UnexpectedData, "Invalid WAV file: missing \"WAVE\" header");
            return WAVResult::InvalidFile;
        }

        FormatData fmt;
        uint8_t *wavData = nullptr;
        uint32_t dataSize = 0;
        std::map<uint32_t, CuePoint> cuePoints;
        std::map<uint32_t, CueStrings> cueStrings;

        // Parse all chunks
        while (buf.position() < buf.size())
        {
            // Read header & chunk size
            WAVREAD_CHECK(buf.readFixedString(header, 4), 4);

            uint32_t chunkSize;
            WAVREAD_CHECK(buf.read(chunkSize), sizeof(chunkSize));

            const auto curPosition = buf.position();

            if (header == "fmt ")
            {
                if (const auto result = parseFmtChunk(buf, chunkSize, &fmt); result != WAVResult::Ok)
                    return result;
            }
            else if (header == "data")
            {
                if (const auto result = parseDataChunk(buf, chunkSize, fmt, &wavData, &dataSize); result != WAVResult::Ok)
                    return result;
            }
            else if (header == "LIST")
            {
                parseLISTChunk(buf, chunkSize, &cueStrings);
            }
            else if (header == "cue ")
            {
                parseCueChunk(buf, chunkSize, &cuePoints);
            }
            else
            {
                // skips the chunk
                parseUnknownChunk(buf, chunkSize);
            }

            if (chunkSize % 2 != 0)
                ++chunkSize;

            if (buf.position() != curPosition + chunkSize)
            {
                // Log a warning here?
                buf.move(curPosition + chunkSize);
            }
        }

        if (outCues)
        {
            // Synthesize cue info
            std::map<uint32_t, Marker> cues;

            for (auto &[id, point] : cuePoints)
            {
                cues[id].position = point.sampleStart;
            }

            for (auto &[id, strings] : cueStrings)
            {
                cues[id].label = strings.label;
            }

            outCues->swap(cues);
        }

        // Return out values
        if (outBufferSize)
            *outBufferSize = dataSize;

        if (outBuffer)
            *outBuffer = wavData;
        else // clean up if outBuffer was discarded
            std::free(wavData);

        if (outSpec)
        {
            outSpec->channels = fmt.channels;
            outSpec->freq = (int)fmt.samplerate;
            outSpec->format = SampleFormat(
                fmt.bitsPerSample == 24 ? 32 : fmt.bitsPerSample,
                fmt.formatCode == 3,
                false,
                fmt.bitsPerSample != 8);
        }

        return WAVResult::Ok;
    }

    bool decodeWAV(const uint8_t *memory, uint32_t size, AudioSpec *outSpec, uint8_t **outBuffer, uint32_t *outBufferSize)
    {
        SDL_AudioSpec sdlspec;
        uint32_t length;
        uint8_t *buffer;

        if (!SDL_LoadWAV_RW(SDL_RWFromConstMem(memory, (int)size), SDL_TRUE, &sdlspec, &buffer, &length))
        {
            pushError(Result::SdlErr, SDL_GetError());
            return false;
        }

        if (outSpec)
        {
            AudioSpec spec;
            spec.channels = sdlspec.channels;
            spec.format = SampleFormat(
                SDL_AUDIO_BITSIZE(sdlspec.format),
                SDL_AUDIO_ISFLOAT(sdlspec.format),
                SDL_AUDIO_ISBIGENDIAN(sdlspec.format),
                SDL_AUDIO_ISSIGNED(sdlspec.format));
            spec.freq = sdlspec.freq;

            *outSpec = spec;
        }

        if (outBuffer)
        {
            *outBuffer = buffer;
        }
        else
        {
            free(buffer);
        }

        if (outBufferSize)
        {
            *outBufferSize = length;
        }

        return true;
    }
}
