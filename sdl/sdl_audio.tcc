#include <string>
#include <vector>
#include <stdexcept>
#include <sstream>

namespace audio {

    void sdl_init () {
        SDL_SetHint(SDL_HINT_AUDIO_INCLUDE_MONITORS, "1");
        SDL_Init(SDL_INIT_AUDIO);
    }

    template <typename SampleT>
    void sdl_audio_cb (void* userdata, uint8_t* stream, int len) {
        RingBuffer<SampleT>* rBuf = (RingBuffer<SampleT>*) userdata;
        SampleT* buf = rBuf->dequeue_clean();
        std::memcpy(buf, stream, len);
        rBuf->enqueue_dirty(buf);
    }

    std::vector<std::string> get_audio_device_names () {
        int i, count = SDL_GetNumAudioDevices(true);
        std::vector<std::string> device_names;
        for (i = 0; i < count; i++) {
            device_names.push_back(std::string(SDL_GetAudioDeviceName(i, true)));
        }
        return device_names;
    }

    template <typename SampleT>
    auto start_audio_stream (RingBuffer<SampleT>* rb, SDL_AudioSpec& spec, int device_id) {
        sdl_init();

        SDL_AudioDeviceID dev;
        SDL_AudioSpec spec_avail;
        spec.callback = sdl_audio_cb<SampleT>;
        spec.userdata = rb;
        dev = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(device_id, 1), 1, &spec, &spec_avail, 0);

        if (spec_avail.format != spec.format) {
            throw std::runtime_error("We didn't get the wanted format.");
        }

        if (spec_avail.samples != spec.samples) {
            // spec.samples = spec_avail.samples;
            printf("Audio device reported different fragment length of %d samples.\n", spec_avail.samples);
            // throw std::runtime_error("We didn't get the wanted samples.");
        }


        SDL_PauseAudioDevice(dev, 0);

        double len_s = ((double) spec_avail.samples) / spec_avail.freq;
        printf("Audio streaming with fragment length of %.1f ms\n", len_s * 1000);
        printf("\n");

        if (dev == 0) {
            std::stringstream ss;
            ss << "Failed to open audio: " << SDL_GetError();
            throw std::runtime_error(ss.str());
        }

        printf("SDL Audio initialized\n");

        auto close_audio_stream = [&dev] () {
            SDL_CloseAudioDevice(dev);
        };
        return close_audio_stream;
    }

} // namespace audio