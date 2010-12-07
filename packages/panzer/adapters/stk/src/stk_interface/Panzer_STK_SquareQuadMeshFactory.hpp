#ifndef __Panzer_STK_SquareQuadMeshFactory_hpp__
#define __Panzer_STK_SquareQuadMeshFactory_hpp__

#include <Panzer_STK_MeshFactory.hpp>
#include <Panzer_STK_Interface.hpp>

namespace panzer_stk {

class STK_Interface;

/** This builds a parallel mesh object. Note that the
  * local IDs are ordered by going up the y axis and
  * across the X-axis (in that order). For a mesh with
  * two X blocks and one Y-block, with each block composed
  * of 3x2 elements the numbering looks like: 
  \verbatim
   8  9 10 11
   4  5  6  7
   0  1  2  3 
  \endverbatim
  */
class SquareQuadMeshFactory : public STK_MeshFactory {
public:
   //! Constructor
   SquareQuadMeshFactory();

   //! Destructor
   ~SquareQuadMeshFactory();

   //! Build the mesh object
   Teuchos::RCP<STK_Interface> buildMesh(stk::ParallelMachine parallelMach) const;

   virtual Teuchos::RCP<STK_Interface> buildUncommitedMesh(stk::ParallelMachine parallelMach) const;
   virtual void completeMeshConstruction(STK_Interface & mesh,stk::ParallelMachine parallelMach) const;

   //! From ParameterListAcceptor
   void setParameterList(const Teuchos::RCP<Teuchos::ParameterList> & paramList);

   //! From ParameterListAcceptor
   Teuchos::RCP<const Teuchos::ParameterList> getValidParameters() const;

protected: 
   void initializeWithDefaults();

   void buildMetaData(stk::ParallelMachine parallelMach,STK_Interface & mesh) const;
   void buildElements(stk::ParallelMachine parallelMach,STK_Interface & mesh) const;
   void buildBlock(stk::ParallelMachine machRank,int xBlock,int yBlock,STK_Interface & mesh) const;

   std::pair<int,int> determineXElemSizeAndStart(int xBlock,unsigned int size,unsigned int rank) const;
   std::pair<int,int> determineYElemSizeAndStart(int yBlock,unsigned int size,unsigned int rank) const;

   void addSideSets(STK_Interface & mesh) const;

   // search through relations for the one matching the ID
   const stk::mesh::Relation * getRelationByID(unsigned ID,stk::mesh::PairIterRelation edges) const;

   double x0_, y0_;
   double xf_, yf_;

   int xBlocks_, yBlocks_;

   int nXElems_, nYElems_;

   mutable unsigned int machRank_, machSize_;
};

}

#endif
