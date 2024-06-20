#pragma once
#include <filesystem>
#include <future>

#include "SoundBuffer.h"

namespace insound {
    namespace fs = std::filesystem;

    class Engine;

    class AudioLoader {
    public:
        explicit AudioLoader(const Engine *engine);
        ~AudioLoader();

        /// @param path audio file path to open
        /// @returns loaded sound buffer, or nullptr if there was an error
        const SoundBuffer *load(const fs::path &path);

        /// @param path      audio file path to open
        /// @param outFuture useful to know if the load function finished, if the sound is still !isLoaded(), it failed.
        /// @returns sound buffer, check isLoaded to make sure it is okay to use
        const SoundBuffer *loadAsync(const fs::path &path, std::future<void> *outFuture = nullptr);

        /// Unload a particular audio file from memory
        /// @param path filepath to unload, should match the path passed to `load` or `loadAsync`.
        /// @returns whether file was unloaded for the path provided
        bool unload(const fs::path &path);

        /// Unload all loaded files from memory
        bool unloadAll();

        /// Number of loaded audio files. If `loadAsync` was called and the file has not loaded yet, it will not
        /// count toward the number here.
        [[nodiscard]]
        size_t size() const;

        [[nodiscard]]
        bool empty() const;
    private:
        struct Impl;
        Impl *m;
    };
}
