#include <cmath>
#include <stdexcept>

namespace math {

	template <typename T>
	class ExpFilter {
	private:
		T* values;
		size_t size;
		double alpha_rise;
		double alpha_decay;

	public:
		ExpFilter (size_t size, double alpha_rise, double alpha_decay, T const& initial_value) :
			size(size), alpha_rise(alpha_rise), alpha_decay(alpha_decay)
		{
			if (alpha_decay < 0.0 or alpha_decay > 1.0
				or alpha_rise < 0.0 or alpha_rise > 1.0) {
				throw std::invalid_argument("Alpha values for ExpFilter not in [0.0, 1.0]");
			}
			values = new T[size];
			for (size_t i = 0; i < size; i++) {
				values[i] = initial_value;
			}
		}

		~ExpFilter () {
			delete[] values;
		}

		T* update (T* new_values) {
			for (size_t i = 0; i < size; i++) {
				double alpha = new_values[i] > values[i] ? alpha_rise : alpha_decay;
				values[i] = alpha * new_values[i] + (1.0 - alpha) * values[i];
			}
			return values;
		}
	};

	template <typename T>
	T max_value (T* const values, size_t len) {
		T max = values[0];
		for (size_t i = 1; i < len; i++) {
			max = values[i] > max ? values[i] : max;
		}
		return max;
	}

	template <typename T>
	size_t max_value_arg (T* const values, size_t len) {
		T max = values[0];
		size_t arg = 0;
		for (size_t i = 1; i < len; i++) {
			if (values[i] > max) {
				max = values[i];
				arg = i;
			}
		}
		return arg;
	}

	template <typename T>
	T min_value (T* const values, size_t len) {
		T min = values[0];
		for (size_t i = 1; i < len; i++) {
			min = values[i] < min ? values[i] : min;
		}
		return min;
	}

	template <typename T>
	void lin_space (T* values, size_t len, T start, T stop, bool periodic = false, T step = 1) {
		T const norm_step = step * (stop - start) / ((double) len - (periodic ? 1 : 0));
		for (size_t i = 0; i < len; i++) {
			values[i] = start + i * norm_step;
		}
	}
} // namespace math