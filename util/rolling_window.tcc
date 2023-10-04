#include <cstring>

template <typename SampleT>
class RollingWindow {
private:
	static constexpr size_t sample_bytes = sizeof(SampleT);
	size_t window_length_samples;
	SampleT* data;

	bool window;
	SampleT* windowed;

	size_t last_update_samples;
    size_t index = 0;

public:
	SampleT* window_function;

	RollingWindow (size_t window_length_samples, SampleT const default_value, bool window = false) :
	window_length_samples(window_length_samples), window(window) {
		data = new SampleT[window_length_samples];
	    window_function = new SampleT[window_length_samples];
	    windowed = new SampleT[window_length_samples];

		for (size_t i = 0; i < window_length_samples; i++) {
			data[i] = default_value;
			windowed[i] = default_value;
		}

	    for (size_t i = 0; i < window_length_samples; i++) {
	        window_function[i] = (.5 * (1 - std::cos(2*M_PI*i)/(window_length_samples)));
	    }
	}

	~RollingWindow () {
		delete[] data;
		delete[] window_function;
		delete[] windowed;
	}

	SampleT* update (SampleT const* update, size_t update_length, bool is_new_beat) {
		const static size_t window_len_bytes = window_length_samples * sample_bytes;
		const size_t update_len_bytes = update_length * sample_bytes;

		index = is_new_beat ? update_length : index + update_length;
		// index %= window_length_samples;
		last_update_samples = update_length;

		// Shift the existing data update_len_bytes bytes towands end
		memmove(data, data + update_length, window_len_bytes - update_len_bytes);
		// Add the new data to the front
		memcpy(data + window_length_samples - update_length, update, update_len_bytes);

		if (window) {
			for (size_t i = 0; i < window_length_samples; i++) {
				windowed[i] = window_function[i] * data[i];
			}

			return windowed;
		} else {
			return data;
		}
	}

	size_t window_length () const {
		return window_length_samples;
	}

	size_t last_update_length () const {
		return last_update_samples;
	}

	size_t current_index () const {
		return index;
	}

	void reset_index () {
		index = window_length_samples;
	}
};