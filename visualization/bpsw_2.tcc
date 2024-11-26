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

    // Crossover-frequency target between standing (zero-phase)
    // and unchanged phase representation
    size_t f_xover;

	double c_rad_base, c_rad_extr; // Radius base and extrusion scaling
	float color_inner[4]; // Inner color of the circle
};

class BPSW2 : public VisualizationHandler {
private:
	RollingWindow<double> rollingWindow;
	FFTHandler fftHandler;

	double* result;
	double* abs_vals;
	double* freq_bins;

    size_t c_length;
	bool const should_weigh = false;

	void visualize (VisualizationBuffer const& data) {

		// Update the rolling window
		double* const window_data = rollingWindow.update(data.audio_buffer, audio_spec.samples, false);
        size_t idx_pad_end = (params.n_fft - params.n_w) / 2;

        std::fill(fftHandler.real, fftHandler.real + params.n_fft, double(0));
        memcpy(fftHandler.real + idx_pad_end, window_data, params.n_w * sizeof(double));
	    fftHandler.exec_r2c();

		std::complex<double>* data_complex = reinterpret_cast<std::complex<double>*>(fftHandler.complex);

		// const double bin_phase = params.n_fft / 2 / audio_spec.freq; // index_last / ((double) params.n_w);

		size_t index_last = rollingWindow.current_index();
		const double samples_shift = ((index_last % params.n_w) /* / ((double)params.n_fft)) */);

		// for (size_t i = 0; i < c_length; i++) {
		// 	abs_vals[i] = std::abs(data_complex[i]);
		// }
		// size_t idx_max = math::max_value_arg(abs_vals, c_length);

		for (size_t i = 0; i < c_length/20; i++) {
			using namespace std::complex_literals;
			using namespace std::numbers;
			double zero_offset = 0.25 + params.n_fft / 2 * (freq_bins[i] / audio_spec.freq);
			double bin_phase = zero_offset; //- samples_shift * (freq_bins[i] / audio_spec.freq); /* + 0.001 */;
			// data_complex[i] *= std::exp(2i * pi * freq_bins[i]);
			// data_complex[i] *= std::exp(-2i * pi * bin_phase);
			data_complex[i] = std::abs(data_complex[i]) * std::exp(2i * ((pi * bin_phase)/*  + std::arg(data_complex[i]) */));
		}

		// if (data.is_new_beat & params.adaptive_crop) {
		// 	double beat_period_sec = 60 / data.tempo_estimate;
		// 	int beat_period_samples = round(audio_spec.freq * beat_period_sec);
		// 	params.crop_length_samples = std::min(((int) params.win_length_samples), beat_period_samples);
		// 	delete[] result;
		// 	result = new double[params.crop_length_samples];
		// 	std::cout << "Setting output size to " << params.crop_length_samples << " samples" << std::endl;
		// }

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
		c_length(params.n_fft / 2 + 1),
		should_weigh(false),
		params(params)
	{
		result = new double[params.n_fft];
		freq_bins = new double[c_length];
		abs_vals = new double[c_length];

		math::freqs_for_dft_r2c(freq_bins, params.n_fft, audio_spec.freq);

		std::cout << "Initilizing BPSW2 with n_w=" << params.n_w << " and n_fft=" << params.n_fft << std::endl;
		if (audio_spec.samples > params.n_w) {
			throw std::invalid_argument("Window cannot be shorter than samples per update");
		}
	}

	~BPSW2 () {
		delete[] result;
		delete[] freq_bins;
		delete[] abs_vals;
	}
};
