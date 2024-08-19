#pragma once

class IProgressMonitor {
public:
    virtual void report(double progress) = 0;
};
