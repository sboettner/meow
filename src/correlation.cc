#include <fftw3.h>
#include "correlation.h"


class CorrelationService:public ICorrelationService {
    fftw_plan fwdplan1;
    fftw_plan fwdplan2;
    fftw_plan invplan;

    double* srcbuf1;
    double* srcbuf2;
    double* resultbuf;

    int     length;

public:
    CorrelationService(int length);
    virtual ~CorrelationService();

    virtual void run(const float* in1, const float* in2, float* out) override;
};


ICorrelationService::~ICorrelationService()
{
}


CorrelationService::CorrelationService(int length):length(length)
{
    srcbuf1=(double*) fftw_malloc(2*length*sizeof(double));
    srcbuf2=(double*) fftw_malloc(2*length*sizeof(double));
    resultbuf=(double*) fftw_malloc(2*length*sizeof(double));

    fwdplan1=fftw_plan_r2r_1d(length*2, srcbuf1, srcbuf1, FFTW_R2HC, FFTW_ESTIMATE);
    fwdplan2=fftw_plan_r2r_1d(length*2, srcbuf2, srcbuf2, FFTW_R2HC, FFTW_ESTIMATE);
    invplan =fftw_plan_r2r_1d(length*2, resultbuf, resultbuf, FFTW_HC2R, FFTW_ESTIMATE);
}


CorrelationService::~CorrelationService()
{
    fftw_destroy_plan(fwdplan1);
    fftw_destroy_plan(fwdplan2);
    fftw_destroy_plan(invplan);

    fftw_free(srcbuf1);
    fftw_free(srcbuf2);
    fftw_free(resultbuf);
}


void CorrelationService::run(const float* in1, const float* in2, float* out)
{
    for (int i=0;i<length;i++) {
        srcbuf1[i]=in1[i];
        srcbuf2[i]=in2[length-i-1];
        srcbuf1[i+length]=srcbuf2[i+length]=0.0;
    }

    fftw_execute(fwdplan1);
    fftw_execute(fwdplan2);

    resultbuf[0     ]=srcbuf1[     0]*srcbuf2[     0];
    resultbuf[length]=srcbuf1[length]*srcbuf2[length];

    for (int i=1;i<length;i++) {
        resultbuf[         i]=srcbuf1[i]*srcbuf2[i]          - srcbuf1[2*length-i]*srcbuf2[2*length-i];
        resultbuf[2*length-i]=srcbuf1[i]*srcbuf2[2*length-i] + srcbuf1[2*length-i]*srcbuf2[i];
    }

    fftw_execute(invplan);

    for (int i=0;i<length;i++)
        out[i]=float(resultbuf[i] / (2*length));
}


ICorrelationService* ICorrelationService::create(int length)
{
    return new CorrelationService(length);
}
