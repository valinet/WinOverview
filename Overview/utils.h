#pragma once

double linear(double percent, double elapsed, double start, double end, double total) {
    return start + (end - start) * percent;
}

// p = t / d
double easeOutQuad(double p) {
    double m = p - 1; return 1 - m * m;
};