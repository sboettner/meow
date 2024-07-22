#pragma once

class IRegressionService {
public:
    virtual ~IRegressionService();

    virtual float run(const float* x, const float* y, const float* w, float* coeffs) = 0;

    static IRegressionService* create(int order, int numpoints);
};
