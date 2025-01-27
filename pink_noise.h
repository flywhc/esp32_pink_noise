#ifndef PINK_NOISE_H
#define PINK_NOISE_H

// pink noise generator parameters
const int NUM_PINK_BINS = 16;
float pink_bins[NUM_PINK_BINS] = {0};
int pink_index = 0;

// generate random number between -1 and 1
float random_float() {
    return ((float)rand() / RAND_MAX) * 2.0f - 1.0f; // white noise
}

// generate pink noise sample
float generate_pink_noise() {
    float white = random_float();
    pink_bins[pink_index] = white;
    
    float pink = 0;
    int index = pink_index;
    
    // calculate pink noise
    for (int i = 0; i < NUM_PINK_BINS; i++) {
        pink += pink_bins[index];
        index = (index - 1 + NUM_PINK_BINS) % NUM_PINK_BINS;
    }
    
    pink_index = (pink_index + 1) % NUM_PINK_BINS;
    pink = pink / NUM_PINK_BINS;
    
    return pink;
}

#define NUM_RANDOMS 16 // size of random value array for pink noise

// Paul Kellet chatgpt
class PinkNoiseFilterV2 {
private:
    float coefficients[7] = {0.99765f, 0.96300f, 0.57000f}; // filter parameters
    float states[7] = {0};                                  // filter states
    float white;                                            // white noise input

public:
    float generateSample() {
        white = random_float();
        // filter update
        states[0] = coefficients[0] * (states[0] + white);
        states[1] = coefficients[1] * (states[1] + white);
        states[2] = coefficients[2] * (states[2] + white);
        states[3] = white * 0.1848f; // low frequency weighting
        return (states[0] + states[1] + states[2] + states[3]) * 0.1848f; // adjust volume
    }
};

// brown noise generation (implemented by integrating white noise)
class BrownNoiseGenerator {
private:
    float lastValue = 0.0f; // save previous sample value
    const float limit = 1.0f;

public:
    float generateSample() {
        float white = random_float();
        lastValue += white * 0.05f;
        if (lastValue > limit) lastValue = limit;
        if (lastValue < -limit) lastValue = -limit;
        return lastValue;
    }
};

PinkNoiseFilterV2 pinkNoiseFilterV2;
BrownNoiseGenerator brownNoiseGenerator;

#endif 