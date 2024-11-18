#include "vis_handler.tcc"
#include <stdexcept>
#include <algorithm>
#include <complex>
#include <cmath>

// enum BPSW_Phase { Constant, Unchanged, Standing };

struct BPSW2_Spec {
	size_t n_w; // Window length in samples
	size_t n_hop; // Update length in samples
    size_t n_fft; // FFT size in samples (also result size)
	// bool win_window_fn; // Apply window function
	// bool adaptive_crop;

	// double* fft_freq_weighing = NULL; // abs(fft(window)) weighing
	// double fft_dispersion; // arg(fft(window)) freq. dependent weighing
	// BPSW_Phase fft_phase; // Type of phase manipulation for inverse trafo
	// double fft_phase_const; // for fft_phase == BPSW_Phase.Constant constant phase value

	// size_t crop_length_samples; // Displayed length of the window
	// size_t crop_offset; // Display window offset

	double c_rad_base, c_rad_extr; // Radius base and extrusion scaling
	float color_inner[4]; // Inner color of the circle
};

class BPSW2 : public VisualizationHandler {
private:
	RollingWindow<double> rollingWindow;
	FFTHandler fftHandler;
	double* result;
	bool const should_weigh = false;

	SDL_Point* points;

	void visualize (VisualizationBuffer const& data) {

		// Update the rolling window and
		double* const window_data = rollingWindow.update(data.audio_buffer, audio_spec.samples, false);
		size_t index_last = rollingWindow.current_index();

        size_t idx_pad_end = (params.n_fft - params.n_w) / 2;
        std::fill(fftHandler.real, fftHandler.real + params.n_fft, double(0));
        memcpy(fftHandler.real + idx_pad_end, window_data, params.n_w * sizeof(double));
	    fftHandler.exec_r2c();

		std::complex<double>* data_complex = reinterpret_cast<std::complex<double>*>(fftHandler.complex);

	    const size_t c_length = params.n_fft / 2 + 1;
		const double pi = std::acos(-1.0);
		// const double bin_phase = params.n_fft / 2 / audio_spec.freq; // index_last / ((double) params.n_w);

		const double samples_shift = ((index_last % params.n_w) /* / ((double)params.n_fft)) */);
		double* freq_bins = new double[c_length];
		double* abs_vals = new double[c_length];
		math::freqs_for_dft_r2c(freq_bins, params.n_fft, audio_spec.freq);
		for (size_t i = 0; i < c_length; i++) {
			abs_vals[i] = std::abs(data_complex[i]);
		}
		size_t idx_max = math::max_value_arg(abs_vals, c_length);
		for (size_t i = 0; i < c_length/20; i++) {
			using namespace std::complex_literals;
			double zero_offset = 0.25 + params.n_fft / 2 * (freq_bins[i] / audio_spec.freq);
			double bin_phase = zero_offset; //- samples_shift * (freq_bins[i] / audio_spec.freq); /* + 0.001 */;
			// data_complex[i] *= std::exp(2i * pi * freq_bins[i]);
			// data_complex[i] *= std::exp(-2i * pi * bin_phase);
			data_complex[i] = std::abs(data_complex[i]) * std::exp(2i * ((pi * bin_phase)/*  + std::arg(data_complex[i]) */));
		}

		delete[] freq_bins;
		delete[] abs_vals;
		// if (data.is_new_beat & params.adaptive_crop) {
		// 	double beat_period_sec = 60 / data.tempo_estimate;
		// 	int beat_period_samples = round(audio_spec.freq * beat_period_sec);
		// 	params.crop_length_samples = std::min(((int) params.win_length_samples), beat_period_samples);
		// 	delete[] result;
		// 	result = new double[params.crop_length_samples];
		// 	std::cout << "Setting output size to " << params.crop_length_samples << " samples" << std::endl;
		// }

		// Execute fourier transformation
		// memcpy(fftHandler.real, window_data, params.n_w * sizeof(double));

	    // Convert to polar basis
	    // double* abs_vals = new double[c_length]; // Allocation inside hot path. Refactor into class members.
	    // double* arg_vals = new double[c_length];
	    // for (size_t i = 0; i < c_length; i++) {
	    //     std::complex<double> c(fftHandler.complex[i][0], fftHandler.complex[i][1]);
	    //     abs_vals[i] = std::abs(c);
	    //     arg_vals[i] = std::arg(c);
	    // }

	    // Transform polar frequency spectrum
	    // for (size_t i = 0; i < c_length; i++) {
	    //     double abs_weighted = abs_vals[i] * params.fft_freq_weighing[i];
	    //     double bin_phase = 2 * M_PI * (index_last / ((double) params.win_length_samples));
	    //     double phase_offset = 2 * M_PI * (params.fft_phase_const / ((double) params.win_length_samples));

	    //     double arg_shifted = 0;
	    //     switch (params.fft_phase) {
	    //     	case BPSW_Phase::Unchanged:
	    //     		arg_shifted = arg_vals[i] + params.fft_dispersion * bin_phase;
	    //     		break;
	    //     	case BPSW_Phase::Constant:
	    //     		arg_shifted = i * i * params.fft_dispersion;
	    //     		break;
	    //     	case BPSW_Phase::Standing:
	    //     		arg_shifted = arg_vals[i] - (i + params.fft_dispersion) * (bin_phase + phase_offset);
	    //     		break;
	    //     }
	    //     std::complex<double> c = std::polar(abs_weighted, arg_shifted);

	    //     fftHandler.complex[i][0] = std::real(c);
	    //     fftHandler.complex[i][1] = std::imag(c);
	    // }
	    // delete[] abs_vals; // See above. Allocation in hot path
	    // delete[] arg_vals;

	    // Execute inverse fourier transformation
	    fftHandler.exec_c2r();

	    for (size_t i = 0; i < params.n_fft; i++) {
	    	// Scaling is not preserved: irfft(rfft(x))[i] = x[i] * len(x)
	    	result[i] = fftHandler.real[i] / params.n_w;
	    }
	}

	void get_result (float* output) {
		for (size_t i = 0; i < params.n_fft; i++) {
			output[i] = result[i];
		}
	}

	unsigned int get_result_size() {
		return params.n_fft;
	}

public:
	BPSW2_Spec& params;
    std::deque<std::vector<float>> data_lookback_beats;

	BPSW2 (SDL_AudioSpec const& audio_spec, BPSW2_Spec& params) :
		VisualizationHandler(audio_spec),
		rollingWindow(params.n_w, 0, true),
		fftHandler(params.n_fft),
		should_weigh(false),
		params(params)
	{
		result = new double[params.n_fft];
		std::cout << "Initializer list finised\n";
		// assert(params.win_length_samples >= (params.crop_length_samples + params.crop_offset),
		std::cout << "Initilizing BPSW2 with n_w=" << params.n_w << " and n_fft=" << params.n_fft << std::endl;

		if (audio_spec.samples > params.n_w) {
			throw std::invalid_argument("Window cannot be shorter than samples per update");
		}
	}

	~BPSW2 () {
		delete[] result;
	}
};
