#include "AudioLoader.h"

#include "Engine.h"
#include "external/ctpl_stl.h"
#include "path.h"

#include <map>

namespace insound {
    struct AudioLoader::Impl {
        explicit Impl(const Engine *engine) : m_targetSpec(), m_threads(2), m_count(), m_baseDir()
        {
            engine->getSpec(&m_targetSpec);
        }

        ~Impl()
        {
            m_threads.stop(true);
        }

        const SoundBuffer *load(const std::string &path)
        {
            auto finalPath = path::join(m_baseDir, path);
            if (const auto it = m_buffers.find(finalPath); it != m_buffers.end())
            {
                return &it->second;
            }

            SoundBuffer buffer;
            if (!buffer.load(finalPath, m_targetSpec))
            {
                return nullptr;
            }

            const auto &[emplacedIt, success] = m_buffers.emplace(finalPath, std::move(buffer));
            if (!success)
            {
                return nullptr;
            }

            m_count.store(m_count.load() + 1);
            return &emplacedIt->second;
        }

        const SoundBuffer *loadAsync(const std::string &path, std::future<void> *outFuture)
        {
            auto finalPath = path::join(m_baseDir, path);
            if (const auto it = m_buffers.find(finalPath); it != m_buffers.end())
            {
                return &it->second;
            }

            const auto &[emplacedIt, success] = m_buffers.emplace(finalPath, SoundBuffer());
            if (!success)
            {
                return nullptr;
            }

            auto sndBuffer = &emplacedIt->second;
            auto future = m_threads.push([finalPath, sndBuffer, this](const int id) {
                if (sndBuffer->load(finalPath, m_targetSpec))
                    m_count.store(m_count.load() + 1);
            });

            if (outFuture)
            {
                *outFuture = std::move(future);
            }

            return sndBuffer;
        }

        bool unload(const std::string &path)
        {
            const auto finalPath = path::join(m_baseDir, path);
            auto it = m_buffers.find(finalPath);
            if (it != m_buffers.end() && it->second.isLoaded())
            {
                m_buffers.erase(it);
                m_count.store(m_count.load() - 1);
                return true;
            }

            return false;
        }

        bool unloadAll()
        {
            auto empty = m_buffers.empty();
            m_buffers.clear();

            m_count.store(0);
            return !empty;
        }

        [[nodiscard]]
        size_t size() const
        {
            return m_count.load();
        }

        [[nodiscard]]
        bool empty() const
        {
            return m_count.load() == 0;
        }

        AudioSpec m_targetSpec;
        ctpl::thread_pool m_threads; ///< threads for handling async loading

        std::map<std::string, SoundBuffer> m_buffers;
        std::atomic<size_t> m_count;
        std::string m_baseDir;
    };

    AudioLoader::AudioLoader(const Engine *engine) : m(new Impl(engine))
    {

    }

    AudioLoader::~AudioLoader()
    {
        delete m;
    }

    const SoundBuffer *AudioLoader::load(const std::string &path)
    {
        return m->load(path);
    }

    const SoundBuffer *AudioLoader::loadAsync(const std::string &path, std::future<void> *outFuture)
    {
#ifdef INSOUND_THREADING
        return m->loadAsync(path, outFuture);
#else
        return m->load(path);
#endif
    }

    bool AudioLoader::unload(const std::string &path)
    {
        return m->unload(path);
    }

    bool AudioLoader::unloadAll()
    {
        return m->unloadAll();
    }

    size_t AudioLoader::size() const
    {
        return m->size();
    }

    bool AudioLoader::empty() const
    {
        return m->empty();
    }

    const std::string &AudioLoader::baseDir() const
    {
        return m->m_baseDir;
    }

    void AudioLoader::setBaseDir(const std::string &path)
    {
        m->m_baseDir = path;
    }
}
