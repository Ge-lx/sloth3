#include <fftw3.h>

#ifndef FFT_HANDLER_H
#define FFT_HANDLER_H

class FFTHandler {
private:
    volatile fftw_plan plan_r2c;
    volatile fftw_plan plan_c2r;
    size_t n;

public:
    fftw_complex* complex;
    double* real;

    FFTHandler (size_t n);
    ~FFTHandler ();

    void exec_r2c ();
    void exec_c2r ();
};

void init_fftw ();

#endif