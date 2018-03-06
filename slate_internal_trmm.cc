//------------------------------------------------------------------------------
// Copyright (c) 2017, University of Tennessee
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the University of Tennessee nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL UNIVERSITY OF TENNESSEE BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//------------------------------------------------------------------------------
// This research was supported by the Exascale Computing Project (17-SC-20-SC),
// a collaborative effort of two U.S. Department of Energy organizations (Office
// of Science and the National Nuclear Security Administration) responsible for
// the planning and preparation of a capable exascale ecosystem, including
// software, applications, hardware, advanced system engineering and early
// testbed platforms, in support of the nation's exascale computing imperative.
//------------------------------------------------------------------------------
// Need assistance with the SLATE software? Join the "SLATE User" Google group
// by going to https://groups.google.com/a/icl.utk.edu/forum/#!forum/slate-user
// and clicking "Apply to join group". Upon acceptance, email your questions and
// comments to <slate-user@icl.utk.edu>.
//------------------------------------------------------------------------------

#include "slate_Matrix.hh"
#include "slate_types.hh"
#include "slate_Tile_blas.hh"
#include "slate_internal.hh"

namespace slate {
namespace internal {

///-----------------------------------------------------------------------------
/// \brief
/// Triangular matrix multiply.
/// Dispatches to target implementations.
template <Target target, typename scalar_t>
void trmm(Side side, Diag diag,
          scalar_t alpha, TriangularMatrix< scalar_t >&& A,
                                    Matrix< scalar_t >&& B,
          int priority)
{
    trmm(internal::TargetType<target>(),
         side, diag,
         alpha, A,
                B,
         priority);
}

///-----------------------------------------------------------------------------
/// \brief
/// Triangular matrix multiply.
/// Host OpenMP task implementation.
template <typename scalar_t>
void trmm(internal::TargetType<Target::HostTask>,
          Side side, Diag diag,
          scalar_t alpha, TriangularMatrix< scalar_t >& A,
                                    Matrix< scalar_t >& B,
          int priority)
{
    assert(A.mt() == 1);
    assert(A.nt() == B.mt());

    for (int64_t n = 0; n < B.nt(); ++n)
        #pragma omp task shared(A, B)
        {
            trmm(side, diag,
                 alpha, A(0, A.nt()-1),
                        B(B.mt()-1, n));
        }

    for (int64_t k = A.nt()-2; k >= 0; --k)
        for (int64_t n = 0; n < B.nt(); ++n)
            #pragma omp task shared(A, B)
            {
                gemm(alpha,         A(0, k),
                                    B(k, n),
                     scalar_t(1.0), B(B.mt()-1, n));
            }

    #pragma omp taskwait
}

//------------------------------------------------------------------------------
// Explicit instantiations.
// ----------------------------------------
template
void trmm< Target::HostTask, float >(
    Side side, Diag diag,
    float alpha, TriangularMatrix< float >&& A,
                           Matrix< float >&& B,
    int priority);

// ----------------------------------------
template
void trmm< Target::HostTask, double >(
    Side side, Diag diag,
    double alpha, TriangularMatrix< double >&& A,
                            Matrix< double >&& B,
    int priority);

// ----------------------------------------
template
void trmm< Target::HostTask, std::complex<float> >(
    Side side, Diag diag,
    std::complex<float> alpha, TriangularMatrix< std::complex<float> >&& A,
                                         Matrix< std::complex<float> >&& B,
    int priority);

// ----------------------------------------
template
void trmm< Target::HostTask, std::complex<double> >(
    Side side, Diag diag,
    std::complex<double> alpha, TriangularMatrix< std::complex<double> >&& A,
                                          Matrix< std::complex<double> >&& B,
    int priority);

} // namespace internal
} // namespace slate
