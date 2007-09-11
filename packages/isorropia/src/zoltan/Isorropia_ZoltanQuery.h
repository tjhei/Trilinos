//@HEADER
// ***********************************************************************
// 
//            Isorropia: Partitioning and Load Balancing Package
//              Copyright (2006) Sandia Corporation
// 
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
// 
// This library is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of the
// License, or (at your option) any later version.
//  
// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//  
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA
//Questions? Contact Alan Williams (william@sandia.gov)
//                or Erik Boman    (egboman@sandia.gov)
// 
// ***********************************************************************
//@HEADER

#ifndef ISORROPIA_ZOLTANQUERY_H
#define ISORROPIA_ZOLTANQUERY_H

#include "Isorropia_ConfigDefs.hpp"

#include <IZoltan_QueryObject.h>
#include <Teuchos_RefCountPtr.hpp>

#include <vector>
#include <set>
#include <map>

class Epetra_BlockMap;
class Epetra_CrsGraph;
class Epetra_RowMatrix;

namespace Isorropia {
  class CostDescriber;

///
/** Query helper object to be used for Zoltan partitioning/ordering.
 * This object allows Zoltan to query an Epetra_CrsGraph object for it's
 * partitioning/ordering algorithms
 */
class ZoltanQuery : public Zoltan::QueryObject
{
  Teuchos::RefCountPtr<const Epetra_RowMatrix> matrix_;
  Teuchos::RefCountPtr<const Epetra_RowMatrix> tmatrix_;
  Teuchos::RefCountPtr<const Epetra_CrsGraph> graph_;
  Teuchos::RefCountPtr<const Epetra_CrsGraph> tgraph_;
  const Epetra_BlockMap* map_;
  const Epetra_BlockMap* tmap_;
  Teuchos::RefCountPtr<const Isorropia::CostDescriber> costs_;

  std::map<int,int> procmap_;

  std::vector< std::set<int> > ugraph_;
  std::vector<int> iwork_;
  std::vector<float> fwork_;
  int worklen_;

  const bool localEdgesOnly_;

  void fill_procmap(const Epetra_CrsGraph* graph,
		    const Epetra_CrsGraph* tgraph);
  void fill_procmap(const Epetra_RowMatrix* matrix,
		    const Epetra_RowMatrix* tmatrix);

  void fill_ugraph(const Epetra_CrsGraph* graph,
		   const Epetra_CrsGraph* tgraph);
  void fill_ugraph(const Epetra_RowMatrix* matrix,
		   const Epetra_RowMatrix* tmatrix);

 public:

  /** Constructor
   */
  ZoltanQuery( Teuchos::RefCountPtr<const Epetra_CrsGraph> graph,
               Teuchos::RefCountPtr<const Epetra_CrsGraph> tgraph,
               bool localEdgesOnly = false );

  /** Constructor
   */
  ZoltanQuery( Teuchos::RefCountPtr<const Epetra_RowMatrix> matrix,
               Teuchos::RefCountPtr<const Epetra_RowMatrix> tmatrix,
               bool localEdgesOnly = false );

  /** Constructor
   */
  ZoltanQuery( Teuchos::RefCountPtr<const Epetra_CrsGraph> graph,
               Teuchos::RefCountPtr<const Epetra_CrsGraph> tgraph,
	       Teuchos::RefCountPtr<const Isorropia::CostDescriber> costs,
               bool localEdgesOnly = false );

  /** Constructor
   */
  ZoltanQuery( Teuchos::RefCountPtr<const Epetra_RowMatrix> matrix,
               Teuchos::RefCountPtr<const Epetra_RowMatrix> tmatrix,
	       Teuchos::RefCountPtr<const Isorropia::CostDescriber> costs,
               bool localEdgesOnly = false );

  /** Destructor
   */
  virtual ~ZoltanQuery();

  const Epetra_BlockMap& RowMap() { return *map_ ; }
  bool haveVertexWeights() const;
  bool haveGraphEdgeWeights() const;
  bool haveHypergraphEdgeWeights() const;

  //General Functions
  int Number_Objects  ( void * data,
                        int * ierr );

  void Object_List    ( void * data,
                        int num_gid_entries,
                        int num_lid_entries,
                        ZOLTAN_ID_PTR global_ids,
                        ZOLTAN_ID_PTR local_ids,
                        int obj_weight_dim,
                        float * object_weights,
                        int * ierr );

  //Graph Based Functions
  int Number_Edges    ( void * data,
                        int num_gid_entries,
                        int num_lid_entries,
                        ZOLTAN_ID_PTR global_id,
                        ZOLTAN_ID_PTR local_id,
                        int * ierr );
  
  void Edge_List      ( void * data,
                        int num_gid_entries,
                        int num_lid_entries,
                        ZOLTAN_ID_PTR global_id,
                        ZOLTAN_ID_PTR local_id,
                        ZOLTAN_ID_PTR neighbor_global_ids,
                        int * neighbor_procs,
                        int edge_weight_dim,
                        float * edge_weights,
                        int * ierr );
  
  //HyperGraph Functions
  void HG_Size_CS ( void * data,
                    int* num_lists,
                    int* num_pins,
                    int* format,
                    int * ierr );

  void HG_CS ( void * data,
               int num_gid_entries,
               int num_row_or_col,
               int num_pins,
               int format,
               ZOLTAN_ID_PTR vtxedge_GID,
               int* vtxedge_ptr,
               ZOLTAN_ID_PTR pin_GID,
               int * ierr );

  void HG_Size_Edge_Weights(void * data,
			    int* num_edges,
			    int* ierr);

  void HG_Edge_Weights(void * data,
		       int num_gid_entries,
		       int num_lid_entries,
		       int num_edges,
		       int edge_weight_dim,
		       ZOLTAN_ID_PTR edge_GID,
		       ZOLTAN_ID_PTR edge_LID,
		       float* edge_weights,
		       int* ierr);
};

} //namespace Isorropia

#endif //ISORROPIA_ZOLTANQUERY_H

