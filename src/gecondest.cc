// Copyright (c) 2017-2023, University of Tennessee. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
// This program is free software: you can redistribute it and/or modify it under
// the terms of the BSD 3-Clause license. See the accompanying LICENSE file.

#include "slate/slate.hh"
#include "auxiliary/Debug.hh"
#include "slate/Matrix.hh"
#include "internal/internal.hh"

namespace slate {

//------------------------------------------------------------------------------
/// Distributed parallel estimate of the reciprocal of the condition number
/// of a general matrix A, in either the 1-norm or the infinity-norm.
/// Generic implementation for any target.
///
/// The reciprocal of the condition number computed as:
/// \[
///     rcond = \frac{1}{\|\|A\|\| \times \|\|A^{-1}\|\-}
/// \]
/// where $A$ is the output of the LU factorization (getrf).
///
//------------------------------------------------------------------------------
/// @tparam scalar_t
///     One of float, double, std::complex<float>, std::complex<double>.
//------------------------------------------------------------------------------
/// @param[in] in_norm
///     rcond to compute:
///     - Norm::One: 1-norm condition number
///     - Norm::Inf: infinity-norm condition number
///
/// @param[in] A
///     On entry, the n-by-n matrix $A$.
///     It is the output of the LU factorization of a general matrix.
///
/// @param[in] Anorm
///     If Norm::One, the 1-norm of the original matrix A.
///     If Norm::Inf, the infinity-norm of the original matrix A.
///
/// @param[in] opts
///     Additional options, as map of name = value pairs. Possible options:
///     - Option::Target:
///       Implementation to target. Possible values:
///       - HostTask:  OpenMP tasks on CPU host [default].
///       - HostNest:  nested OpenMP parallel for loop on CPU host.
///       - HostBatch: batched BLAS on CPU host.
///       - Devices:   batched BLAS on GPU device.
///
/// @return rcond
///     The reciprocal of the condition number of the matrix A,
///     computed as stated above.
///     rcond is used instead of cond to handle overflow better.
///
/// @ingroup cond_specialization
///
template <typename scalar_t>
blas::real_type<scalar_t> gecondest(
    Norm in_norm,
    Matrix<scalar_t>& A,
    blas::real_type<scalar_t> Anorm,
    Options const& opts)
{
    using blas::real;
    using real_t = blas::real_type<scalar_t>;

    int kase, kase1;
    if (in_norm == Norm::One) {
        kase1 = 1;
    }
    else if (in_norm == Norm::Inf) {
        kase1 = 2;
    }
    else {
        slate_error("invalid norm.");
    }
    if (Anorm < 0 || std::isnan( Anorm )) {
        slate_error( "invalid Anorm" );
    }

    int64_t m = A.m();

    // Quick return
    if (m <= 1) {
        return 1.;
    }
    else if (Anorm == 0.) {
        return 0.;
    }

    scalar_t alpha = 1.;
    real_t Ainvnorm = 0.0;

    std::vector<int64_t> isave = {0, 0, 0, 0};

    auto tileMb = A.tileMbFunc();
    auto tileNb = func::uniform_blocksize(1, 1);
    auto tileRank = A.tileRankFunc();
    auto tileDevice = A.tileDeviceFunc();
    slate::Matrix<scalar_t> X (m, 1, tileMb, tileNb,
                               tileRank, tileDevice, A.mpiComm());
    X.insertLocalTiles(Target::Host);
    slate::Matrix<scalar_t> V (m, 1, tileMb, tileNb,
                               tileRank, tileDevice, A.mpiComm());
    V.insertLocalTiles(Target::Host);
    slate::Matrix<int64_t> isgn (m, 1, tileMb, tileNb,
                                 tileRank, tileDevice, A.mpiComm());
    isgn.insertLocalTiles(Target::Host);

    auto L  = TriangularMatrix<scalar_t>(
        Uplo::Lower, slate::Diag::Unit, A );
    auto U  = TriangularMatrix<scalar_t>(
        Uplo::Upper, slate::Diag::NonUnit, A );

    // initial and final value of kase is 0
    kase = 0;
    internal::norm1est( X, V, isgn, &Ainvnorm, &kase, isave );

    MPI_Bcast( &isave[0], 4, MPI_INT64_T, X.tileRank(0, 0), A.mpiComm() );
    MPI_Bcast( &kase, 1, MPI_INT, X.tileRank(0, 0), A.mpiComm() );

    while (kase != 0) {
        if (kase == kase1) {
            // Multiply by inv(L).
            slate::trsm( Side::Left, alpha, L, X, opts );

            // Multiply by inv(U).
            slate::trsm( Side::Left, alpha, U, X, opts );
        }
        else {
            // Multiply by inv(U^H).
            auto UH = conj_transpose( U );
            slate::trsm( Side::Left, alpha, UH, X, opts );

            // Multiply by inv(L^H).
            auto LH = conj_transpose( L );
            slate::trsm( Side::Left, alpha, LH, X, opts );
        }

        internal::norm1est( X, V, isgn, &Ainvnorm, &kase, isave );
        MPI_Bcast( &isave[0], 4, MPI_INT64_T, X.tileRank(0, 0), A.mpiComm() );
        MPI_Bcast( &kase, 1, MPI_INT, X.tileRank(0, 0), A.mpiComm() );
    } // while (kase != 0)

    // Compute the estimate of the reciprocal condition number.
    if (Ainvnorm != 0.0) {
        return (1.0 / Ainvnorm) / Anorm;
    }
    else {
        return 0.;
    }
}

//------------------------------------------------------------------------------
// Explicit instantiations.
template
float gecondest<float>(
    Norm in_norm,
    Matrix<float>& A,
    float Anorm,
    Options const& opts);

template
double gecondest<double>(
    Norm in_norm,
    Matrix<double>& A,
    double Anorm,
    Options const& opts);

template
float gecondest< std::complex<float> >(
    Norm in_norm,
    Matrix< std::complex<float> >& A,
    float Anorm,
    Options const& opts);

template
double gecondest< std::complex<double> >(
    Norm in_norm,
    Matrix< std::complex<double> >& A,
    double Anorm,
    Options const& opts);

} // namespace slate
