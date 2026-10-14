#pragma once

#include <QVector>
#include <complex>
#include <cmath>

namespace Kalorite {

class AudioSpectrumAnalyzer {
public:
    static void fft(QVector<std::complex<double>>& a) {
        int n = a.size();
        if (n <= 1) return;

        QVector<std::complex<double>> a0(n / 2), a1(n / 2);
        for (int i = 0; 2 * i < n; i++) {
            a0[i] = a[2 * i];
            a1[i] = a[2 * i + 1];
        }
        fft(a0);
        fft(a1);

        double ang = 2 * M_PI / n;
        std::complex<double> w(1), wn(std::cos(ang), -std::sin(ang));
        for (int i = 0; 2 * i < n; i++) {
            a[i] = a0[i] + w * a1[i];
            a[i + n / 2] = a0[i] - w * a1[i];
            w *= wn;
        }
    }

    static QVector<double> analyzeSamples(const std::vector<float>& samples, int channels = 2, int numBands = 20, double sampleRate = 44100.0) {
        QVector<double> bands(numBands, 0.0);
        if (samples.empty()) return bands;

        const int FFT_SIZE = 1024;
        QVector<std::complex<double>> input(FFT_SIZE, 0.0);

        int step = channels <= 0 ? 1 : channels;
        int sampleCount = samples.size();
        int limit = std::min(FFT_SIZE, sampleCount / step);

        for (int i = 0; i < limit; ++i) {
            double sum = 0;
            for (int c = 0; c < step; ++c) {
                sum += samples[i * step + c];
            }
            double val = sum / step;
            double window = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (limit - 1)));
            input[i] = val * window;
        }

        // Run FFT
        fft(input);

        // Map FFT magnitudes to logarithmic bands
        double minFreq = 20.0;
        double maxFreq = 20000.0;
        if (sampleRate <= 0) sampleRate = 44100.0;

        for (int b = 0; b < numBands; ++b) {
            double fLow = minFreq * std::pow(maxFreq / minFreq, (double)b / numBands);
            double fHigh = minFreq * std::pow(maxFreq / minFreq, (double)(b + 1) / numBands);

            int binLow = std::max(0, (int)(fLow * FFT_SIZE / sampleRate));
            int binHigh = std::min(FFT_SIZE / 2 - 1, (int)(fHigh * FFT_SIZE / sampleRate));
            if (binHigh < binLow) binHigh = binLow;

            double sumMag = 0.0;
            int count = 0;
            for (int bin = binLow; bin <= binHigh; ++bin) {
                sumMag += std::abs(input[bin]);
                count++;
            }
            double avgMag = (count > 0) ? (sumMag / count) : 0.0;

            // See analyzeBuffer(): normalize by transform size + dB floor so the
            // bars don't all peg at the top on loud material.
            double mag = avgMag / (FFT_SIZE / 2.0);
            const double floorDb = -70.0;
            double dbVal = 20.0 * std::log10(mag + 1e-6);
            double norm = (dbVal - floorDb) / (0.0 - floorDb);
            if (norm < 0.0) norm = 0.0;
            if (norm > 1.0) norm = 1.0;
            bands[b] = norm;
        }

        return bands;
    }
};

} // namespace Kalorite
