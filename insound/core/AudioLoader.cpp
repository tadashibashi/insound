#include "AudioLoader.h"

#include "Engine.h"

#include "external/ctpl_stl.h"
#include "io/loadAudio.h"

#include <map>

namespace insound {
    struct AudioLoader::Impl {
        explicit Impl(const Engine *engine) : m_targetSpec(), m_threads(2)
        {
            engine->getSpec(&m_targetSpec);
        }

        ~Impl()
        {
            m_threads.stop(true);
        }

        const SoundBuffer *load(const fs::path &path)
        {

            if (const auto it = m_buffers.find(path); it != m_buffers.end())
            {
                return &it->second;
            }

            SoundBuffer buffer;
            if (!buffer.load(path, m_targetSpec))
            {
                return nullptr;
            }

            const auto &[emplacedIt, success] = m_buffers.emplace(path.native(), std::move(buffer));
            if (!success)
            {
                return nullptr;
            }

            m_count.store(m_count.load() + 1);
            return &emplacedIt->second;
        }

        const SoundBuffer *loadAsync(const fs::path &path, std::future<void> *outFuture)
        {
            if (const auto it = m_buffers.find(path); it != m_buffers.end())
            {
                return &it->second;
            }

            const auto &[emplacedIt, success] = m_buffers.emplace(path.native(), SoundBuffer());
            if (!success)
            {
                return nullptr;
            }

            auto sndBuffer = &emplacedIt->second;
            auto future = m_threads.push([path, sndBuffer, this](const int id) {
                if (sndBuffer->load(path, m_targetSpec))
                    m_count.store(m_count.load() + 1);
            });

            if (outFuture)
            {
                *outFuture = std::move(future);
            }

            return sndBuffer;
        }

        bool unload(const fs::path &path)
        {
            auto it = m_buffers.find(path.native());
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

        std::map<fs::path::string_type, SoundBuffer> m_buffers;
        std::atomic<size_t> m_count;
    };

    AudioLoader::AudioLoader(const Engine *engine) : m(new Impl(engine))
    {

    }

    AudioLoader::~AudioLoader()
    {
        delete m;
    }

    const SoundBuffer *AudioLoader::load(const fs::path &path)
    {
        return m->load(path);
    }

    const SoundBuffer *AudioLoader::loadAsync(const fs::path &path, std::future<void> *outFuture)
    {
#ifdef INSOUND_THREADING
        return m->loadAsync(path, outFuture);
#else
        return m->load(path);
#endif
    }

    bool AudioLoader::unload(const fs::path &path)
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
}
