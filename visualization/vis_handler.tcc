#include <functional>

#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>

struct VisualizationBuffer {
	double const* audio_buffer;
	double tempo_estimate;
	bool is_new_beat;
};

class VisualizationHandler {

private:
	SDL_mutex* vh_mutex;
    SDL_cond* vh_cond;
    SDL_Thread* vh_thread;

    bool should_stop = false;
    bool buffer_processed = false;
    VisualizationBuffer buffer;

    static int worker_thread (void * _self) {
    	VisualizationHandler* self = static_cast<VisualizationHandler*>(_self);
    	while (true) {
    		SDL_LockMutex(self->vh_mutex);
			if (self->should_stop) {
				SDL_UnlockMutex(self->vh_mutex);
				SDL_CondBroadcast(self->vh_cond);
				printf("Exiting worker_thread.\n");
				break;
			}

			SDL_CondWait(self->vh_cond, self->vh_mutex);
			if (self->buffer_processed == true) {
				continue;
			}

			self->visualize(self->buffer);
			self->buffer_processed = true;

	        SDL_UnlockMutex(self->vh_mutex);
	        SDL_CondSignal(self->vh_cond);
    	}
    	return 0;
	}

	void stop_thread () {
		// Send stop signal
		SDL_LockMutex(vh_mutex);
		should_stop = true;
		SDL_UnlockMutex(vh_mutex);
		SDL_CondSignal(vh_cond);

		// Await thread exit
		SDL_WaitThread(vh_thread, NULL);
	}

	virtual void visualize (VisualizationBuffer const&) = 0;
	virtual void get_result (float*) = 0;

protected:
    SDL_AudioSpec const& spec;

public:
	virtual unsigned int get_result_size() = 0;

	virtual void process_ring_buffer (VisualizationBuffer const& data) final {
		SDL_LockMutex(vh_mutex);

		buffer = data;
		buffer_processed = false;

        SDL_UnlockMutex(vh_mutex);
        SDL_CondSignal(vh_cond);
	}

	void await_buffer_processed (bool unlock = true) {
		SDL_LockMutex(vh_mutex);
		while (buffer_processed == false) {
			SDL_CondWait(vh_cond, vh_mutex);
		}
		if (unlock) {
			SDL_UnlockMutex(vh_mutex);
		}
	}

	void await_result (float* result) {
		await_buffer_processed(false);
		// SDL_LockMutex(vh_mutex);
		get_result(result);
		SDL_UnlockMutex(vh_mutex);
	}

	void unlock_mutex () {
		SDL_UnlockMutex(vh_mutex);
	}

	VisualizationHandler (SDL_AudioSpec const& spec) : spec(spec) {
		vh_thread = SDL_CreateThread(&VisualizationHandler::worker_thread, "visualization worker", (void *) this);
		vh_mutex = SDL_CreateMutex();
        vh_cond = SDL_CreateCond();
	}

    ~VisualizationHandler () {
    	stop_thread();
        SDL_DestroyCond(vh_cond);
        SDL_DestroyMutex(vh_mutex);
    }
};
