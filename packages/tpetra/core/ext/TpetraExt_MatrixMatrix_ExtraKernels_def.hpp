// @HEADER
// ***********************************************************************
//
//          Tpetra: Templated Linear Algebra Services Package
//                 Copyright (2008) Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Michael A. Heroux (maherou@sandia.gov)
//
// ************************************************************************
// @HEADER

#ifndef TPETRA_MATRIXMATRIX_EXTRAKERNELS_DEF_HPP
#define TPETRA_MATRIXMATRIX_EXTRAKERNELS_DEF_HPP
#include "TpetraExt_MatrixMatrix_ExtraKernels_decl.hpp"

namespace Tpetra {

namespace MatrixMatrix{

namespace MMDetails{


template<class CrsMatrixType>
size_t C_estimate_nnz_per_row(CrsMatrixType & A, CrsMatrixType &B){
  // Follows the NZ estimate in ML's ml_matmatmult.c
  size_t Aest = 100, Best=100;
  if (A.getNodeNumEntries() > 0)
    Aest = (A.getNodeNumRows() > 0)?  A.getNodeNumEntries()/A.getNodeNumRows() : 100;
  if (B.getNodeNumEntries() > 0)
    Best = (B.getNodeNumRows() > 0) ? B.getNodeNumEntries()/B.getNodeNumRows() : 100;

  size_t nnzperrow = (size_t)(sqrt((double)Aest) + sqrt((double)Best) - 1);
  nnzperrow *= nnzperrow;

  return nnzperrow;
}


#if defined (HAVE_TPETRA_INST_OPENMP)
/*********************************************************************************************************/
template<class Scalar,
         class LocalOrdinal,
         class GlobalOrdinal>
void mult_A_B_newmatrix_LowThreadGustavsonKernel(CrsMatrixStruct<Scalar, LocalOrdinal, GlobalOrdinal, Kokkos::Compat::KokkosOpenMPWrapperNode>& Aview,
                                                 CrsMatrixStruct<Scalar, LocalOrdinal, GlobalOrdinal, Kokkos::Compat::KokkosOpenMPWrapperNode>& Bview,
                                                 const Teuchos::Array<LocalOrdinal> & targetMapToOrigRow,
                                                 const Teuchos::Array<LocalOrdinal> & targetMapToImportRow,
                                                 const Teuchos::Array<LocalOrdinal> & Bcol2Ccol,
                                                 const Teuchos::Array<LocalOrdinal> & Icol2Ccol,
                                                 CrsMatrix<Scalar, LocalOrdinal, GlobalOrdinal, Kokkos::Compat::KokkosOpenMPWrapperNode>& C,
                                                 Teuchos::RCP<const Import<LocalOrdinal,GlobalOrdinal,Kokkos::Compat::KokkosOpenMPWrapperNode> > Cimport,
                                                 const std::string& label,
                                                 const Teuchos::RCP<Teuchos::ParameterList>& params) {
#ifdef HAVE_TPETRA_MMM_TIMINGS
  std::string prefix_mmm = std::string("TpetraExt ") + label + std::string(": ");
  using Teuchos::TimeMonitor;
  Teuchos::RCP<Teuchos::TimeMonitor> MM = rcp(new TimeMonitor(*TimeMonitor::getNewTimer(prefix_mmm + std::string("MMM Newmatrix LTGCore"))));
#endif

  using Teuchos::Array;
  using Teuchos::ArrayRCP;
  using Teuchos::ArrayView;
  using Teuchos::RCP;
  using Teuchos::rcp;


  // Lots and lots of typedefs
  typedef typename Kokkos::Compat::KokkosOpenMPWrapperNode Node;
  typedef typename Tpetra::CrsMatrix<Scalar,LocalOrdinal,GlobalOrdinal,Node>::local_matrix_type KCRS;
  //  typedef typename KCRS::device_type device_t;
  typedef typename KCRS::StaticCrsGraphType graph_t;
  typedef typename graph_t::row_map_type::non_const_type lno_view_t;
  typedef typename graph_t::entries_type::non_const_type lno_nnz_view_t;
  typedef typename KCRS::values_type::non_const_type scalar_view_t;
  typedef Scalar            SC;
  typedef LocalOrdinal      LO;
  typedef GlobalOrdinal     GO;
  typedef Node              NO;
  typedef Map<LO,GO,NO>                     map_type;


  // NOTE (mfh 15 Sep 2017) This is specifically only for
  // execution_space = Kokkos::OpenMP, so we neither need nor want
  // KOKKOS_LAMBDA (with its mandatory __device__ marking).
  typedef NO::execution_space execution_space;
  typedef Kokkos::RangePolicy<execution_space, size_t> range_type;


  // All of the invalid guys
  const size_t ST_INVALID = Teuchos::OrdinalTraits<LO>::invalid();
  const LO LO_INVALID = Teuchos::OrdinalTraits<LO>::invalid();
  const SC SC_ZERO = Teuchos::ScalarTraits<Scalar>::zero();
  const size_t INVALID = Teuchos::OrdinalTraits<size_t>::invalid();

  
  // Grab the  Kokkos::SparseCrsMatrices & inner cstuff
  const KCRS & Ak = Aview.origMatrix->getLocalMatrix();
  const KCRS & Bk = Bview.origMatrix->getLocalMatrix();
  const KCRS * Ik = Bview.importMatrix.is_null() ? 0 : Bview.importMatrix->getLocalMatrix();    

  const lno_view_t Arowptr = Ak.graph.row_map, Browptr = Bk.graph.row_map;
  const lno_nnz_view_t Acolind = Ak.graph.entries, Bcolind = Bk.graph.endtirs;
  const scalar_view_t Avals = Ak.values, Bvals = Bk.vals;

  const lno_view_t * Irowptr=0;
  const lno_nnz_view_t * Icolind=0;
  const scalar_view_t * Ivals=0;
  if(!Bview.importMatrix.is_null()) {
    Irowptr = &Ik->graph.row_map;
    Icolind = &Ik->graph.entries;
    Ivals   = &Ik->values;
  }

  // Sizes
  RCP<const map_type> Ccolmap = C.getColMap();
  size_t m = Aview.origMatrix->getNodeNumRows();
  size_t n = Ccolmap->getNodeNumElements();
  size_t Cest_nnz_per_row = C_estimate_nnz_per_row(Aview.origMatrix(),Bview.origMatrix());

  // Get my node / thread info (right from openmp)
  int thread_max = omp_get_num_threads();

  if(thread_max > 128) printf("WARNING: You probably don't want to run this kernel with %d threads\n",thread_max);

  // Thread-local memory
  std::vector<lno_view_t *> tl_rowptr(thread_max);
  std::vector<lno_nnz_view_t *> tl_colind(thread_max);
  std::vector<scalar_view_t *> tl_values(thread_max);

  double thread_chunk = (double)(m) / thread_max;

  // Run chunks of the matrix independently 
  Kokkos::parallel_for(range_type(0, thread_max),[=](const int tid) {
      // Thread coordiation stuff
      size_t my_thread_start = (size_t) tid * thread_chunk;
      size_t my_thread_stop  = tid == thread_max-1 ? m : (size_t)(tid+1)*thread_chunk;
      size_t my_thread_m     = my_thread_stop - my_thread_start;

      // Size estimate 
      size_t CSR_alloc = (size_t) (my_thread_m*Cest_nnz_per_row*0.75 + 100);

      // Allocations
      std::vector<size_t> c_status(n,INVALID);
      tl_rowptr[tid] = new lno_view_t("rowptr", my_thread_m+1);
      tl_colind[tid] = new lno_nnz_view_t("colind",CSR_alloc);
      tl_values[tid] = new scalar_view_t("values",CSR_alloc);      
      lno_view_t &Crowptr     = &*tl_rowptr[tid];
      lno_nnz_view_t &Ccolind = &*tl_colind[tid];
      scalar_view_t &Cvals    = &*tl_values[tid];

      // For each row of A/C
      size_t CSR_ip = 0, OLD_ip = 0;
      for (size_t i = my_thread_start; i < my_thread_stop; i++) {
        // mfh 27 Sep 2016: m is the number of rows in the input matrix A
        // on the calling process.
        Crowptr(i-my_thread_start) = CSR_ip;
        
        // mfh 27 Sep 2016: For each entry of A in the current row of A
        for (size_t k = Arowptr(i); k < Arowptr(i+1); k++) {
          LO Aik  = Acolind(k); // local column index of current entry of A
          const SC Aval = Avals(k);   // value of current entry of A
          if (Aval == SC_ZERO)
            continue; // skip explicitly stored zero values in A
          
          if (targetMapToOrigRow[Aik] != LO_INVALID) {
            // mfh 27 Sep 2016: If the entry of targetMapToOrigRow
            // corresponding to the current entry of A is populated, then
            // the corresponding row of B is in B_local (i.e., it lives on
            // the calling process).
            
            // Local matrix
            size_t Bk = Teuchos::as<size_t>(targetMapToOrigRow[Aik]);
            
            // mfh 27 Sep 2016: Go through all entries in that row of B_local.
            for (size_t j = Browptr(Bk); j < Browptr(Bk+1); ++j) {
              LO Bkj = Bcolind(j);
              LO Cij = Bcol2Ccol[Bkj];
              
              if (c_status[Cij] == INVALID || c_status[Cij] < OLD_ip) {
                // New entry
                c_status[Cij]   = CSR_ip;
                Ccolind(CSR_ip) = Cij;
                Cvals(CSR_ip)   = Aval*Bvals(j);
                CSR_ip++;
                
              } else {
                Cvals(c_status[Cij]) += Aval*Bvals(j);
              }
            }

          } else {
            // mfh 27 Sep 2016: If the entry of targetMapToOrigRow
            // corresponding to the current entry of A NOT populated (has
            // a flag "invalid" value), then the corresponding row of B is
            // in B_local (i.e., it lives on the calling process).
            
            // Remote matrix
            size_t Ik = Teuchos::as<size_t>(targetMapToImportRow[Aik]);
            for (size_t j = *Irowptr(Ik); j < *Irowptr(Ik+1); ++j) {
              LO Ikj = Icolind(j);
              LO Cij = Icol2Ccol(Ikj);
              
              if (c_status[Cij] == INVALID || c_status[Cij] < OLD_ip){
                // New entry
                c_status[Cij]   = CSR_ip;
                Ccolind(CSR_ip) = Cij;
                Cvals(CSR_ip)   = Aval*Ivals(j);
                CSR_ip++;
                
              } else {
                Cvals(c_status[Cij]) += Aval*Ivals(j);
              }
            }
          }
        }
        
        // Resize for next pass if needed
        if (CSR_ip + n > CSR_alloc) {
          CSR_alloc *= 2;
          Kokkos::resize(*tl_rowptr[tid],CSR_alloc);
          Kokkos::resize(*tl_values[tid],CSR_alloc);
        }
        OLD_ip = CSR_ip;
      }
      
      Crowptr(my_thread_m) = CSR_ip;
    });

  // Generate the starting nnz number per thread
  size_t c_nnz_size=0;
  lno_view_t row_mapC("non_const_lnow_row", m + 1);
  lno_view_t thread_start_nnz("thread_nnz",thread_max);
  Kokkos::parallel_scan(range_type(0,thread_max), [=] (const size_t i, size_t& update, const bool final) {
      size_t mynnz = (*tl_rowptr[i])[tl_rowptr[i]->dimension(0)];
      if(final) thread_start_nnz(i) = update;
      update+=mynnz;
    });

  // Allocate output
  lno_nnz_view_t  entriesC(Kokkos::ViewAllocateWithoutInitializing("entriesC"), c_nnz_size);
  scalar_view_t   valuesC(Kokkos::ViewAllocateWithoutInitializing("entriesC"), c_nnz_size);

  // Copy out
  Kokkos::parallel_for(range_type (0, m),[=](const size_t i) {
      size_t tid = i / thread_chunk;
      size_t ii = i- tid * thread_chunk;
      size_t thread_start = thread_start_nnz(tid);

      // Rowptr
      if(ii==0) row_mapC(i) = thread_start;
      else if (i==m-1) {
        row_mapC(i) = thread_start + (*tl_rowptr[tid])[ii-1];
        row_mapC(m) = thread_start + (*tl_rowptr[tid])[ii];
      }
      else row_mapC(i) = thread_start + (*tl_rowptr[tid])[ii-1];
        

      
      // Colind / Values
      for(int j = (*tl_rowptr[tid])[ii]; j<(*tl_rowptr[tid])[ii+1]; j++) {
        entriesC(thread_start + j) = (*tl_colind[tid])(j);
        valuesC(thread_start + j)  = (*tl_values[tid])(j);        
      }


    });

  // FINISH ME                    


#ifdef HAVE_TPETRA_MMM_TIMINGS
  MM = rcp(new TimeMonitor (*TimeMonitor::getNewTimer(prefix_mmm + std::string("MMM Newmatrix LTGSort"))));
#endif

  // Sort & set values
  Import_Util::sortCrsEntries(row_mapC, entriesC, valuesC);
  C.setAllValues(row_mapC,entriesC,valuesC);

#ifdef HAVE_TPETRA_MMM_TIMINGS
  MM = rcp(new TimeMonitor (*TimeMonitor::getNewTimer(prefix_mmm + std::string("MMM Newmatrix LTGESFC"))));
#endif

  // Final Fillcomplete
  RCP<Teuchos::ParameterList> labelList = rcp(new Teuchos::ParameterList);
  labelList->set("Timer Label",label);
  RCP<const Export<LocalOrdinal,GlobalOrdinal,Kokkos::Compat::KokkosOpenMPWrapperNode> > dummyExport;
  C.expertStaticFillComplete(Bview.origMatrix->getDomainMap(), Aview.origMatrix->getRangeMap(), Cimport,dummyExport,labelList);


}

#endif // OpenMP


}//MMDetails
}//MatrixMatirx
}//Tpetra
                        

#endif
