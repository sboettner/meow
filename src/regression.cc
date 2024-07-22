#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_multifit.h>
#include "regression.h"

namespace gsl {

class Vector {
    gsl_vector* vec;

public:
    Vector(int size);
    ~Vector();

    struct entry_proxy {
        gsl_vector* vec;
        int         i;

        operator double()
        {
            return gsl_vector_get(vec, i);
        }

        void operator=(double v)
        {
            gsl_vector_set(vec, i, v);
        }
    };

    entry_proxy operator[](int i)
    {
        return entry_proxy { vec, i };
    }

    operator gsl_vector*()
    {
        return vec;
    }
};


Vector::Vector(int size)
{
    vec=gsl_vector_alloc(size);
}


Vector::~Vector()
{
    gsl_vector_free(vec);
    vec=nullptr;
}


class Matrix {
    gsl_matrix* mat;

public:
    Matrix(int rows, int cols);
    ~Matrix();

    struct entry_proxy {
        gsl_matrix* mat;
        int         i, j;

        operator double()
        {
            return gsl_matrix_get(mat, i, j);
        }

        void operator=(double v)
        {
            gsl_matrix_set(mat, i, j, v);
        }
    };

    entry_proxy operator()(int i, int j)
    {
        return entry_proxy { mat, i, j };
    }

    operator gsl_matrix*()
    {
        return mat;
    }
};


Matrix::Matrix(int rows, int cols)
{
    mat=gsl_matrix_alloc(rows, cols);
}


Matrix::~Matrix()
{
    gsl_matrix_free(mat);
    mat=nullptr;
}

}


class RegressionService:public IRegressionService {
    int order;
    int numpoints;

    gsl::Matrix A;
    gsl::Vector Y;
    gsl::Vector W;
    gsl::Vector C;
    gsl::Matrix cov;

    gsl_multifit_linear_workspace*  work;

public:
    RegressionService(int order, int numpoints);
    virtual ~RegressionService();

    virtual float run(const float* x, const float* y, const float* w, float* coeffs) override;
};


IRegressionService::~IRegressionService()
{
}


IRegressionService* IRegressionService::create(int order, int numpoints)
{
    return new RegressionService(order, numpoints);
}


RegressionService::RegressionService(int order, int numpoints):
    numpoints(numpoints),
    order(order),
    A(numpoints, order),
    Y(numpoints),
    W(numpoints),
    C(order),
    cov(order, order)
{
    work=gsl_multifit_linear_alloc(numpoints, order);
}


RegressionService::~RegressionService()
{
    gsl_multifit_linear_free(work);
    work=nullptr;
}


float RegressionService::run(const float* x, const float* y, const float* w, float* coeffs)
{
    for (int i=0;i<numpoints;i++) {
        float f=1.0f;

        for (int j=0;j<order;j++) {
            A(i, j)=f;
            f*=x[i];
        }

        Y[i]=y[i];
        W[i]=w[i];
    }

    double residual=0.0;
    int result=gsl_multifit_wlinear(A, W, Y, C, cov, &residual, work);

    for (int i=0;i<order;i++)
        coeffs[i]=float(C[i]);

    return float(residual);
}
