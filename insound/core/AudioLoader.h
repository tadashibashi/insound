#pragma once
#include "SoundBuffer.h"
#include <future>
#include <string>


namespace insound {
    class Engine;

    class AudioLoader {
    public:
        explicit AudioLoader(const Engine *engine);
        ~AudioLoader();

        /// @param path audio file path to open
        /// @returns loaded sound buffer, or nullptr if there was an error
        const SoundBuffer *load(const std::string &path);

        /// @param path      audio file path to open
        /// @param outFuture useful to know if the load function finished, if the sound is still !isLoaded(), it failed.
        /// @returns sound buffer, check isLoaded to make sure it is okay to use
        const SoundBuffer *loadAsync(const std::string &path, std::future<void> *outFuture = nullptr);

        /// Unload a particular audio file from memory
        /// @param path filepath to unload, should match the path passed to `load` or `loadAsync`.
        /// @returns whether file was unloaded for the path provided
        bool unload(const std::string &path);

        /// Unload all loaded files from memory
        bool unloadAll();

        /// Number of loaded audio files. If `loadAsync` was called and the file has not loaded yet, it will not
        /// count toward the number here.
        [[nodiscard]]
        size_t size() const;

        [[nodiscard]]
        bool empty() const;

        [[nodiscard]]
        const std::string &baseDir() const;
        void setBaseDir(const std::string &path);
    private:
        struct Impl;
        Impl *m;
    };
}
