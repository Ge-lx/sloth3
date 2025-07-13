#include <complex>
#include <iostream>
#include <stdexcept>

#include "vis_handler.tcc"
#include "../util/rolling_window.tcc"
#include "../util/fft_handler.h"
#include "../util/math.tcc"

enum BPSW_Phase { Constant, Unchanged, Standing };

struct BPSW_Spec {
	size_t win_length_samples; // Window length in samples
	size_t update_length_samples; // Update length in samples
	bool win_window_fn; // Apply window function
	bool adaptive_crop;

	bool use_filter;
	double f_cutoff;
	bool is_lowpass;

	double fft_dispersion; // arg(fft(window)) freq. dependent weighing
	BPSW_Phase fft_phase; // Type of phase manipulation for inverse trafo
	double fft_phase_const; // for fft_phase == BPSW_Phase.Constant constant phase value

	size_t crop_length_samples; // Displayed length of the window
	size_t crop_offset; // Display window offset

	DisplayParams display_params;
};

class BandpassStandingWave : public VisualizationHandler {
private:
	RollingWindow<double>* rollingWindow = NULL;
	FFTHandler* fftHandler = NULL;
	double* result = NULL;
	double* freq_weights = NULL;

	void visualize (VisualizationBuffer const& data) {

		if (data.is_new_beat) std::cout << "&fftHandler: " << fftHandler << std::endl;

		// Update the rolling window and
		size_t index_last = rollingWindow->current_index();
		double* const window_data = rollingWindow->update(data.audio_buffer, audio_spec.samples, data.is_new_beat);
		// index_last -= rollingWindow->last_update_length() / 2.0;
		// Execute fourier transformation
		memcpy(fftHandler->real, window_data, params.win_length_samples * sizeof(double));
		fftHandler->exec_r2c();

	    // Convert to polar basis
	    const size_t c_length = params.win_length_samples / 2 + 1;
	    double* abs_vals = new double[c_length]; // Allocation inside hot path. Refactor into class members.
	    double* arg_vals = new double[c_length];
	    for (size_t i = 0; i < c_length; i++) {
	        std::complex<double> c(fftHandler->complex[i][0], fftHandler->complex[i][1]);
	        abs_vals[i] = std::abs(c);
	        arg_vals[i] = std::arg(c);
	    }

	    // Transform polar frequency spectrum
	    for (size_t i = 0; i < c_length; i++) {
	    	const double should_weigh = (freq_weights != NULL);
	        double abs_weighted = should_weigh ? abs_vals[i] * freq_weights[i] : abs_vals[i];
	        double bin_phase = 2 * M_PI * (index_last / ((double) params.win_length_samples));
	        double phase_offset = 2 * M_PI * (params.fft_phase_const / ((double) params.win_length_samples));

	        double arg_shifted = 0;
	        switch (params.fft_phase) {
	        	case BPSW_Phase::Unchanged:
	        		arg_shifted = arg_vals[i] + params.fft_dispersion * bin_phase;
	        		break;
	        	case BPSW_Phase::Constant:
	        		arg_shifted = i * i * params.fft_dispersion;
	        		break;
	        	case BPSW_Phase::Standing:
	        		arg_shifted = arg_vals[i] - (i + params.fft_dispersion) * (bin_phase + phase_offset);
	        		break;
	        }
	        std::complex<double> c = std::polar(abs_weighted, arg_shifted);

	        fftHandler->complex[i][0] = std::real(c);
	        fftHandler->complex[i][1] = std::imag(c);
	    }
	    delete[] abs_vals; // See above. Allocation in hot path
	    delete[] arg_vals;

	    // Execute inverse fourier transformation
	    fftHandler->exec_c2r();

	    for (size_t i = 0; i < params.crop_length_samples; i++) {
	    	// Scaling is not preserved: irfft(rfft(x))[i] = x[i] * len(x)
	    	result[i] = fftHandler->real[params.crop_offset + i] / params.win_length_samples;
	    }
	}

	void get_result (float* output) {
		for (size_t i = 0; i < params.crop_length_samples; i++) {
			output[i] = result[i];
		}
	}

	void on_new_beat (double tempo_estimate) {
		if (params.adaptive_crop) {
			double beat_period_sec = 60 / tempo_estimate;
			size_t beat_period_samples = round(audio_spec.freq * beat_period_sec / 4.0);

			if (beat_period_samples == params.win_length_samples) {
				return;
			}

			allocate_for_window_length(beat_period_samples);
		}
	}

	void allocate_for_window_length (size_t window_length) {
		params.win_length_samples = window_length;
		params.crop_length_samples = window_length;

		bool has_prev_window = (rollingWindow != NULL);
		size_t prev_index = has_prev_window ? rollingWindow->current_index() : 0;

		if (rollingWindow != NULL) delete rollingWindow;
		if (fftHandler != NULL) delete fftHandler;
		if (result != NULL) delete[] result;
		if (freq_weights != NULL) delete[] freq_weights;

		rollingWindow = new RollingWindow<double>(params.win_length_samples, 0, params.win_window_fn);
		fftHandler = new FFTHandler(params.win_length_samples);
		result = new double[params.win_length_samples];

		if (params.use_filter) {
			const size_t c_length = params.win_length_samples / 2 + 1;

			double* freq_bins = new double[c_length];
			math::freqs_for_dft_r2c(freq_bins, c_length, (size_t) audio_spec.freq);

			freq_weights = new double[c_length];
			for (size_t i = 0; i < c_length; i++) {
				const bool above = freq_bins[i] > params.f_cutoff;
				freq_weights[i] = (above ^ params.is_lowpass) ? 1 : 0;
			}

			delete[] freq_bins;
		}

		rollingWindow->index = prev_index;
	}

	unsigned int get_result_size() {
		return params.crop_length_samples;
	}

public:
	BPSW_Spec& params;

	BandpassStandingWave (SDL_AudioSpec const& audio_spec, BPSW_Spec& params) :
		VisualizationHandler(audio_spec, params.display_params),
		params(params)
	{
		allocate_for_window_length(params.win_length_samples);

		std::cout << "Initilizing BPSW with win_length_samples=" << params.win_length_samples << std::endl;

		if (audio_spec.samples > params.win_length_samples) {
			throw std::invalid_argument("Window cannot be shorter than samples per update");
		}
	}

	~BandpassStandingWave () {
		if (rollingWindow != NULL) delete rollingWindow;
		if (fftHandler != NULL) delete fftHandler;
		if (result != NULL) delete[] result;
		if (freq_weights != NULL) delete[] freq_weights;
	}
};
