//  Small test program showing how to locate off-process GIDs
//  Trying to use Tpetra::Map like the Zoltan DDirectory
//
//  Bug in Tpetra::Map?  Filed as Bug 6412
//  Behavior differs between default Tpetra Map distribution
//  and user-defined distribution when there are duplicate search entries.

#include "Teuchos_DefaultComm.hpp"
#include "Teuchos_RCP.hpp"
#include "Teuchos_ArrayRCP.hpp"
#include "Tpetra_Map.hpp"
#include <string>
#include <iostream>

using Teuchos::arcp;
typedef int lno_t;
typedef int gno_t;
typedef Tpetra::Map<lno_t, gno_t> map_t;

/////////////////////////////////////////////////////////////////////
void searchIt(const map_t &myMap, const std::string &myName)
{
  int me = myMap.getComm()->getRank();

  // Print the map elements
  std::cout << me << " " << myName << " MINE: ";
  for (size_t i = 0; i < myMap.getNodeNumElements(); i++)
    std::cout << myMap.getGlobalElement(i) << " ";
  std::cout << std::endl;
 
  // Memory for Gids for which to search
  size_t nSearch = 6; 
  Teuchos::ArrayRCP<gno_t> searchGids = arcp(new gno_t[nSearch],
                                             0, nSearch, true);
  Teuchos::ArrayRCP<int> searchRemoteRanks = arcp(new int[nSearch],
                                                  0, nSearch, true);
  Teuchos::ArrayRCP<lno_t> searchRemoteLids = arcp(new lno_t[nSearch],
                                                   0, nSearch, true);

  // Search without duplicates
  for (size_t i = 0; i < nSearch; i++) searchGids[i] = i;
  myMap.getRemoteIndexList(searchGids(),
                           searchRemoteRanks(), searchRemoteLids());

  for (size_t i = 0; i < nSearch; i++)
    std::cout << me << " " << myName 
                    << " NoDuplicates:  GID " << searchGids[i]
                    << " RANK " << searchRemoteRanks[i] 
                    << " LID " << searchRemoteLids[i]
                    << (searchRemoteRanks[i] == -1 ? "  BAD!" : " ")
                    << std::endl;

  // Search with duplicates
  for (size_t i = 0; i < nSearch; i++) searchGids[i] = i/2;
  myMap.getRemoteIndexList(searchGids(),
                           searchRemoteRanks(), searchRemoteLids());

  for (size_t i = 0; i < nSearch; i++)
    std::cout << me << " " << myName 
                    << " WithDuplicates:  GID " << searchGids[i]
                    << " RANK " << searchRemoteRanks[i] 
                    << " LID " << searchRemoteLids[i]
                    << (searchRemoteRanks[i] == -1 ? "  BAD!" : " ")
                    << std::endl;
}


/////////////////////////////////////////////////////////////////////

int main(int narg, char **arg)
{
  // Usual Teuchos MPI stuff
  Teuchos::GlobalMPISession mpiSession(&narg,&arg);
  Teuchos::RCP<const Teuchos::Comm<int> >
  comm = Teuchos::DefaultComm<int>::getComm();
  int me = comm->getRank();
  int np = comm->getSize();

  gno_t nGlobal = 24;   // Global number of Gids

  // Create and search Default Tpetra Map
  const map_t defaultMap(nGlobal, 0, comm);

  searchIt(defaultMap, "defaultMap");

  // Create and seach customized map
  // Identify locally owned GIDs:  same as default map (if nGlobal%np == 0)
  lno_t nLocal = nGlobal / np + (me < (nGlobal%np));
  gno_t myFirst = me * (nGlobal / np) + (me < (nGlobal%np) ? me : (nGlobal%np));
  Teuchos::ArrayRCP<gno_t> myGids = arcp(new gno_t[nLocal], 0, nLocal, true);
  for (lno_t i = 0; i < nLocal; i++)
    myGids[i] = myFirst + i;

  // Construct customMap
  gno_t dummy = Teuchos::OrdinalTraits<gno_t>::invalid();
  const map_t customMap(dummy, myGids(), 0, comm);

  searchIt(customMap, "customMap");

  return 0;
}
