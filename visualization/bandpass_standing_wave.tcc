#include "vis_handler.tcc"
#include <stdexcept>

enum BPSW_Phase { Constant, Unchanged, Standing };

struct BPSW_Spec {
	size_t win_length_samples; // Window length in samples
	size_t update_length_samples; // Update length in samples
	bool win_window_fn; // Apply window function
	bool adaptive_crop;

	double* fft_freq_weighing = NULL; // abs(fft(window)) weighing
	double fft_dispersion; // arg(fft(window)) freq. dependent weighing
	BPSW_Phase fft_phase; // Type of phase manipulation for inverse trafo
	double fft_phase_const; // for fft_phase == BPSW_Phase.Constant constant phase value

	size_t crop_length_samples; // Displayed length of the window
	size_t crop_offset; // Display window offset

	unsigned int c_center_x, c_center_y; // Circle center location
	double c_rad_base, c_rad_extr; // Radius base and extrusion scaling
	float color_inner[4]; // Inner color of the circle
};

class BandpassStandingWave : public VisualizationHandler {
private:
	RollingWindow<double> rollingWindow;
	FFTHandler fftHandler;
	double* result;
	bool const should_weigh = false;

	SDL_Point* points;

	void visualize (VisualizationBuffer const& data) {

		// Update the rolling window and
		size_t index_last = rollingWindow.current_index();
		double* const window_data = rollingWindow.update(data.audio_buffer, VisualizationHandler::spec.samples, data.is_new_beat);

		if (data.is_new_beat & params.adaptive_crop) {
			double beat_period_sec = 60 / data.tempo_estimate;
			int beat_period_samples = round(spec.freq * beat_period_sec);
			// int multiples = 12 * params.win_length_samples / (beat_period_samples * 4);
			params.crop_length_samples = std::min(((int) params.win_length_samples), beat_period_samples);
			delete[] result;
			result = new double[params.crop_length_samples];
			std::cout << "Setting output size to " << params.crop_length_samples << " samples" << std::endl;
		}

		// Execute fourier transformation
		memcpy(fftHandler.real, window_data, params.win_length_samples * sizeof(double));
	    fftHandler.exec_r2c();

	    // Convert to polar basis
	    const size_t c_length = params.win_length_samples / 2 + 1;
	    double* abs_vals = new double[c_length];
	    double* arg_vals = new double[c_length];
	    for (size_t i = 0; i < c_length; i++) {
	        std::complex<double> c(fftHandler.complex[i][0], fftHandler.complex[i][1]);
	        abs_vals[i] = std::abs(c);
	        arg_vals[i] = std::arg(c);
	    }

	    // Transform polar frequency spectrum
	    for (size_t i = 0; i < c_length; i++) {
	        double bin_phase = 2 * M_PI * (index_last / ((double) params.win_length_samples));
	        double phase_offset = 2 * M_PI * (params.fft_phase_const / ((double) params.win_length_samples));
	        double abs_weighted = abs_vals[i] * params.fft_freq_weighing[i];

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

	        fftHandler.complex[i][0] = std::real(c);
	        fftHandler.complex[i][1] = std::imag(c);
	    }
	    delete[] abs_vals;
	    delete[] arg_vals;

	    // Execute inverse fourier transformation
	    fftHandler.exec_c2r();

	    for (size_t i = 0; i < params.crop_length_samples; i++) {
	    	// Scaling is not preserved: irfft(rfft(x))[i] = x[i] * len(x)
	    	result[i] = fftHandler.real[params.crop_offset + i] / params.win_length_samples;
	    }
	}

	void get_result (float* output) {
		for (size_t i = 0; i < params.crop_length_samples; i++) {
			output[i] = result[i];
		}
	}

	unsigned int get_result_size() {
		return params.crop_length_samples;
	}

public:
	BPSW_Spec& params;

	BandpassStandingWave (SDL_AudioSpec const& spec, BPSW_Spec& params) :
		VisualizationHandler(spec),
		params(params),
		rollingWindow(params.win_length_samples, 0, params.win_window_fn),
		fftHandler(params.win_length_samples),
		should_weigh(params.fft_freq_weighing != NULL)
	{
		result = new double[params.crop_length_samples];
		std::cout << "Initializer list finised\n";
		// assert(params.win_length_samples >= (params.crop_length_samples + params.crop_offset),
		std::cout << "Initilizing BPSW with win_length_samples=" << params.win_length_samples << " and crop_length_samples=" << params.crop_length_samples << std::endl;

		if (spec.samples > params.win_length_samples) {
			throw std::invalid_argument("Window cannot be shorter than samples per update");
		}
	}

	~BandpassStandingWave () {
		delete[] result;
	}
};
