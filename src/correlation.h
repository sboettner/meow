#pragma once

class ICorrelationService {
public:
    virtual ~ICorrelationService();
    virtual void run(const float* in1, const float* in2, float* out) = 0;

    static ICorrelationService* create(int length);
};
