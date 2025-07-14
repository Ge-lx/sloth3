#include <SDL2/SDL.h>
#include <SDL2/SDL_mutex.h>
#include <SDL2/SDL_thread.h>
#include <deque>
#include <vector>

#ifndef VIS_HANDLER_H
#define VIS_HANDLER_H

struct VisualizationBuffer {
	double const* audio_buffer;
	double tempo_estimate;
	bool is_new_beat;
};

struct DisplayParams {
	double base, scale; // Radius base and extrusion scaling
	float color_inner[4]; // Inner color of the circle
};

class VisualizationHandler {

private:
	SDL_mutex* vh_mutex;
    SDL_cond* vh_cond;
    SDL_Thread* vh_thread;

    bool should_stop = false;
    bool running = false;
    bool buffer_processed = true;
    bool result_taken = true;
    VisualizationBuffer buffer;

    static int worker_thread (void * _self) {
    	VisualizationHandler* self = static_cast<VisualizationHandler*>(_self);
    	while (true) {
    		SDL_LockMutex(self->vh_mutex);
	    	self->running = true;
			if (self->should_stop) {
				printf("Exiting worker_thread.\n");
				self->running = false;
				SDL_UnlockMutex(self->vh_mutex);
				SDL_CondBroadcast(self->vh_cond);
				break;
			}

			SDL_CondBroadcast(self->vh_cond);
			SDL_CondWait(self->vh_cond, self->vh_mutex);
			if (self->buffer_processed == true || self->should_stop) {
				SDL_UnlockMutex(self->vh_mutex);
				continue;
			}

			self->visualize(self->buffer);
			self->buffer_processed = true;

	        SDL_UnlockMutex(self->vh_mutex);
	        SDL_CondBroadcast(self->vh_cond);
    	}
    	return 0;
	}


	virtual void visualize (VisualizationBuffer const&) = 0;
	virtual void get_result (float*) = 0;
	virtual void on_new_beat (double tempo_estimate) = 0;

protected:
    SDL_AudioSpec const& audio_spec;

public:
	DisplayParams const& display_params;
    std::deque<std::vector<float>> data_lookback_beats;

	void stop_thread () {
		// Send stop signal
		SDL_LockMutex(vh_mutex);
		should_stop = true;
		SDL_UnlockMutex(vh_mutex);
		SDL_CondBroadcast(vh_cond);

		// Await thread exit
		SDL_WaitThread(vh_thread, NULL);
	}

	virtual unsigned int get_result_size () = 0;

	virtual void process_ring_buffer (VisualizationBuffer const& data) final {
		SDL_CondBroadcast(vh_cond);

		SDL_LockMutex(vh_mutex);
		if (!buffer_processed) {
			SDL_UnlockMutex(vh_mutex);

			await_buffer_processed(false);
			while (result_taken == false) {
				SDL_CondWait(vh_cond, vh_mutex);
			}
		} else {
			buffer = data;
			buffer_processed = false;
			result_taken = false;
		}

        SDL_UnlockMutex(vh_mutex);
        SDL_CondBroadcast(vh_cond);
	}

	void handle_new_beat (double tempo_estimate) {
		await_buffer_processed(false);
		on_new_beat(tempo_estimate);
		SDL_UnlockMutex(vh_mutex);
	}

	void await_buffer_processed (bool unlock = true) {
		SDL_CondBroadcast(vh_cond);

		SDL_LockMutex(vh_mutex);
		while (buffer_processed == false) {
			SDL_CondBroadcast(vh_cond);
			SDL_CondWait(vh_cond, vh_mutex);
		}
		if (unlock) {
			SDL_UnlockMutex(vh_mutex);
		}
	}

	void await_result (float* result) {
		await_buffer_processed(false);
		get_result(result);
		result_taken = true;
		SDL_UnlockMutex(vh_mutex);
	}

	void unlock_mutex () {
		SDL_UnlockMutex(vh_mutex);
	}

	VisualizationHandler (SDL_AudioSpec const& audio_spec, DisplayParams const& display_params) :
		vh_mutex(SDL_CreateMutex()),
		vh_cond(SDL_CreateCond()),
		audio_spec(audio_spec),
		display_params(display_params)
	{
		vh_thread = SDL_CreateThread(&VisualizationHandler::worker_thread, "visualization worker", (void *) this);
	}

    ~VisualizationHandler () {
		SDL_LockMutex(vh_mutex);
		if (running) {
			SDL_UnlockMutex(vh_mutex);
			stop_thread();
			SDL_LockMutex(vh_mutex);
		}
        SDL_DestroyCond(vh_cond);
		SDL_UnlockMutex(vh_mutex);
        SDL_DestroyMutex(vh_mutex);
    }
};

#endif
