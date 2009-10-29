/*********************************************************************/
/* File:   hcurlhdivfes.cpp                                          */
/* Author: Joachim Schoeberl                                         */
/* Date:   12. Jan. 2002                                             */
/*********************************************************************/

/* 
   Finite Element Space
*/

#include <comp.hpp>
#include <multigrid.hpp>


#ifdef PARALLEL
#include <parallelngs.hpp>
#endif

using namespace ngmg; 

#ifdef PARALLEL
extern MPI_Comm MPI_HIGHORDER_COMM;
#endif

namespace ngcomp
{
  using namespace ngcomp;

#ifdef PARALLEL
  using namespace ngparallel;
#endif

  // Nedelec FE Space

#ifdef OLD
  // old style constructor
  NedelecFESpace :: NedelecFESpace (const MeshAccess & ama,
				    int aorder, int adim, bool acomplex, bool adiscontinuous)
    
    : FESpace (ama, aorder, adim, acomplex),
      discontinuous(adiscontinuous)
  {
    name="NedelecFESpace(hcurl)";
    
    tet     = new FE_NedelecTet1;
    prism   = new FE_NedelecPrism1;
    pyramid = new FE_NedelecPyramid1;
    trig    = new FE_NedelecTrig1;
    quad    = new FE_NedelecQuad1;
    segm    = new FE_NedelecSegm1;
    hex     = new FE_NedelecHex1; 

    // prol = CreateVecObject<EdgeProlongation, Prolongation> (dimension, iscomplex, *this); 
    //    CreateVecObject1(prol, EdgeProlongation, dimension, iscomplex, *this);
    prol = new EdgeProlongation (*this);
    order = 1;


    // Evaluator for shape tester 
    static ConstantCoefficientFunction one(1);
    if (ma.GetDimension() == 2)
      {
	Array<CoefficientFunction*> coeffs(1);
	coeffs[0] = &one;
	evaluator = GetIntegrators().CreateBFI("massedge", 2, coeffs);
      }
    else if(ma.GetDimension() == 3) 
      {
	Array<CoefficientFunction*> coeffs(1); 
	coeffs[0] = &one;
	evaluator = GetIntegrators().CreateBFI("massedge",3,coeffs); 
	boundary_evaluator = GetIntegrators().CreateBFI("robinedge",3,coeffs); 
	
      }

#ifdef PARALLEL
    paralleldofs = 0;
#endif
  }
#endif

  
  // new style constructor
  NedelecFESpace :: NedelecFESpace (const MeshAccess & ama, const Flags& flags, bool parseflags)
    : FESpace (ama, flags)
  {
    name="NedelecFESpace(hcurl)";
    DefineDefineFlag("hcurl");
    // parse standard flags
    if(parseflags) CheckFlags(flags);
    
    if( flags.GetDefineFlag("hcurl"))
      cerr << "WARNING: -hcurl flag is deprecated: use -type=hcurl instead" << endl;
    
    tet     = new FE_NedelecTet1;
    prism   = new FE_NedelecPrism1;
    pyramid = new FE_NedelecPyramid1;
    trig    = new FE_NedelecTrig1;
    quad    = new FE_NedelecQuad1;
    segm    = new FE_NedelecSegm1;
    hex     = new FE_NedelecHex1; 

    prol = new EdgeProlongation (*this);
    order = 1;

    // Evaluator for shape tester 
    static ConstantCoefficientFunction one(1);
    if (ma.GetDimension() == 2)
      {
	Array<CoefficientFunction*> coeffs(1);
	coeffs[0] = &one;
	evaluator = GetIntegrators().CreateBFI("massedge", 2, coeffs);
      }
    else if(ma.GetDimension() == 3) 
      {
	Array<CoefficientFunction*> coeffs(1); 
	coeffs[0] = &one;
	evaluator = GetIntegrators().CreateBFI("massedge",3,coeffs); 
	boundary_evaluator = GetIntegrators().CreateBFI("robinedge",3,coeffs); 
	
      }

    discontinuous = flags.GetDefineFlag("discontinuous");
#ifdef PARALLEL
    paralleldofs = 0;
#endif
  }



                                    
  NedelecFESpace :: ~NedelecFESpace ()
  {
    ;
  }


  FESpace * NedelecFESpace :: Create (const MeshAccess & ma, const Flags & flags)
  {
    int order = int(flags.GetNumFlag ("order", 1)); 

#ifdef PARALLEL
    if (order >= 2)
      {
	NedelecFESpace2 * ned2fespace = new NedelecFESpace2 ( ma, flags, true );
	ned2fespace -> ResetParallelDofs ();
	return ned2fespace;
      }
    else
      {
        NedelecFESpace * nedfespace = new NedelecFESpace ( ma, flags, true );
	nedfespace -> ResetParallelDofs ();
	return nedfespace;
      }
#else

    if (order >= 2) 
      return new NedelecFESpace2 (ma, flags, true);
    else
      return new NedelecFESpace (ma, flags, true);
#endif

  }



  
  void NedelecFESpace :: Update(LocalHeap & lh)
  {

    int i, j, k;
    const MeshAccess & ma = GetMeshAccess();
    int ne = ma.GetNE();
    int nse = ma.GetNSE();
    // int np = ma.GetNP();
    int ned = ma.GetNEdges();
    
    Array<int> pnums, enums, eorient;
    
    
    int level = ma.GetNLevels();
    
#ifdef PARALLEL
    if ( level == nelevel.Size() )
      {
	UpdateParallelDofs();
	return;
      }
#else
    if (level == nelevel.Size())
      return;
#endif
    
    nelevel.Append (ned);
    

    for(int i=0; i<specialelements.Size(); i++)
      delete specialelements[i];
    specialelements.DeleteAll();

    // new implementation of finelevelofedge - array:
    
    int oldned = finelevelofedge.Size();
    finelevelofedge.SetSize(ned);
    for (i = oldned; i < ned; i++)
      finelevelofedge[i] = -1;
    
    for (i = 0; i < ne; i++)
      {
	ma.GetElEdges (i, enums, eorient);
	for (j = 0; j < enums.Size(); j++)
	  finelevelofedge[enums[j]] = level-1;
      }

    for (i = 0; i < nse; i++)
      {
	ma.GetSElEdges (i, enums, eorient);
	for (j = 0; j < enums.Size(); j++)
	  finelevelofedge[enums[j]] = level-1;
      }
  

    // generate edge points, and temporary hash table
    // HashTable<INT<2>, int> * node2edge = new HashTable<INT<2>, int>(ned/2+1);
    ClosedHashTable<INT<2>, int> * node2edge = new ClosedHashTable<INT<2>, int>(5*ned+10);

    edgepoints.SetSize(0);
    
    for (i = 0; i < ned; i++)
      {
	INT<2> edge;
	ma.GetEdgePNums (i, edge[0], edge[1]);
	int edgedir = (edge[0] > edge[1]);
	if (edgedir) Swap (edge[0], edge[1]);
	node2edge -> Set (edge, i);
	edgepoints.Append (edge);
      }


    
    // build edge hierarchy:
    parentedges.SetSize (ned);
    for (i = 0; i < ned; i++)
      {
	parentedges[i][0] = -1;
	parentedges[i][1] = -1;
      }
    

    for (i = 0; i < ned; i++)
      {
	INT<2> i2 (edgepoints[i][0], edgepoints[i][1]);
	int pa1[2], pa2[2];
	ma.GetParentNodes (i2[0], pa1);
	ma.GetParentNodes (i2[1], pa2);
	
	if (pa1[0] == -1 && pa2[0] == -1)
	  continue;
	
	int issplitedge = 0;
	if (pa1[0] == i2[1] || pa1[1] == i2[1])
	  issplitedge = 1;
	if (pa2[0] == i2[0] || pa2[1] == i2[0])
	  issplitedge = 2;
	
	if (issplitedge)
	  {
	    // edge is obtained by splitting one edge into two parts:
	    INT<2> paedge;
	    if (issplitedge == 1)
	      paedge = INT<2> (pa1[0], pa1[1]);
	    else
	      paedge = INT<2> (pa2[0], pa2[1]);
	    
	    if (paedge[0] > paedge[1]) 
	      Swap (paedge[0], paedge[1]);
	    
	    int paedgenr = node2edge->Get (paedge);
	    int orient = (paedge[0] == i2[0] || paedge[1] == i2[1]) ? 1 : 0;
	    
	    parentedges[i][0] = 2 * paedgenr + orient;
	  }
	else
	  {
	    // edge is splitting edge in middle of triangle:
	    for (j = 1; j <= 2; j++)
	      {
		INT<2> paedge1, paedge2;
		if (j == 1)
		  {
		    paedge1 = INT<2> (pa1[0], i2[1]);
		    paedge2 = INT<2> (pa1[1], i2[1]);
		  }
		else
		  {
		    paedge1 = INT<2> (pa2[0], i2[0]);
		    paedge2 = INT<2> (pa2[1], i2[0]);
		  }
		if (paedge1[0] > paedge1[1]) 
		  Swap (paedge1[0], paedge1[1]);
		if (paedge2[0] > paedge2[1]) 
		  Swap (paedge2[0], paedge2[1]);
		
		int paedgenr1 = 0, paedgenr2 = 0;
		int orient1, orient2;
		
		// if first vertex number is -1, then don't try to find entry in node2edge hash table
		if ( paedge1[0] == -1 || paedge2[0] == -1 )
		  continue;

		if (node2edge->Used (paedge1) && node2edge->Used (paedge2))
		  {
		    paedgenr1 = node2edge->Get (paedge1);
		    orient1 = (paedge1[0] == i2[0] || paedge1[1] == i2[1]) ? 1 : 0;
		    paedgenr2 = node2edge->Get (paedge2);
		    orient2 = (paedge2[0] == i2[0] || paedge2[1] == i2[1]) ? 1 : 0;
		    
		    parentedges[i][0] = 2 * paedgenr1 + orient1;	      
		    parentedges[i][1] = 2 * paedgenr2 + orient2;	      
		  }
	      }
	    
	    if (parentedges[i][0] == -1)
	      {
		// quad split
		if (pa1[0] != pa2[0] && 
		    pa1[0] != pa2[1] && 
		    pa1[1] != pa2[0] && 
		    pa1[1] != pa2[1])
		  for (j = 1; j <= 2; j++)
		    {
		      INT<2> paedge1, paedge2;
		      if (j == 1)
			{
			  paedge1 = INT<2> (pa1[0], pa2[0]);
			  paedge2 = INT<2> (pa1[1], pa2[1]);
			}
		      else
			{
			  paedge1 = INT<2> (pa1[0], pa2[1]);
			  paedge2 = INT<2> (pa1[1], pa2[0]);
			}
		      
		      int paedgenr1 = 0, paedgenr2 = 0;
		      int orient1 = 1, orient2 = 1;
		      
		      if (paedge1[0] > paedge1[1]) 
			{
			  Swap (paedge1[0], paedge1[1]);
			  orient1 = 0;
			}
		      if (paedge2[0] > paedge2[1]) 
			{
			  Swap (paedge2[0], paedge2[1]);
			  orient2 = 0;
			}

		      if ( paedge1[0] == -1 || paedge2[0] == -1 )
			continue;
		      
		      if (node2edge->Used (paedge1) && node2edge->Used (paedge2))
			{
			  paedgenr1 = node2edge->Get (paedge1);
			  paedgenr2 = node2edge->Get (paedge2);
			  parentedges[i][0] = 2 * paedgenr1 + orient1;	      
			  parentedges[i][1] = 2 * paedgenr2 + orient2;	      
			}
		    }
	      }
	    
	    if (parentedges[i][0] == -1)
	      {
		// triangle split into quad+trig (from anisotropic pyramids)
		for (j = 0; j < 2; j++)
		  for (k = 0; k < 2; k++)
		    {
		      INT<2> paedge (pa1[1-j], pa2[1-k]);
		      int orientpa = 1;
		      if (paedge[0] > paedge[1]) 
			{
			  Swap (paedge[0], paedge[1]);
			  orientpa = 0;
			}	
		      if (pa1[j] == pa2[k] && node2edge->Used(paedge))
			{
			  int paedgenr = node2edge->Get (paedge);
			  parentedges[i][0] = 2 * paedgenr + orientpa;
			}
		    }
	      }
	  }
      
	if (i > nelevel[0] && parentedges[i][0] == -1)
	  {
	    cerr << "no parent edge found, edge = " 
		 << i2[0] << ", " << i2[1] 
		 << ", pa1 = " << pa1[0] << ", " << pa1[1] 
		 << ", pa2 = " << pa2[0] << ", " << pa2[1]
		 << endl;
	  }
      }
    
    
    delete node2edge;
    
    prol->Update();
    

#ifdef PARALLEL
    UpdateParallelDofs();
#endif
    
    //   (*testout) << "edges: " << endl;
    //   for (i = 1; i <= ned; i++)
    //     {
    //       (*testout) << i << ": " << EdgePoint1(i) << ", " << EdgePoint2(i) << endl;
    //     }

    //   (*testout) << "parent edges: " << endl;
    //   for (i = 1; i <= ned; i++)
    //     {
    //       (*testout) << i << ": " << parentedges.Get(i)[0] << ", " << parentedges.Get(i)[1] << endl;
    //     }
  }



  int NedelecFESpace :: GetNDof () const
  {
    return nelevel.Last();
  }

  int NedelecFESpace :: GetNDofLevel (int level) const
  {
    return nelevel[level];
  }

  
  void NedelecFESpace :: GetDofNrs (int elnr, Array<int> & dnums) const
  {
    int eoa[12];
    Array<int> eorient(12, eoa);
    GetMeshAccess().GetElEdges (elnr, dnums, eorient);

    if (!DefinedOn (ma.GetElIndex (elnr)))
      dnums = -1;

    //(*testout) << "el = " << elnr << ", dofs = " << dnums << endl;
  }


  void NedelecFESpace :: GetSDofNrs (int selnr, Array<int> & dnums) const
  {
    int eoa[12];
    Array<int> eorient(12, eoa);
    GetMeshAccess().GetSElEdges (selnr, dnums, eorient);

    if (!DefinedOnBoundary (ma.GetSElIndex (selnr)))
      dnums = -1;
  }




  template <class MAT>
  void NedelecFESpace::TransformMat (int elnr, bool boundary,
				     MAT & mat, TRANSFORM_TYPE tt) const
  {
    int nd;
    ArrayMem<int,12> enums, eorient;
    LocalHeapMem<1000> lh;

    if (boundary)
      {
	GetMeshAccess().GetSElEdges (elnr, enums, eorient);
	nd = GetSFE (elnr, lh).GetNDof();
      }
    else
      {
	GetMeshAccess().GetElEdges (elnr, enums, eorient);
	nd = GetFE (elnr, lh).GetNDof();
      }

    int i, j, k, l;

    if (tt & TRANSFORM_MAT_LEFT)
      for (k = 0; k < dimension; k++)
	for (i = 0; i < nd; i++)
	  for (j = 0; j < mat.Width(); j++)
	    mat(k+i*dimension, j) *= eorient[i];

    if (tt & TRANSFORM_MAT_RIGHT)
      for (l = 0; l < dimension; l++)
	for (i = 0; i < mat.Height(); i++)
	  for (j = 0; j < nd; j++)
	    mat(i, l+j*dimension) *= eorient[j];
  }


  template <class VEC>
  void NedelecFESpace::TransformVec (int elnr, bool boundary,
				     VEC & vec, TRANSFORM_TYPE tt) const
  {
    int nd;
    // int ena[12], eoa[12];
    // Array<int> enums(12, ena), eorient(12, eoa);
    ArrayMem<int,12> enums, eorient;
    LocalHeapMem<1000> lh;

    if (boundary)
      {
	GetMeshAccess().GetSElEdges (elnr, enums, eorient);
	nd = GetSFE (elnr, lh).GetNDof();
      }
    else
      {
	GetMeshAccess().GetElEdges (elnr, enums, eorient);
	nd = GetFE (elnr, lh).GetNDof();
      }
  
    if ((tt & TRANSFORM_RHS) || (tt & TRANSFORM_SOL))
      {
	for (int k = 0; k < dimension; k++)
	  for (int i = 0; i < nd; i++)
	    vec(k+i*dimension) *= eorient[i];
      }
  }

  
  Table<int> * NedelecFESpace :: CreateSmoothingBlocks (const Flags & precflags) const
  {
    CreateSmoothingBlocks (int (precflags.GetNumFlag ("blocktype", 0)));
  }



  Table<int> * NedelecFESpace :: CreateSmoothingBlocks (int type) const
  {
    cout << "NedelecFESpace::CreateSmoothingBlocks" << endl;

    int nd = GetNDof();
    int nv = ma.GetNV();
    int level = ma.GetNLevels()-1;

    Table<int> *node2edge = 0;
    //type = SB_AFW;  
    switch (type)
      {
      case SB_AFW:
	{
	  cout << " ******** Low-order H(Curl) Smoother: AFW" << endl; 
	  Array<int> cnts(nv);
	  for (int k = 1; k <= 2; k++)
	    {
	      if (k == 2)
		node2edge = new Table<int>(cnts);
	    
	      cnts = 0;

	    
	      for (int j = 0; j < nd; j++)
		{
		  if (FineLevelOfEdge(j) < level) continue;
		
		  int ep1 = EdgePoint1(j);
		  int ep2 = EdgePoint2(j);

		
		  // for anisotropic connections:
		  int cep1 = ma.GetClusterRepVertex(ep1);
		  int cep2 = ma.GetClusterRepVertex(ep2);

		  if (k == 2)
		    {
		      (*node2edge)[cep1][cnts[cep1]] = j;
		      cnts[cep1]++;
		    
		      if (cep1 != cep2)
			{
			  (*node2edge)[cep2][cnts[cep2]] = j;
			  cnts[cep2]++;
			}
		    }
		  else
		    {
		      cnts[cep1]++;
		      if (cep1 != cep2)
			cnts[cep2]++;
		    }
		
		}
	    }
	  //(*testout) << "node2egde: " << *node2edge << endl;
	  break;
	}
      case SB_JAC: // only for getting bad condition numbers  ... 
	{
	  cout << " Jacobi Smoother for Low-order H(Curl) --> bad conditoning" << endl;  
	  Array<int> cnts(nd);
	  for (int k = 1; k <= 2; k++)
	    {
	      if (k == 2)
		node2edge = new Table<int>(cnts);
	    
	      cnts = 0;

	    
	      for (int j = 0; j < nd; j++)
		{
		  if (FineLevelOfEdge(j) < level) continue;
		
		
		  if (k == 2)
		    {
		      (*node2edge)[j][0] = j;
		   
		    }
		  else
		    {
		      cnts[j]=1; 
		    }
		
		}
	    }
	  (*testout) << "node2egde: " << *node2edge << endl;
	  break;
	}
	//     case SB_AFW:
	//       {
	// 	Array<int> cnts(nv);
	// 	for (int k = 1; k <= 2; k++)
	// 	  {
	// 	    if (k == 2)
	// 	      node2edge = new Table<int>(cnts);
	    
	// 	    cnts = 0;
	    
	// 	    for (int j = 0; j < nd; j++)
	// 	      {
	// 		if (FineLevelOfEdge(j) < level) continue;
		
	// 		int ep1 = EdgePoint1(j);
	// 		int ep2 = EdgePoint2(j);

		
	// 		// for anisotropic connections:
	// 		int cep1 = ma.GetClusterRepVertex(ep1);
	// 		int cep2 = ma.GetClusterRepVertex(ep2);

	// 		if (k == 2)
	// 		  {
	// 		    (*node2edge)[cep1][cnts[cep1]] = j;
	// 		    cnts[cep1]++;
		    
	// 		    if (cep1 != cep2)
	// 		      {
	// 			(*node2edge)[cep2][cnts[cep2]] = j;
	// 			cnts[cep2]++;
	// 		      }
	// 		  }
	// 		else
	// 		  {
	// 		    cnts[cep1]++;
	// 		    if (cep1 != cep2)
	// 		      cnts[cep2]++;
	// 		  }
		
	// 	      }
	// 	  }
	// 	//	(*testout) << "node2egde: " << *node2edge << endl;
	// 	break;
	//       }



      case SB_HIPTMAIR:
	{
	  Array<int> cnts(nd);
	  for (int k = 1; k <= 2; k++)
	    {
	      if (k == 2)
		node2edge = new Table<int>(cnts);
	    
	      cnts = 0;
	    
	      for (int j = 0; j < nd; j++)
		{
		  if (FineLevelOfEdge(j) < level) continue;

		  int ecl = ma.GetClusterRepEdge (j);
		  if (ecl < nv)
		    ecl = j;
		  else
		    ecl -= nv;

		  if (k == 2)
		    {
		      (*node2edge)[ecl][cnts[ecl]] = j;
		      cnts[ecl]++;
		    }
		  else
		    {
		      cnts[ecl]++;
		    }
		
		}
	    }
	  break;
	}
      case SB_POTENTIAL:
	{
	  Array<int> cnts(nv);
	  for (int k = 1; k <= 2; k++)
	    {
	      if (k == 2)
		node2edge = new Table<int>(cnts);
	    
	      cnts = 0;
	    
	      for (int j = 0; j < nv; j++)
		{
		  int vcl = ma.GetClusterRepVertex (j);
		  if (k == 2)
		    {
		      (*node2edge)[vcl][cnts[vcl]] = j;
		      cnts[vcl]++;
		    }
		  else
		    {
		      cnts[vcl]++;
		    }
		}
	    }
	  break;
	}
      }
  
    return node2edge;
  }

  SparseMatrix<double> * 
  NedelecFESpace :: CreateGradient() const
  {
    int i;
    int ned = GetNDof();
    int level = ma.GetNLevels()-1;

    Array<int> cnts(ned);
    for (i = 0; i < ned; i++)
      cnts[i] = (FineLevelOfEdge(i) == level) ? 2 : 0;

    SparseMatrix<double> & grad = *new SparseMatrix<double>(cnts);

    for (i = 0; i < ned; i++)
      {
	if (FineLevelOfEdge(i) < level) continue;
	grad.CreatePosition (i, edgepoints[i][0]);
	grad.CreatePosition (i, edgepoints[i][1]);
      }
    for (i = 0; i < ned; i++)
      {
	if (FineLevelOfEdge(i) < level) continue;
	grad(i, edgepoints[i][0]) = 1;
	grad(i, edgepoints[i][1]) = -1;
      }

    return &grad;
  }



  void NedelecFESpace :: GetVertexDofNrs (int vnr, Array<int> & dnums) const
  {
    dnums.SetSize(0);
  }

  void NedelecFESpace :: GetEdgeDofNrs (int ednr, Array<int> & dnums) const
  {
    dnums.SetSize(1);
    dnums[0] = ednr;
  }

  void NedelecFESpace :: GetFaceDofNrs (int fanr, Array<int> & dnums) const
  {
    dnums.SetSize(0);
  }

  void NedelecFESpace :: GetInnerDofNrs (int elnr, Array<int> & dnums) const
  {
    dnums.SetSize(0);
  }

#ifdef PARALLEL
  void NedelecFESpace :: UpdateParallelDofs_loproc()
  {
    if ( discontinuous )
      {
	*testout << "NedelecFESpace::UpdateParallelDofs_loproc -- discontinuous" << endl;
	
	const MeshAccess & ma = (*this). GetMeshAccess();
	
	int ndof = GetNDof();
	
	// Find number of exchange dofs
	Array<int> nexdof(ntasks); 
	nexdof = 0;
	
	paralleldofs->SetNExDof(nexdof);
	
// 	paralleldofs->localexchangedof = new Table<int> (nexdof);
// 	paralleldofs->distantexchangedof = new Table<int> (nexdof);
	paralleldofs->sorted_exchangedof = new Table<int> (nexdof);
	return;
      }


    *testout << "NedelecFESpace::UpdateParallelDofs_loproc" << endl;

    const MeshAccess & ma = (*this). GetMeshAccess();

    int ndof = GetNDof();

    // Find number of exchange dofs
    Array<int> nexdof(ntasks); 
    nexdof = 0;

    MPI_Status status;
    MPI_Request * sendrequest = new MPI_Request [ntasks];
    MPI_Request * recvrequest = new MPI_Request [ntasks];

    // number of edge exchange dofs
    for ( int edge = 0; edge < ma.GetNEdges(); edge++ )
      {
	nexdof[id] ++;//= dnums.Size() ; 
	for ( int dest = 1; dest < ntasks; dest ++ )
	  if (  parallelma -> GetDistantEdgeNum ( dest, edge ) >= 0 )
	    { 
	      nexdof[dest] ++; 
	    }
      }

    paralleldofs->SetNExDof(nexdof);

//     paralleldofs->localexchangedof = new Table<int> (nexdof);
//     paralleldofs->distantexchangedof = new Table<int> (nexdof);
    paralleldofs->sorted_exchangedof = new Table<int> (nexdof);



    Array<int> ** owndofs,** distantdofs;
    owndofs = new Array<int> * [ntasks];
    distantdofs = new Array<int> * [ntasks];

    for ( int i = 0; i < ntasks; i++)
      {
	owndofs[i] = new Array<int> (1);
	(*owndofs[i])[0] = ndof;
	distantdofs[i] = new Array<int> (0);
      }

    int exdof = 0;
    Array<int> cnt_nexdof(ntasks);
    cnt_nexdof = 0;

    // *****************
    // Parallel Edge dofs
    // *****************


    // find local and distant dof numbers for edge exchange dofs
    for ( int edge = 0; edge < ma.GetNEdges(); edge++ )
      {
	(*(paralleldofs->sorted_exchangedof))[id][exdof++] = edge;

	for ( int dest = 1; dest < ntasks; dest++ )
	  {
	    int distedge = parallelma -> GetDistantEdgeNum ( dest, edge );
	    if( distedge < 0 ) continue;

	    owndofs[dest]->Append ( distedge );
	    paralleldofs->SetExchangeDof ( dest, edge );
	    paralleldofs->SetExchangeDof ( edge );
	    owndofs[dest]->Append ( edge );
  	  }
      }   


    for ( int dest = 1; dest < ntasks; dest ++ )
      {
	MyMPI_ISend ( *owndofs[dest], dest, sendrequest[dest]);
	MyMPI_IRecv ( *distantdofs[dest], dest, recvrequest[dest]);
      }
    


    int ii = 1;
    for( int dest=1; dest<ntasks; dest++) 
      {
	if ( dest == id ) continue;
	MPI_Wait(recvrequest+dest, &status);
	paralleldofs -> SetDistNDof( dest, (*distantdofs[dest])[0]) ;
	ii = 1;
	while ( ii < distantdofs[dest]->Size() )
	  {
	    int ednum = (*distantdofs[dest])[ii++];
// 	    (*(paralleldofs->localexchangedof))[dest][ cnt_nexdof[dest] ] = ednum;
// 	    (*(paralleldofs->distantexchangedof))[dest][ cnt_nexdof[dest] ] = (*distantdofs[dest])[ii];
	    (*(paralleldofs->sorted_exchangedof))[dest][ cnt_nexdof[dest] ] = ednum;
	    ii++; cnt_nexdof[dest]++;
	     
	  }
      }

    for ( int dest = id+1; dest < ntasks; dest++ )
      QuickSort ( (*(paralleldofs->sorted_exchangedof))[dest] );

    for ( int i = 0; i < ntasks; i++ )
      {
	delete distantdofs[i];
	delete owndofs[i];
      }

    delete [] owndofs;
    delete [] distantdofs;
    delete [] sendrequest;
    delete [] recvrequest;
  }

  void NedelecFESpace :: UpdateParallelDofs_hoproc()
  {
    if ( discontinuous )
      {
	*testout << "NedelecFESpace::UpdateParallelDofs_hoproc -- discontinuous" << endl;
	int ndof = GetNDof();
	// Find number of exchange dofs
	Array<int> nexdof(ntasks);
	nexdof = 0;
	
	const MeshAccess & ma = (*this). GetMeshAccess();
	
	paralleldofs->SetNExDof(nexdof);
	
// 	paralleldofs->localexchangedof = new Table<int> (nexdof);
// 	paralleldofs->distantexchangedof = new Table<int> (nexdof);
	paralleldofs->sorted_exchangedof = new Table<int> (nexdof);
	
	for ( int i = 0; i < ndof; i++ )
	  {
	    paralleldofs->ClearExchangeDof(i);
	    for ( int dest = 0; dest < ntasks; dest++ )
	      paralleldofs->ClearExchangeDof(dest, i);
	  }
	
	
	int * ndof_dist = new int[ntasks];
	MPI_Allgather ( &ndof, 1, MPI_INT, &ndof_dist[0], 
			1, MPI_INT, MPI_COMM_WORLD);
	for ( int dest = 0; dest < ntasks; dest++ )
	  paralleldofs -> SetDistNDof( dest, ndof_dist[dest]) ;
	
	delete [] ndof_dist;
	
	return;
      }


    // ******************************
    // update exchange dofs 
    // ******************************
    *testout << "NedelecFESpace::UpdateParallelDofs_hoproc" << endl;
    // Find number of exchange dofs
    Array<int> nexdof(ntasks);
    nexdof = 0;

    MPI_Status status;
    MPI_Request * sendrequest = new MPI_Request [ntasks];
    MPI_Request * recvrequest = new MPI_Request [ntasks];

    // number of edge exchange dofs
    for ( int edge = 0; edge < ma.GetNEdges(); edge++ )
      {
	if ( !parallelma->IsExchangeEdge ( edge ) ) continue;
	nexdof[id] ++ ;  
	
	for ( int dest = 1; dest < ntasks; dest ++ )
	  if (  parallelma -> GetDistantEdgeNum ( dest, edge ) >= 0 )
	    nexdof[dest] ++ ;  
      }

    int ndof = GetNDof();

    nexdof[0] = ndof;

    paralleldofs->SetNExDof(nexdof);

//     paralleldofs->localexchangedof = new Table<int> (nexdof);
//     paralleldofs->distantexchangedof = new Table<int> (nexdof);
    paralleldofs->sorted_exchangedof = new Table<int> (nexdof);

    Array<int> ** owndofs, ** distantdofs;
    owndofs = new Array<int>* [ntasks];
    distantdofs = new Array<int>* [ntasks];

    for ( int i = 0; i < ntasks; i++ )
      {
	owndofs[i] = new Array<int>(1);
	(*owndofs[i])[0] = GetNDof();
	distantdofs[i] = new Array<int>(0);
      }
    // *****************
    // Parallel Vertex dofs
    // *****************

    Array<int> cnt_nexdof(ntasks);
    cnt_nexdof = 0;
    int exdof = 0;

   
    // *****************
    // Parallel Edge dofs
    // *****************

    for ( int dest = 0; dest < ntasks; dest++)
      {
	owndofs[dest]->SetSize(1);
	distantdofs[dest]->SetSize(0);
      }
    for ( int edge = 0; edge < ma.GetNEdges(); edge++ )
      if ( parallelma->IsExchangeEdge ( edge ) )
	{
	  (*(paralleldofs->sorted_exchangedof))[id][exdof++] = edge;

	  for ( int dest = 1; dest < ntasks; dest++ )
	    {
	      int distedge = parallelma -> GetDistantEdgeNum ( dest, edge );
	      if( distedge < 0 ) continue;

	      owndofs[dest]->Append ( distedge );
	      owndofs[dest]->Append (int(paralleldofs->IsGhostDof(edge)) );
	      paralleldofs->SetExchangeDof ( dest, edge );
	      paralleldofs->SetExchangeDof ( edge );
	      owndofs[dest]->Append ( edge );
	    }
	}   


    for ( int sender = 1; sender < ntasks; sender ++ )
      {
        if ( id == sender )
          for ( int dest = 1; dest < ntasks; dest ++ )
            if ( dest != id )
              {
		MyMPI_ISend ( *owndofs[dest], dest, sendrequest[dest]);
              }
	  
        if ( id != sender )
          {
	    MyMPI_IRecv ( *distantdofs[sender], sender, recvrequest[sender]);
          }
 	  
	  
      }

    for( int dest=1; dest<ntasks; dest++) 
      {
	if ( dest == id ) continue;
	MPI_Wait(recvrequest+dest, &status);
	paralleldofs -> SetDistNDof( dest, (*distantdofs[dest])[0]) ;
	int ii = 1;
	while ( ii < distantdofs[dest]->Size() )
	  {
	    int ednum = (*distantdofs[dest])[ii++];
	    int isdistghost = (*distantdofs[dest])[ii++];
	    (*(paralleldofs->sorted_exchangedof))[dest][ cnt_nexdof[dest] ] = ednum;
// 	    (*(paralleldofs->distantexchangedof))[dest][ cnt_nexdof[dest] ] = (*distantdofs[dest])[ii];
	    if ( dest < id && !isdistghost )
	      paralleldofs->ismasterdof.Clear ( ednum ) ;
	    ii++; cnt_nexdof[dest]++;
	  }
      }


    /*******************************

         update low order space

    *****************************/

    for ( int i = 1; i < ntasks; i++ )
      {
	delete distantdofs[i];
	delete  owndofs[i];
      }

    exdof = 0;
    cnt_nexdof = 0;


    // *****************
    // Parallel Edge dofs
    // *****************

    owndofs[0]->SetSize(1);
    (*owndofs[0])[0] = ndof;
    distantdofs[0]->SetSize(0);
    
    // find local and distant dof numbers for vertex exchange dofs
    for ( int edge = 0; edge < ma.GetNEdges(); edge++ )
      {
	int dest = 0;
	int distedge = parallelma -> GetDistantEdgeNum ( dest, edge );
	owndofs[0]->Append ( distedge );
	paralleldofs->SetExchangeDof ( dest, edge );
	owndofs[0]->Append ( edge );
      }   
    
    int dest = 0;
    MyMPI_ISend ( *owndofs[0], dest, sendrequest[dest]);
    MyMPI_IRecv ( *distantdofs[0], dest, recvrequest[dest]);
    MPI_Wait ( recvrequest + dest, &status);

    paralleldofs -> SetDistNDof( dest, (*distantdofs[dest])[0]) ;
    int ii = 1;
    while ( ii < distantdofs[0]->Size() )
      {
	int ednum = (*distantdofs[0])[ii++];
	(*(paralleldofs->sorted_exchangedof))[dest][ cnt_nexdof[dest] ] = ednum;
// 	(*(paralleldofs->distantexchangedof))[dest][ cnt_nexdof[dest] ] = (*distantdofs[0])[ii];
	ii++; cnt_nexdof[dest]++;
      }

    for ( int dest = id+1; dest < ntasks; dest++ )
      QuickSort ( (*(paralleldofs->sorted_exchangedof))[dest] );


    delete distantdofs[0];
    delete owndofs[0];

    delete [] owndofs;
    delete [] distantdofs;
    delete [] recvrequest;
    delete  [] sendrequest;

  }
#endif
  /*

  #ifdef PARALLEL
  void NedelecFESpace :: UpdateParallelDofs ( )
  {
  int id, ntasks;
  MPI_Comm_size(MPI_COMM_WORLD, &ntasks);
  MPI_Comm_rank(MPI_COMM_WORLD, &id);

  if  ( ntasks == 1 ) 
  {
  if ( paralleldofs ) delete paralleldofs;
  paralleldofs = 0;
  return;
  }

  if ( paralleldofs == 0 ) 
  paralleldofs = new ParallelDofs (*this);
  paralleldofs -> Update();
  }


  void NedelecFESpace2 :: UpdateParallelDofs ( )
  {
  int id, ntasks;
  MPI_Comm_size(MPI_COMM_WORLD, &ntasks);
  MPI_Comm_rank(MPI_COMM_WORLD, &id);

  if  ( ntasks == 1 ) 
  {
  if ( paralleldofs ) delete paralleldofs;
  paralleldofs = 0;
  return;
  }

  if ( paralleldofs == 0 ) 
  paralleldofs = new ParallelDofs (*this);
  paralleldofs -> Update();
  }
  void NedelecFESpace :: UpdateParallelDofs ( LocalHeap & lh )
  {
  int id, ntasks;
  MPI_Comm_size(MPI_COMM_WORLD, &ntasks);
  MPI_Comm_rank(MPI_COMM_WORLD, &id);

  if  ( ntasks == 1 ) 
  {
  if ( paralleldofs ) delete paralleldofs;
  paralleldofs = 0;
  return;
  }

  if ( paralleldofs == 0 ) 
  paralleldofs = new ParallelDofs (*this);
  paralleldofs -> Update();
  }



  void NedelecFESpace2 :: UpdateParallelDofs ( LocalHeap & lh )
  {
  int id, ntasks;
  MPI_Comm_size(MPI_COMM_WORLD, &ntasks);
  MPI_Comm_rank(MPI_COMM_WORLD, &id);

  if  ( ntasks == 1 ) 
  {
  if ( paralleldofs ) delete paralleldofs;
  paralleldofs = 0;
  return;
  }

  if ( paralleldofs == 0 ) 
  paralleldofs = new ParallelDofs (*this);
  paralleldofs -> Update();
  }
  #endif
  */


  NedelecFESpace2 :: NedelecFESpace2 (const MeshAccess & ama, const Flags & flags, bool parseflags)
    : FESpace (ama, flags)
  {
    name="NedelecFESpace2(hcurl)";
    // defined flags
    DefineDefineFlag("hcurl");
    DefineNumFlag("zorder");
    DefineNumListFlag("gradientdomains");
    DefineNumListFlag("gradientboundaries");
    DefineNumListFlag("direcsolverdomains");
    DefineNumListFlag("directsolvermaterials");
  
    if(parseflags) CheckFlags(flags);
  
    order = (int) flags.GetNumFlag("order",2);
    zorder = int(flags.GetNumFlag ("zorder", order));

    ned = 0;

    n_plane_edge_dofs = order;
    n_z_edge_dofs = zorder;
    n_edge_dofs = max2 (n_z_edge_dofs, n_plane_edge_dofs);
    n_pyramid_el_dofs = 0;


    curltet = 0;
    curlprism = 0;
    curlpyramid = 0;
  
    switch (order)
      {
      case 2:
	{
	  segm    = new FE_NedelecSegm2;
	  trig    = new FE_NedelecTrig2;
	  tet     = new FE_NedelecTet2;

	  n_trig_face_dofs = 0;
	  n_tet_el_dofs = 0;

	  n_quad_face_dofs = 3*zorder-2;
	  n_prism_el_dofs = 0;
	  n_prism_nograd_el_dofs = 0;
	  n_pyramid_el_dofs = 0;

	  switch (zorder)
	    {
	    case 1:
	      {
		quad    = new FE_TNedelecQuad<2,1>;
		prism   = new FE_TNedelecPrism2<1>;
		//	      pyramid = new FE_NedelecPyramid2;
		break;
	      }
	    case 2:
	      {
		quad    = new FE_TNedelecQuad<2,2>;
		prism   = new FE_TNedelecPrism2<2>;
		pyramid = new FE_NedelecPyramid2;
		break;
	      }
	    case 3:
	      {
		quad    = new FE_TNedelecQuad<2,3>;
		prism   = new FE_TNedelecPrism2<3>;
		break;
	      }
	    case 4:
	      {
		quad    = new FE_TNedelecQuad<2,4>;
		prism   = new FE_TNedelecPrism2<4>;
		break;
	      }
	    }
	  break;
	}
      case 3:
	{
	  segm    = new FE_NedelecSegm3;
	  trig    = new FE_NedelecTrig3;
	  tet     = new FE_NedelecTet3;
	  curltet     = new FE_NedelecTet3NoGrad;

	  n_trig_face_dofs = 3;
	  n_tet_el_dofs = 0;
	  n_quad_face_dofs = 5*zorder-3;
	  n_prism_el_dofs = 4*zorder-3;
	  n_prism_nograd_el_dofs = 3*zorder-2;
	  n_pyramid_el_dofs = 9;

	  switch (zorder)
	    {
	    case 1:
	      {
		quad    = new FE_TNedelecQuad<3,1>;
		prism   = new FE_TNedelecPrism3<1>;	      
		curlprism   = new FE_TNedelecPrism3NoGrad<1>;
		break;
	      }
	    case 2:
	      {
		quad    = new FE_TNedelecQuad<3,2>;
		prism   = new FE_TNedelecPrism3<2>;
		curlprism   = new FE_TNedelecPrism3NoGrad<2>;
		break;
	      }
	    case 3:
	      {
		quad    = new FE_TNedelecQuad<3,3>;
		prism   = new FE_TNedelecPrism3<3>;
		pyramid = new FE_NedelecPyramid3;
		curlprism   = new FE_TNedelecPrism3NoGrad<3>;
		break;
	      }
	    }
	  break;
	}
      }

    if (!curltet) curltet = tet;
    if (!curlprism) curlprism = prism;


    gradientdomains.SetSize (ma.GetNDomains());
    gradientdomains.Set();
    gradientboundaries.SetSize (ma.GetNBoundaries());
    gradientboundaries.Set();

    if (flags.NumListFlagDefined("gradientdomains"))
      {
        cout << "has gradientdomains" << endl;
	const Array<double> & graddomains = flags.GetNumListFlag ("gradientdomains");
	for (int i = 0; i < gradientdomains.Size(); i++)
	  if (!graddomains[i])
	    gradientdomains.Clear(i);
      }

    if (flags.NumListFlagDefined("gradientboundaries"))
      {
	const Array<double> & gradbounds = flags.GetNumListFlag ("gradientboundaries");
	for (int i = 0; i < gradientboundaries.Size(); i++)
	  if (!gradbounds[i])
	    gradientboundaries.Clear(i);
      }


    cout << "gradientdomains = " << gradientdomains << endl;

    Flags loflags;
    loflags.SetFlag ("order", 1);
    loflags.SetFlag ("dim", dimension);
    if (iscomplex) loflags.SetFlag ("complex");
    low_order_space = new NedelecFESpace (ma, loflags);
    // low_order_space = new NedelecFESpace (ama, 1, dimension, iscomplex);

    prol = new EdgeProlongation (*static_cast<NedelecFESpace*> (low_order_space));
    /*
      CreateVecObject1(prol, EdgeProlongation,
      dimension, iscomplex, 
      *static_cast<NedelecFESpace*> (low_order_space));
      */

    // Evaluator for shape tester 
    if (ma.GetDimension() == 2)
      {
	Array<CoefficientFunction*> coeffs(1);
	coeffs[0] = new ConstantCoefficientFunction(1);
	evaluator = GetIntegrators().CreateBFI("massedge", 2, coeffs);
      }
    else if(ma.GetDimension() == 3) 
      {
	Array<CoefficientFunction*> coeffs(1); 
	coeffs[0] = new ConstantCoefficientFunction(1); 
	evaluator = GetIntegrators().CreateBFI("massedge",3,coeffs); 
	boundary_evaluator = GetIntegrators().CreateBFI("robinedge",3,coeffs); 
	
      }


    if(flags.NumListFlagDefined("directsolverdomains"))
      {
	directsolverclustered.SetSize(ama.GetNDomains());
	directsolverclustered = false;
	Array<double> clusters(flags.GetNumListFlag("directsolverdomains"));
	for(int i=0; i<clusters.Size(); i++) 
	  directsolverclustered[static_cast<int>(clusters[i])-1] = true; // 1-based!!
      }
    
    if(flags.StringListFlagDefined("directsolvermaterials"))
      {
	directsolvermaterials.SetSize(flags.GetStringListFlag("directsolvermaterials").Size());
	for(int i=0; i<directsolvermaterials.Size(); i++)
	  directsolvermaterials[i] = flags.GetStringListFlag("directsolvermaterials")[i];
      }

  }


  NedelecFESpace2 :: ~NedelecFESpace2 ()
  {
    //delete low_order_space;
  }

  void NedelecFESpace2 :: Update(LocalHeap & lh)
  {
    if (low_order_space)
      low_order_space -> Update(lh);

    int i, j;
    const MeshAccess & ma = GetMeshAccess();

    int level = ma.GetNLevels();
    int ne = ma.GetNE();
    int nse = ma.GetNSE();
    // int np = ma.GetNP();

    ned = ma.GetNEdges();
    nfa = ma.GetNFaces(); 
    nel = ma.GetNE(); 

    if (ma.GetDimension() == 2)
      nfa = nel;

    Array<int> pnums;

    if (gradientedge.Size() == ned)
      return;

    // new definition of gradient edges

    Array<int> enums, eorient, fnums, forient;

    gradientedge.SetSize(ned);
    gradientedge = 1;
    gradientface.SetSize(nfa);
    gradientface = 1;

    /*
      for (i = 0; i < nse; i++)
      {
      ma.GetSElEdges (i, enums, eorient);
      for (j = 0; j < enums.Size(); j++)
      gradientedge[enums[j]] = 1;
      }
    */


    first_face_dof.SetSize (nfa+1);
    first_el_dof.SetSize (nel+1);

    first_face_dof[0] = n_edge_dofs * ned;
    for (i = 0; i < nfa; i++)
      {
	ma.GetFacePNums (i, pnums);
	if (pnums.Size() == 3)
	  first_face_dof[i+1] = first_face_dof[i] + n_trig_face_dofs;
	else
	  first_face_dof[i+1] = first_face_dof[i] + n_quad_face_dofs;
      }

    first_el_dof[0] = first_face_dof[nfa];
    for (i = 0; i < ne; i++)
      {
	bool gradel = gradientdomains[ma.GetElIndex(i)];
	switch (ma.GetElType (i))
	  {
	  case ET_TET:
	    first_el_dof[i+1] = first_el_dof[i] + n_tet_el_dofs; 
	    break;
	  case ET_PRISM:
	    {
	      if (gradel) first_el_dof[i+1] = first_el_dof[i] + n_prism_el_dofs; 
	      else first_el_dof[i+1] = first_el_dof[i] + n_prism_nograd_el_dofs; 
	      break;
	    }
	  case ET_PYRAMID:
	    first_el_dof[i+1] = first_el_dof[i] + n_pyramid_el_dofs; 
	    break;
	  default:
	    throw Exception ("unhandled case in NedelecFESpace2::Update");
	  }
      }

    if (level != ndlevel.Size())
      ndlevel.Append (first_el_dof[nel]);
  

    if (gradientdomains.Size())
      {
	gradientedge = 0;
	gradientface = 0;

	for (i = 0; i < ne; i++)
	  {
	    if (gradientdomains[ma.GetElIndex(i)])
	      {
		ma.GetElEdges (i, enums, eorient);
		for (j = 0; j < enums.Size(); j++)
		  gradientedge[enums[j]] = 1;
		ma.GetElFaces (i, fnums, forient);
		for (j = 0; j < fnums.Size(); j++)
		  gradientface[fnums[j]] = 1;
	      }
	  }
      }

    fnums.SetSize(1);
    forient.SetSize(1);
    (*testout) << "gradientboundaries = " << endl << gradientboundaries << endl;
    if (gradientboundaries.Size())
      for (i = 0; i < nse; i++)
	{
	  if (gradientboundaries[ma.GetSElIndex(i)])
	    {
	      ma.GetSElEdges (i, enums, eorient);
	      for (j = 0; j < enums.Size(); j++)
		gradientedge[enums[j]] = 1;
	      ma.GetSElFace (i, fnums[0], forient[0]);
	      gradientface[fnums[0]] = 1;
	    }
	}

    //  gradientface = 0;
    //  (*testout) << "gradientedges = " << endl << gradientedge << endl;
  }


  int NedelecFESpace2 :: GetNDof () const
  {
    return ndlevel.Last();
  }

  int NedelecFESpace2 :: GetNDofLevel (int level) const
  {
    return ndlevel[level];
  }



  const FiniteElement & NedelecFESpace2 :: GetFE (int elnr, LocalHeap & lh) const
  {
    FiniteElement * fe = 0;
    ELEMENT_TYPE typ = ma.GetElType(elnr);

    switch (typ)
      {
      case ET_TET:
	fe = tet; break;
      case ET_PYRAMID:
	fe = pyramid; break;
      case ET_PRISM:
	fe = prism; break;
      case ET_HEX:
	fe = hex; break;
      case ET_TRIG:
	fe = trig; break;
      case ET_QUAD:
	fe = quad; break;
      default:
	fe = 0;
      }

    if (!gradientdomains[ma.GetElIndex(elnr)])
      {
	switch (typ)
	  {
	  case ET_TET:
	    fe = curltet; break;
	  case ET_PRISM:
	    fe = curlprism; break;
	    //	case ET_PYRAMID:
	    //	  fe = curlpyramid; break;
	  default:
	    ;
	  }
      }
  
    if (!fe)
      {
	stringstream str;
	str << "FESpace " << GetClassName() 
	    << ", undefined eltype " 
	    << ElementTopology::GetElementName(ma.GetElType(elnr))
	    << ", order = " << order << endl;
	throw Exception (str.str());
      }
  
    return *fe;
  }

  
  void NedelecFESpace2 :: GetDofNrs (int elnr, Array<int> & dnums) const
  {
    // int eled, elfa;
    int j;

    ArrayMem<int,6> fnums, forient;
    ArrayMem<int,12> enums;

    ma.GetElEdges (elnr, enums);
    ma.GetElFaces (elnr, fnums, forient);

    LocalHeapMem<1000> lh;
    int nd = GetFE (elnr, lh).GetNDof();
    dnums.SetSize(nd);
    dnums = -1;

    int index = ma.GetElIndex (elnr);

    if (!DefinedOn (index)) return;

    bool graddom = gradientdomains[index];

    switch (ma.GetElType(elnr))
      {
      case ET_TRIG:
	{
	  switch (order)
	    {
	    case 2:
	      {
		for (j = 0; j < 3; j++)
		  {
		    dnums[j] = enums[j];
		    if (gradientedge[enums[j]])
		      dnums[j+3] = enums[j] + ned;
		  }
		break;
	      }	
	    case 3:
	      {
		for (j = 0; j < 3; j++)
		  {
		    int edgenr = enums[j];
		    dnums[j] = edgenr;
		    if (gradientedge[edgenr])
		      {
			dnums[j+3] = edgenr + ned;
			dnums[j+6] = edgenr + 2*ned;
		      }
		  }
		int nfd = gradientface[elnr] ? 3 : 2;
		for (j = 0; j < nfd; j++)
		  dnums[9+j] = first_el_dof[elnr];
		break;
	      }
	    }
	  break;
	}
      case ET_TET:
	{
	  switch (order)
	    {
	    case 2:
	      {
		for (j = 0; j < 6; j++)
		  {
		    dnums[j] = enums[j];
		    if (gradientedge[enums[j]])
		      dnums[j+6] = enums[j] + ned;
		  }
		break;
	      }
	    case 3:
	      {
		for (j = 0; j < 6; j++)
		  dnums[j] = enums[j];

		int base = 6;

		if (nd == 30)
		  {
		    for (j = 0; j < 6; j++)
		      {
			int edgenr = enums[j];
			if (gradientedge[edgenr] && nd == 30)
			  {
			    dnums[base+j]  = edgenr + ned;
			    dnums[base+6+j] = edgenr + 2*ned;
			  }
		      }
		    base += 12;
		  }

		for (j = 0; j < 4; j++)
		  {
		    int facedir = forient[j];
		  
		    static const int reorder[8][3] =
		      { { 1, 2, 3 }, { 2, 1, 3 }, { 1, 3, 2 }, { 2, 3, 1 },
			{ 2, 1, 3 }, { 1, 2, 3 }, { 3, 1, 2 }, { 3, 2, 1 } };
		  
		    int facebase = first_face_dof[fnums[j]];

		    dnums[base + 3*j+reorder[facedir][0]-1] = facebase;
		    dnums[base + 3*j+reorder[facedir][1]-1] = facebase + 1;
		    if (gradientface[fnums[j]])
		      dnums[base + 3*j+reorder[facedir][2]-1] = facebase + 2;
		  }	  
		break;
	      }
	    }
	  break;
	}
      case ET_PRISM:
	{
	  switch (order)
	    {
	    case 2:
	      {
		int j, k, ii = 0;
		// all edges
		for (j = 0; j < 9; j++)
		  dnums[ii++] = enums[j];

		// horizontal edges
		for (j = 0; j < 6; j++)
		  dnums[ii++] = 
		    (gradientedge[enums[j]]) ? enums[j] + ned : -1;

		// vertical edges
		for (j = 6; j < 9; j++)
		  for (k = 0; k < zorder-1; k++)
		    dnums[ii++] = 
		      (gradientedge[enums[j]]) ? enums[j] + ned*(k+1) : -1;

		// quad faces:
		for (j = 0; j < 3; j++)
		  {
		    int facebase = first_face_dof[fnums[j+2]];
		    for (k = 0; k < n_quad_face_dofs; k++)
		      dnums[ii++] = facebase + k;
		  }		    
		break;
	      }

	    case 3:
	      {
		int j, k, ii = 0;

		// all edges
		for (j = 0; j < 9; j++)
		  dnums[ii++] = enums[j];

		if (graddom)
		  {
		    // horizontal edges
		    for (j = 0; j < 6; j++)
		      if (gradientedge[enums[j]])
			{
			  dnums[ii++] = enums[j] + ned;
			  dnums[ii++] = enums[j] + 2*ned;
			}
		      else ii+=2;
		  
		    // vertical edges
		    for (j = 6; j < 9; j++)
		      if (gradientedge[enums[j]])
			for (k = 0; k < zorder-1; k++)
			  dnums[ii++] = enums[j] + ned*(k+1);
		      else
			ii += zorder-1;
		  }
	      
		// trig faces:
		for (j = 0; j < 2; j++)
		  {
		    int facedir = forient[j];
		  
		    static const int reorder[8][3] =
		      { { 1, 2, 3 }, { 2, 1, 3 }, { 1, 3, 2 }, { 2, 3, 1 },
			{ 2, 1, 3 }, { 1, 2, 3 }, { 3, 1, 2 }, { 3, 2, 1 } };
		  
		    int facebase = first_face_dof[fnums[j]];
		  
		    dnums[ii+reorder[facedir][0]-1] = facebase;
		    dnums[ii+reorder[facedir][1]-1] = facebase + 1;
		    if (gradientface[fnums[j]])
		      dnums[ii+reorder[facedir][2]-1] = facebase + 2;
		    ii += 3;
		  }

		// quad faces:
		for (j = 0; j < 3; j++)
		  {
		    int facebase = first_face_dof[fnums[j+2]];
		    for (k = 0; k < n_quad_face_dofs; k++)
		      dnums[ii++] = facebase + k;
		    if (!gradientface[fnums[j+2]]) dnums[ii-1] = -1;
		  }		    

		// vol dofs:
		int elbase = first_el_dof[elnr];
		int next = first_el_dof[elnr+1];
		for (k = elbase; k < next; k++)
		  dnums[ii++] = k;

		break;
	      }
	    }
	  break;
	}


      case ET_PYRAMID:
	{
	  switch (order)
	    {
	    case 2:
	      {
		int j, k; 
		// all edges
		for (j = 0; j < 8; j++)
		  dnums[j] = enums[j];
		for (j = 0; j < 8; j++)
		  if (gradientedge[enums[j]])
		    dnums[8+j] = enums[j] + ned;

		// quad face:
		int facebase = first_face_dof[fnums[4]];
		for (k = 0; k < 4; k++)
		  dnums[16+k] = facebase + k;

		break;
	      }
	    case 3:
	      {
		int j, k;
		for (j = 0; j < 8; j++)
		  {
		    int edgenr = enums[j];
		    dnums[j] = edgenr;
		    if (gradientedge[edgenr])
		      {
			dnums[j+8] = edgenr + ned;
			dnums[j+16] = edgenr + 2*ned;
		      }
		  }
		int ii = 24;

		for (j = 0; j < 4; j++)
		  {
		    int facedir = forient[j];
		  
		    static const int reorder[8][3] =
		      { { 1, 2, 3 }, { 2, 1, 3 }, { 1, 3, 2 }, { 2, 3, 1 },
			{ 2, 1, 3 }, { 1, 2, 3 }, { 3, 1, 2 }, { 3, 2, 1 } };
		  
		    int facebase = first_face_dof[fnums[j]];

		    dnums[ii + reorder[facedir][0]-1] = facebase;
		    dnums[ii + reorder[facedir][1]-1] = facebase + 1;
		    if (gradientface[fnums[j]])
		      dnums[ii + reorder[facedir][2]-1] = facebase + 2;
		    ii += 3;
		  }	  

		// quad face:
		int facebase = first_face_dof[fnums[4]];
		for (k = 0; k < n_quad_face_dofs; k++)
		  dnums[ii++] = facebase + k;
		if (!gradientface[fnums[4]]) dnums[ii-1] = -1;
	      
		for (k = 0; k < n_pyramid_el_dofs; k++)
		  dnums[ii++] = first_el_dof[elnr]+k;

		break;
	      }
	    }
	  break;
	}
      default:
	{
	  cerr << "NedelecFE2, GetDofNrs, unkown element" << endl;
	}
      }

    //  (*testout) << "el = " << elnr << ", dnums = " << dnums << endl;
  }



  void NedelecFESpace2 :: GetSDofNrs (int selnr, Array<int> & dnums) const
  {
    int fnum, forient;
    int ena[4], eoa[4];
    Array<int> enums(4, ena), eorient(4, eoa);

    ma.GetSElFace (selnr, fnum, forient);
    ma.GetSElEdges (selnr, enums, eorient);

    LocalHeapMem<1000> lh;
    int nd = GetSFE (selnr, lh).GetNDof();
    dnums.SetSize(nd);
    dnums = -1;

    if (!DefinedOnBoundary (ma.GetSElIndex (selnr)))
      return;

    switch (ma.GetSElType(selnr))
      {
      case ET_TRIG:
	{
	  switch (order)
	    {
	    case 2:
	      {
		for (int j = 0; j < 3; j++)
		  {
		    dnums[j] = enums[j];
		    if (gradientedge[enums[j]])
		      dnums[j+3] = enums[j] + ned;
		  }
		break;
	      }
	    case 3:
	      {
		int j;
		for (j = 0; j < 3; j++)
		  {
		    int edgenr = enums[j];
		    dnums[j] = edgenr;
		    if (gradientedge[edgenr])
		      {
			dnums[j+3] = edgenr + ned;
			dnums[j+6] = edgenr + 2*ned;
		      }
		  }

		int facedir = forient;
	      
		static const int reorder[8][3] =
		  { { 1, 2, 3 }, { 2, 1, 3 }, { 1, 3, 2 }, { 2, 3, 1 },
		    { 2, 1, 3 }, { 1, 2, 3 }, { 3, 1, 2 }, { 3, 2, 1 } };
	      
		int facebase = first_face_dof[fnum];
		dnums[9+reorder[facedir][0]-1] = facebase;
		dnums[9+reorder[facedir][1]-1] = facebase + 1;
		if (gradientface[fnum])
		  dnums[9+reorder[facedir][2]-1] = facebase + 2;
		break;
	      }
	    }
	  break;
	}
      case ET_QUAD:
	{
	  switch (order)
	    {
	    case 2:
	      {
		int j, k, ii = 0;
		// all edges
		for (j = 0; j < 4; j++)
		  dnums[j] = enums[j];
		// horizontal edges
		for (j = 0; j < 2; j++)
		  if (gradientedge[enums[j]])
		    dnums[4+j] = enums[j] + ned;
		ii = 6;
		// vertical edges
		for (j = 2; j < 4; j++)
		  if (gradientedge[enums[j]])
		    for (k = 0; k < zorder-1; k++)
		      dnums[ii++] = enums[j] + ned*(k+1);
		  else
		    ii += zorder-1;

		int facebase = first_face_dof[fnum];
		for (k = 0; k < n_quad_face_dofs; k++)
		  dnums[ii++] = facebase + k;
		break;
	      }
	    case 3:
	      {
		int j, k, ii = 0;
		// all edges
		for (j = 0; j < 4; j++)
		  dnums[ii++] = enums[j];

		// horizontal edges
		for (j = 0; j < 2; j++)
		  if (gradientedge[enums[j]])
		    {
		      dnums[ii++] = enums[j] + ned;
		      dnums[ii++] = enums[j] + 2*ned;
		    }
		  else 
		    ii+=2;

		// vertical edges
		for (j = 2; j < 4; j++)
		  if (gradientedge[enums[j]])
		    for (k = 0; k < zorder-1; k++)
		      dnums[ii++] = enums[j] + ned*(k+1);
		  else
		    ii+=zorder-1;

		int facebase = first_face_dof[fnum];
		for (k = 0; k < n_quad_face_dofs; k++)
		  dnums[ii++] = facebase + k;
		if (!gradientface[fnum]) dnums[ii-1] = -1;
		break;
	      }
	    }
	  break;
	}
      case ET_SEGM:
	{
	  for (int k = 0; k < order; k++)
	    dnums[k] = enums[0] + k * ned;
	  break;
	}
      default:
	{
	  throw Exception ("Unhandled Element in GetSDofNrs");
	}
      }

    //  (*testout) << "sel = " << selnr << ", dnums = " << dnums << endl;
  }




  void NedelecFESpace2 ::GetTransformation (ELEMENT_TYPE eltype, 
					    int elnr,
					    const Array<int> & eorient,
					    const Array<int> & forient,
					    FlatVector<double> & fac) const
  {
    bool graddom = gradientdomains[ma.GetElIndex(elnr)];

    fac = 1.0;
    switch (eltype)
      {
      case ET_SEGM:
	{
	  fac(0) = eorient[0];
	  if (order >= 3) fac(2) = eorient[0];
	  break;
	}

      case ET_TRIG:
	{
	  for (int i = 0; i < 3; i++)
	    {
	      fac(i) = eorient[i];
	      if (order >= 3) fac(i+6) = eorient[i];
	    }
	  break;
	}

      case ET_QUAD:
	{
	  int i, j, k, ii;

	  for (i = 0; i < 4; i++)
	    fac(i) = eorient[i];
	  ii = 4;

	  for (i = 0; i < 2; i++)
	    {
	      if (order >= 3)
		fac(ii+1) = eorient[i];
	      ii += order-1;
	    }

	  for (i = 0; i < 2; i++)
	    {
	      if (zorder >= 3)
		fac(ii+1) = eorient[i+2];
	      ii += zorder-1;
	    }

	
	  int nmx = order * (zorder-1);
	  // int nmy = (order-1) * zorder;

	  // vertical flip
	  if (forient[0] & 1)
	    { 
	      // horizontal moments
	      for (j = 0; j < order; j++)
		for (k = 1; k < zorder-1; k+=2)
		  fac(ii+j*(zorder-1)+k) *= -1;

	      // vertical moments
	      for (j = 0; j < order-1; j++)
		for (k = 0; k < zorder; k+=2)
		  fac(ii+nmx+j*zorder+k) *= -1;
	    }

	  // horizontal flip
	  if (forient[0] & 2)
	    {
	      // horizontal moments
	      for (j = 0; j < order; j+=2)
		for (k = 0; k < zorder-1; k++)
		  fac(ii+j*(zorder-1)+k) *= -1;

	      // vertical moments
	      for (j = 1; j < order-1; j+=2)
		for (k = 0; k < zorder; k++)
		  fac(ii+nmx+j*zorder+k) *= -1;
	    }
	  break;
	}
      
      case ET_TET:
	{
	  if (graddom)
	    for (int i = 0; i < 6; i++)
	      {
		fac(i) = eorient[i];
		if (order >= 3)
		  fac(i+12) = eorient[i];
	      }
	  else
	    for (int i = 0; i < 6; i++)
	      fac(i) = eorient[i];
	  break;
	}
      

      case ET_PRISM:
	{
	  int i, j, k, ii;
	  for (i = 0; i < 9; i++)
	    fac(i) = eorient[i];
	  ii = 9;

	  if (graddom)
	    {
	      for (i = 0; i < 6; i++)
		{
		  if (order >= 3)
		    fac(ii+1) = eorient[i];
		  ii += order-1;
		}
	    
	      for (i = 0; i < 3; i++)
		{
		  if (zorder >= 3)
		    fac(ii+1) = eorient[i+6];
		  ii += zorder-1;
		}
	    }

	  // trig faces:
	  if (order == 3)
	    ii += 6;


	
	  int nmx = order * (zorder-1);
	  // int nmy = (order-1) * zorder;

	  for (i = 2; i < 5; i++)
	    {
	      if (forient[i] & 1)
		{
		  for (j = 0; j < order; j++)
		    for (k = 1; k < zorder-1; k+=2)
		      fac(ii+j*(zorder-1)+k) *= -1;
		
		  for (j = 0; j < order-1; j++)
		    for (k = 0; k < zorder; k+=2)
		      fac(ii+nmx+j*zorder+k) *= -1;
		}
	    
	      if (forient[i] & 2)
		{
		  for (j = 0; j < order; j+=2)
		    for (k = 0; k < zorder-1; k++)
		      fac(ii+j*(zorder-1)+k) *= -1;
		
		  for (j = 1; j < order-1; j+=2)
		    for (k = 0; k < zorder; k++)
		      fac(ii+nmx+j*zorder+k) *= -1;
		}
	      ii += n_quad_face_dofs;
	    }
	  break;
	}



      case ET_PYRAMID:
	{
	  int i, ii;
	  for (i = 0; i < 8; i++)
	    {
	      fac(i) = eorient[i];
	      if (order >= 3)
		fac(i+16) = eorient[i];
	    }

	  ii = 8*order + 4 * n_trig_face_dofs;

	  // quad face:
	  if (order == 2 && zorder == 1)
	    {
	      if (forient[4] & 1)
		fac(ii) *= -1;
	      ii += n_quad_face_dofs;
	    }
	
	  if (order == 2 && zorder == 2)
	    {
	      if (forient[4] & 1)
		fac(ii+2) = -1;
	      if (forient[4] & 2)
		fac(ii) = -1;
	      ii += n_quad_face_dofs;
	    }

	  if (order == 2 && zorder == 3)
	    {
	      // x-components: 0,1,2,3
	      // y-components: 4,5,6
	      if (forient[4] & 1)
		{
		  fac(ii+1) *= -1;
		  fac(ii+3) *= -1;
		  fac(ii+4) *= -1;
		  fac(ii+6) *= -1;
		}
	      if (forient[4] & 2)
		{
		  fac(ii) *= -1;
		  fac(ii+1) *= -1;
		}
	      ii += n_quad_face_dofs;
	    }	

	
	  if (order == 3 && zorder == 1)
	    {
	      if (forient[4] & 1)
		{ // vertical flip
		  fac(ii) *= -1;
		  fac(ii+1) *= -1;
		}
	    
	      if (forient[4] & 2)
		{ // horizontal flip
		  fac(ii+1) *= -1;
		}
	      ii += n_quad_face_dofs;
	    }


	  if (order == 3 && zorder == 2)
	    {
	      if (forient[4] & 1)
		{ // vertical flip
		  fac(ii+3) *= -1;
		  fac(ii+5) *= -1;
		}
		
	      if (forient[4] & 2)
		{ // horizontal flip
		  fac(ii+0) *= -1;
		  fac(ii+2) *= -1;
		  fac(ii+4) *= -1;
		  fac(ii+5) *= -1;
		}
	      ii += n_quad_face_dofs;
	    }
	
	  if (order == 3 && zorder == 3)
	    {
	      if (forient[4] & 1)
		{ // vertical flip
		  fac(ii+1) *= -1;
		  fac(ii+3) *= -1;
		  fac(ii+5) *= -1;
		  fac(ii+6) *= -1;
		  fac(ii+8) *= -1;
		  fac(ii+9) *= -1;
		  fac(ii+11) *= -1;
		}
	    
	      if (forient[4] & 2)
		{ // horizontal flip
		  fac(ii+0) *= -1;
		  fac(ii+1) *= -1;
		  fac(ii+4) *= -1;
		  fac(ii+5) *= -1;
		  fac(ii+9) *= -1;
		  fac(ii+10) *= -1;
		  fac(ii+11) *= -1;
		}
	      ii += n_quad_face_dofs;
	    }
	  break;
	}

      default:
        {
          cerr << "unhandled case 152345" << endl;
          break;
        }

      }
  }




  template <class MAT>
  void NedelecFESpace2::TransformMat (int elnr, bool boundary,
				      MAT & mat, TRANSFORM_TYPE tt) const
  {
    int nd;
    ELEMENT_TYPE et;
    ArrayMem<int,12> enums, eorient;
    ArrayMem<int,6> fnums, forient;
    LocalHeapMem<1000> lh;

    if (boundary)
      {
	nd = GetSFE (elnr, lh).GetNDof();
	et = ma.GetSElType (elnr);
	ma.GetSElEdges (elnr, enums, eorient);
	ma.GetSElFace (elnr, fnums[0], forient[0]);
      }
    else
      {
	nd = GetFE (elnr, lh).GetNDof();
	et = ma.GetElType (elnr);
	ma.GetElEdges (elnr, enums, eorient);
	ma.GetElFaces (elnr, fnums, forient);
      }

  
    ArrayMem<double, 100> mem(nd);
    FlatVector<double> fac(nd, &mem[0]);

    GetTransformation (et, elnr, eorient, forient, fac);

    int i, j, k, l;
    if (tt & TRANSFORM_MAT_LEFT)
      for (k = 0; k < dimension; k++)
	for (i = 0; i < nd; i++)
	  for (j = 0; j < mat.Width(); j++)
	    mat(k+i*dimension, j) *= fac(i);
  
    if (tt & TRANSFORM_MAT_RIGHT)
      for (l = 0; l < dimension; l++)
	for (i = 0; i < mat.Height(); i++)
	  for (j = 0; j < nd; j++)
	    mat(i, l+j*dimension) *= fac(j);
  }







  template <class VEC>
  void NedelecFESpace2::TransformVec (int elnr, bool boundary,
				      VEC & vec, TRANSFORM_TYPE tt) const
  {
    int nd;
    ELEMENT_TYPE et;
    /*
      int ena[12], eoa[12];
      int fna[12], foa[12];
      Array<int> enums(12, ena), eorient(12, eoa);
      Array<int> fnums(6, fna), forient(6, foa);
    */
    ArrayMem<int,12> enums, eorient;
    ArrayMem<int,6> fnums, forient;
    LocalHeapMem<1000> lh;

    if (boundary)
      {
	nd = GetSFE (elnr, lh).GetNDof();
	et = ma.GetSElType (elnr);
	ma.GetSElEdges (elnr, enums, eorient);
	ma.GetSElFace (elnr, fnums[0], forient[0]);
      }
    else
      {
	nd = GetFE (elnr, lh).GetNDof();
	et = ma.GetElType (elnr);
	ma.GetElEdges (elnr, enums, eorient);
	ma.GetElFaces (elnr, fnums, forient);
      }

    ArrayMem<double, 100> mem(nd);
    FlatVector<double> fac(nd, &mem[0]);

    GetTransformation (et, elnr, eorient, forient, fac);

    for (int k = 0; k < dimension; k++)
      for (int i = 0; i < nd; i++)
	vec(k+i*dimension) *= fac(i);
  }



  template
  void NedelecFESpace2::TransformVec<FlatVector<double> >
  (int elnr, bool boundary, FlatVector<double> & vec, TRANSFORM_TYPE tt) const;
  template
  void NedelecFESpace2::TransformVec<FlatVector<Complex> >
  (int elnr, bool boundary, FlatVector<Complex> & vec, TRANSFORM_TYPE tt) const;

  template
  void NedelecFESpace2::TransformMat<FlatMatrix<double> > 
  (int elnr, bool boundary, FlatMatrix<double> & mat, TRANSFORM_TYPE tt) const;
  template
  void NedelecFESpace2::TransformMat<FlatMatrix<Complex> > 
  (int elnr, bool boundary, FlatMatrix<Complex> & mat, TRANSFORM_TYPE tt) const;








  void NedelecFESpace2 ::
  SetGradientDomains (const BitArray & adoms)
  {
    gradientdomains = adoms;
  }

  void NedelecFESpace2 ::
  SetGradientBoundaries (const BitArray & abnds)
  {
    gradientboundaries = abnds;
  }



  Table<int> * NedelecFESpace2 :: 
  CreateSmoothingBlocks (int type) const
  {
    cout << "Ned2, CreateSmoothingblocks, type = " << type << endl;
    int ne = ma.GetNE();
    // int nse = ma.GetNSE();
    int nv = ma.GetNV();
    // int nd = nv+ned + nfa + ne;
    // int level = ma.GetNLevels()-1;

    int i, j, k, l;

    ArrayMem<int,12> pnums;
    ArrayMem<int,37> dnums, dcluster;
    ArrayMem<int,12> enums, eorient, fnums, forient, fpnum1, fpnum2;
    ArrayMem<int,12> ecluster, fcluster;

    // int elcluster;
    Table<int> * it = NULL;
    // const NedelecFESpace & fe1 = 
    // dynamic_cast<const NedelecFESpace&> (*low_order_space);

  
  
    switch (type)
      {
     
      case NedelecFESpace::SB_AFW:
	{
	  cout << " Take Smoothing Block Type SB_AFW " << endl; 
	  // all edge-dofs in common with vertex-representant
	  // all vertically aligned edges and faces
	  // all vertically aligned faces and elements
	
	  // non-overlapping small blocks
	  Array<int> cnts(nv+ned+nfa+ne);
	  for (k = 1; k <= 2; k++)
	    {
	      if (k == 2)
		it = new Table<int>(cnts);
	      cnts = 0;
	      for (i = 0; i < ned; i++)
		{
		  int nd, cl;
		  int ecl = ma.GetClusterRepEdge(i);
		  if (ecl < 0) continue;
		  if (ecl < nv)
		    { // vertical edge -> assign to edge-block (not ecluster = vertex)
		      cl = i+nv;
		      nd = n_z_edge_dofs;
		    }
		  else
		    { // plane edge -> assign to edge-cluster
		      cl = ecl;
		      nd = n_plane_edge_dofs;
		    }
	
		  if (!gradientedge[i] && nd > 1)
		    nd = 1;
		
		  if (k == 1)
		    cnts[cl] += nd;
		  else
		    for (l = 0; l < nd; l++)
		      (*it)[cl][cnts[cl]++] = i + l * ned;
		}
	    
	      for (i = 0; i < nfa; i++)
		{
		  int cl = ma.GetClusterRepFace (i);
		  if (cl < 0) continue;

		  int nd = first_face_dof[i+1] - first_face_dof[i];
		  if (!gradientface[i])
		    {
		      if (nd == 3) nd = 2;
		      if (nd == 4) nd = 3;
		    }
		  // if (nd == 3 && !gradientface[i]) nd = 2;

		  if (k == 1)
		    cnts[cl] += nd;
		  else
		    for (l = 0; l < nd; l++)
		      (*it)[cl][cnts[cl]++] = first_face_dof[i] + l;

		
		}
	      for (i = 0; i < nel; i++)
		{
		  int cl = ma.GetClusterRepElement (i);
		  int nd = first_el_dof[i+1] - first_el_dof[i];
		
		  if (k == 1)
		    cnts[cl] += nd;
		  else
		    for (l = 0; l < nd; l++)
		      (*it)[cl][cnts[cl]++] = first_el_dof[i] + l;

		  // test ... 
		  // if (ma.GetElType(i) == ET_PYRAMID)
		  {
		    ma.GetElFaces (i, fnums, forient);
		    for (j = 0; j < forient.Size(); j++)
		      {		
			int fcl = ma.GetClusterRepFace (fnums[j]);
			// int fcl = nv+ned+fnums[j];
			if (fcl != cl)
			  {
			    if (k == 1)
			      cnts[fcl] += nd;
			    else
			      for (l = 0; l < nd; l++)
				(*it)[fcl][cnts[fcl]++] = first_el_dof[i] + l;
			  }
		      }
		  }
		}

	      for (i = 0; i < ned; i++)
		{
		  int ecl = ma.GetClusterRepEdge (i);
		  if (ecl < 0) continue;

		  int pi1, pi2;
		  ma.GetEdgePNums (i, pi1, pi2);
		  pi1 = ma.GetClusterRepVertex (pi1);
		  pi2 = ma.GetClusterRepVertex (pi2);

		  int nd = (pi1 == pi2) 
		    ? n_z_edge_dofs
		    : n_plane_edge_dofs;

		  if (!gradientedge[i] && nd > 1)
		    nd = 1;

		  if (k == 1)
		    {
		      cnts[pi1] += nd;
		      if (pi1 != pi2)
			cnts[pi2] += nd;
		    }
		  else
		    {
		      for (l = 0; l < nd; l++)
			{
			  (*it)[pi1][cnts[pi1]++] = i + l * ned;
			  if (pi1 != pi2)
			    (*it)[pi2][cnts[pi2]++] = i + l * ned;
			}
		    }
		}


	      for (i = 0; i < nfa; i++)
		{
		  int cl = ma.GetClusterRepFace (i);
		  if (cl < 0) continue;
		
		  ma.GetFaceEdges (i, enums);

		  int nd = first_face_dof[i+1] - first_face_dof[i];
		  if (nd == n_quad_face_dofs) continue;
		  // if (nd == 3 && !gradientface[i]) nd = 2;
		  if (!gradientface[i])
		    {
		      if (nd == 3) nd = 2;
		      if (nd == 4) nd = 3;
		    }

		  for (j = 0; j < enums.Size(); j++)
		    {
		      //		    int pi = nv + enums[j];
		      int ecl = ma.GetClusterRepEdge (enums[j]);

		      if (ecl == cl) continue;


		      /*
		      // edges on face cluster
		      int pi1, pi2;
		      ma.GetEdgePNums (enums[j], pi1, pi2);
		      pi1 = ma.GetClusterRepVertex (pi1);
		      pi2 = ma.GetClusterRepVertex (pi2);

		      int nedof;
		      if (pi1 == pi2)
		      nedof = n_z_edge_dofs;
		      else
		      nedof = n_plane_edge_dofs;

		      if (k == 1)
		      cnts[cl] += nedof;
		      else
		      for (l = 0; l < nedof; l++)
		      (*it)[cl][cnts[cl]++] = enums[j] + l * ned;
		      */

		      // face on eclusters:
		      if (k == 1)
			cnts[ecl] += nd;
		      else 
			for (l = 0; l < nd; l++)
			  (*it)[ecl][cnts[ecl]++] = first_face_dof[i] + l;
		    }
		}


	      /*	    
		BitArray prism_faces(nfa);
		prism_faces.Clear();
		for (i = 0; i < -nel; i++)
		{
		ma.GetElFaces (i, fnums, forient);
		if (fnums.Size() == 4)
		for (j = 0; j < 4; j++)
		prism_faces.Set(fnums[j]);
		}
		for (i = 0; i < -nse; i++)
		{
		int fnr, fori;
		ma.GetSElFace (i, fnr, fori);
		prism_faces.Set(fnr);
		}

		for (i = 0; i < nfa; i++)
		{
		if (!prism_faces[i]) continue;

		int cl = ma.GetClusterRepFace (i);
		if (cl < 0) continue;
		
		ma.GetFacePNums (i, pnums);
		int nd = first_face_dof[i+1] - first_face_dof[i];
		
		if (pnums.Size() == 4)
		{ // quad... use diagonal
		pnums[1] = pnums[2];
		pnums.SetSize(2);
		}

		for (j = 0; j < pnums.Size(); j++)
		{
		int pi = ma.GetClusterRepVertex (pnums[j]);
		if (k == 1)
		cnts[pi] += nd;
		else
		for (l = 0; l < nd; l++)
		(*it)[pi][cnts[pi]++] = first_face_dof[i] + l;
		}
		}
	      */
	    
	      /*
		Array<int> fpnums;
		for (i = 0; i < nfa; i++)
		{
		ma.GetFacePNums (i, fpnums);
		for (j = 0; j < fpnums.Size(); j++)
		fpnums[j] = ma.GetClusterRepVertex (fpnums[j]);
	      
		for (j = 0; j < fpnums.Size(); j++)
		{
		bool dup = 0;
		for (l = 0; l < j; l++)
		if (fpnums[j] == fpnums[l])
		dup = 1;

		if (!dup)
		{
		int nd = first_face_dof[i+1] - first_face_dof[i];
		int pi = fpnums[j];
		if (k == 1)
		{
		cnts[pi] += nd;
		}
		else
		{
		for (l = 0; l < nd; l++)
		{
		(*it)[pi][cnts[pi]] = first_face_dof[i] + l;
		cnts[pi]++;
		}
		}
		}
		}
		}
	      */
	    }
	
	  //(*testout) << "AFW Blocks: " << (*it) << endl;
	  break;
	}


      case  NedelecFESpace::SB_HIPTMAIR:
	{
	  // all edge-dofs in common with vertex-representant
	  // all vertically aligned edges and faces
	  // all vertically aligned faces and elements
	
	  // non-overlapping small blocks
	  Array<int> cnts(nv+ned+nfa+ne);
	  cout << " Take Smoothing Block Type SB_HIPTMAIR " << endl; 
	  for (k = 1; k <= 2; k++)
	    {
	      if (k == 2)
		it = new Table<int>(cnts);
	      cnts = 0;
	    
	      for (i = 0; i < ned; i++)
		{
		  int nd, cl;
		  int ecl = ma.GetClusterRepEdge(i);
		  if (ecl < 0) continue;
		  if (ecl < nv)
		    {
		      cl = i+nv;
		      nd = n_z_edge_dofs;
		    }
		  else
		    {
		      cl = ecl;
		      nd = n_plane_edge_dofs;
		    }
		
		  if (k == 1)
		    {
		      cnts[cl] += nd;
		    }
		  else
		    {
		      for (l = 0; l < nd; l++)
			(*it)[cl][cnts[cl]++] = i + l * ned;
		    }
		}
	    
	      for (i = 0; i < nfa; i++)
		{
		  int cl = ma.GetClusterRepFace (i);
		  if (cl < 0) continue;

		  int nd = first_face_dof[i+1] - first_face_dof[i];

		  if (k == 1)
		    {
		      cnts[cl] += nd;
		    }
		  else
		    {
		      for (l = 0; l < nd; l++)
			(*it)[cl][cnts[cl]++] = first_face_dof[i] + l;
		    }
		}
	    
	      for (i = 0; i < nel; i++)
		{
		  int cl = ma.GetClusterRepElement (i);
		  int nd = first_el_dof[i+1] - first_el_dof[i];
		
		  if (k == 1)
		    {
		      cnts[cl] += nd;
		    }
		  else
		    {
		      for (l = 0; l < nd; l++)
			(*it)[cl][cnts[cl]++] = first_el_dof[i] + l;
		    }
		}
	    }



	  break;
	}


      case  NedelecFESpace::SB_POTENTIAL:
	{
	  Array<int> cnts(nv+ned);
	  for (int k = 1; k <= 2; k++)
	    {
	      if (k == 2)
		it = new Table<int>(cnts);
	    
	      cnts = 0;
	    
	      for (int j = 0; j < nv; j++)
		{
		  int vcl = ma.GetClusterRepVertex (j);
		  if (k == 2)
		    {
		      (*it)[vcl][cnts[vcl]++] = j;
		    }
		  else
		    {
		      cnts[vcl]++;
		    }
		}


	      for (i = 0; i < ned; i++)
		{
		  int nd, cl;
		  int ecl = ma.GetClusterRepEdge(i);
		  if (ecl < 0) continue;

		  if (ecl < nv)
		    { // vertical edge
		      cl = i+nv;
		      nd = n_z_edge_dofs-1;
		    }
		  else
		    {
		      cl = ecl;
		      nd = n_plane_edge_dofs-1;
		    }

		  if (k == 1)
		    {
		      cnts[cl] += nd;
		    }
		  else
		    {
		      for (l = 0; l < nd; l++)
			{
			  (*it)[cl][cnts[cl]] = i + l * ned + nv;
			  cnts[cl]++;
			}
		    }
		}
	    }
	  break;
	}
      }
  
    // (*testout) << "Nedelec2, Smoothingblocks type = " << type << endl << (*it) << endl;
    return it;
  }

  BitArray * NedelecFESpace2 :: 
  CreateIntermediatePlanes (int type) const
  {
    BitArray & ba = *new BitArray (GetNDof());
    ba.Clear();

    int i;
    for (i = 0; i < ned; i++)
      {
	int pi1, pi2;
	ma.GetEdgePNums (i, pi1, pi2);
	  
	if (ma.GetClusterRepVertex (pi1) ==
	    ma.GetClusterRepVertex (pi2))
	  {
	    for (int l = 1; l < n_z_edge_dofs; l++)
	      ba.Set (i + l * ned);
	  }
      }


    for (i = 0; i < nfa; i++)
      {
	int first = first_face_dof[i];
	int nd = first_face_dof[i+1] - first;
	if (nd == n_quad_face_dofs)
	  {

	    if (order == 2 && zorder == 1)
	      {
		ba.Set (first);
	      }
	  
	    if (order == 2 && zorder == 2)
	      {
		ba.Set (first + 0);
		ba.Set (first + 2);
		ba.Set (first + 3);
	      }

	    if (order == 2 && zorder == 3)
	      {
		ba.Set (first + 0);
		ba.Set (first + 1);
		ba.Set (first + 5);
		ba.Set (first + 6);
	      }

	    if (order == 3 && zorder == 1)
	      {
		ba.Set (first + 0);
		ba.Set (first + 1);
	      }

	    if (order == 3 && zorder == 2)
	      {
		ba.Set (first + 0);
		ba.Set (first + 3);
		ba.Set (first + 4);

		/*
		  for (int l = 0; l < 7; l++)
		  ba.Set (first+l);
		*/
	      }

	    if (order == 3 && zorder == 3)
	      {
		ba.Set (first + 0);
		ba.Set (first + 1);
		ba.Set (first + 6);
		ba.Set (first + 7);
		ba.Set (first + 8);
	      }
	  }
      }

    /*
      for (i = first_el_dof[0]; i < first_el_dof[nel]; i++)
      ba.Set(i);
    */
    return &ba;
  }


  Array<int> * 
  NedelecFESpace2 :: CreateDirectSolverClusters (const Flags & flags) const
  {
    (*testout) << "CreateDirectSolverClusters" << endl;

    // int nv = ma.GetNV();
    int nd = GetNDof();
    int ne = ma.GetNE();


    Array<int> & clusters = *new Array<int> (nd);
    clusters = 0;


    int i,k;
  
    //lo 
    //for(i=0;i<ma.GetNEdges();i++)
    //  clusters[i]=1; 

    //
    for (i = 0; i < ned; i++)
      {
	int pi1, pi2;
	ma.GetEdgePNums (i, pi1, pi2);
	  
	if (ma.GetClusterRepVertex (pi1) ==
	    ma.GetClusterRepVertex (pi2))
	  {
	    for (int l = 1; l < n_z_edge_dofs; l++)
	      clusters[i + l * ned] = 1;
	  }
      }

  
    for (i = 0; i < nfa; i++)
      {
	int first = first_face_dof[i];
	int nd = first_face_dof[i+1] - first;
	if (nd == n_quad_face_dofs)
	  {

	    if (order == 2 && zorder == 1)
	      {
		clusters[first] = 1;
	      }
	  
	    if (order == 2 && zorder == 2)
	      {
		clusters[first + 0] = 1;
		clusters[first + 2] = 1;
		clusters[first + 3] = 1;
	      }

	    if (order == 2 && zorder == 3)
	      {
		clusters[first + 0] = 1;
		clusters[first + 1] = 1;
		clusters[first + 5] = 1;
		clusters[first + 6] = 1;
	      }

	    if (order == 3 && zorder == 1)
	      {
		clusters[first + 0] = 1;
		clusters[first + 1] = 1;
	      }

	    if (order == 3 && zorder == 2)
	      {
		clusters[first + 0] = 1;
		clusters[first + 3] = 1;
		clusters[first + 4] = 1;

		/*
		  for (int l = 0; l < 7; l++)
		  ba.Set (first+l);
		*/
	      }

	    if (order == 3 && zorder == 3)
	      {
		clusters[first + 0] = 1;
		clusters[first + 1] = 1;
		clusters[first + 6] = 1;
		clusters[first + 7] = 1;
		clusters[first + 8] = 1;
	      }
	  }
      }


    //


    Array<int> dnums;
  

    for(i=0; i<ne && (directsolverclustered.Size() > 0 || directsolvermaterials.Size() > 0); i++)
      {
	if((directsolverclustered.Size() > 0 && directsolverclustered[ma.GetElIndex(i)]) || 
	   directsolvermaterials.Contains(ma.GetElMaterial(i)))
	  {     
	    ELEMENT_TYPE eltype = ma.GetElType(i);
	    if(eltype != ET_PRISM) continue;

	    GetDofNrs(i,dnums);
	    for(k=0; k<dnums.Size(); k++)
	      if(dnums[k] >= 0) clusters[dnums[k]] = 2;

	  }
      }
    
    for(i=0; i< adddirectsolverdofs.Size(); i++)
      {
	clusters[adddirectsolverdofs[i]] = 2;
      }


    //   int clusternum = 2;
    //   for(i=0; i<ne; i++)
    //     {
    //       ELEMENT_TYPE eltype = ma.GetElType(i);
    //       if(eltype == ET_PRISM)
    // 	{
    // 	  GetDofNrs(i,dnums);
    // 	  for(k=0; k<dnums.Size(); k++)
    // 	    {
    // 	      if(dnums[k] >= 0) clusters[dnums[k]] = clusternum;
    // 	    }
    // 	  //clusternum++;
    // 	}
    //     }
	  
	 
    return &clusters;
    

  }



  SparseMatrix<double> * 
  NedelecFESpace2 :: CreateGradient() const
  {
    cout << "update gradient, N2" << endl;
    int i, j;
    int nv = ma.GetNV();
    int level = ma.GetNLevels()-1;
    const NedelecFESpace & fe1 = 
      dynamic_cast<const NedelecFESpace&> (*low_order_space);

    Array<int> cnts(GetNDof());
    cnts = 0;
    for (i = 0; i < ned; i++)
      {
	if (fe1.FineLevelOfEdge(i) == level)
	  {
	    cnts[i] = 2;
	    for (j = 1; j < n_edge_dofs; j++)
	      cnts[i+j*ned] = 1;
	  }
      }
  

    SparseMatrix<double> & grad = *new SparseMatrix<double>(cnts);

    for (i = 0; i < ned; i++)
      {
	if (fe1.FineLevelOfEdge(i) < level) continue;
	int pi1, pi2;
	ma.GetEdgePNums (i, pi1, pi2);
	grad.CreatePosition (i, pi1);
	grad.CreatePosition (i, pi2);
      }

    for (i = 0; i < ned; i++)
      {
	if (fe1.FineLevelOfEdge(i) < level) continue;
	int pi1, pi2;
	ma.GetEdgePNums (i, pi1, pi2);
	grad(i, pi1) = 1;
	grad(i, pi2) = -1;
      }


    for (i = 0; i < ned; i++)
      {
	if (fe1.FineLevelOfEdge(i) == level)
	  {
	    for (j = 1; j < n_edge_dofs; j++)
	      grad.CreatePosition(i+j*ned, i+(j-1)*ned+nv);
	  }
      }
    for (i = 0; i < ned; i++)
      {
	if (fe1.FineLevelOfEdge(i) == level)
	  {
	    for (j = 1; j < n_edge_dofs; j++)
	      grad(i+j*ned, i+(j-1)*ned+nv) = 1;
	  }
      }

    (*testout) << "grad, p2 = " << grad << endl;
    return &grad;
  }





  void NedelecFESpace2 :: 
  LockSomeDofs (BaseMatrix & mat) const
  {
  
    cout << "Lock hanging dofs" << endl;

    int eled, elfa;
    int i, j, k;
    int ne = ma.GetNE();
    eled = 8;
    elfa = 5;


    Matrix<double> elmat(1);
    Matrix<Complex> elmatc(1);
    elmat(0,0) = 1e15;
    elmatc(0,0) = 1e15;
    Array<int> dnums(1);

    Array<int> fnums, forient;
    Array<int> enums, eorient;
    Array<int> lock;

    cout << "type is " << typeid(mat).name() << endl;
    SparseMatrixSymmetric<Mat<1,1,double> > & smat =
      dynamic_cast<SparseMatrixSymmetric<Mat<1,1,double> > &> (mat);
  
    for (i = 0; i < ne; i++)
      {
	lock.SetSize (0);
	switch (ma.GetElType(i))
	  {
	  case ET_PRISM:
	    {
	      ma.GetElFaces (i, fnums, forient);
	      ma.GetElEdges (i, enums, eorient);
	    
	      if (order == 3)
		{
		  // lock 3rd vert. edge dofs and trig face dofs
		  for (j = 6; j < 9; j++)
		    {
		      lock.Append (3 * enums[j]);
		    }
		  for (j = 0; j < 2; j++)
		    {
		      int base = first_face_dof[fnums[j]];
		      for (k = 0; k < n_trig_face_dofs; k++)
			lock.Append (base+k);
		    }
		}
	      break;
	    }
	  default:
	    { 
	      ;
	    }
	  }
      
	for (k = 0; k < lock.Size(); k++)
	  {
	    smat(lock[k], lock[k]) (0,0) += 1e15;
	  }
      }



    /*
      if (ma.GetElType(elnr) == ET_PYRAMID)
      {

      switch (type)
      {
      case N2:
      {
      for (j = 1; j <= eled; j++)
      {
      //		dnums.Elem(1) = abs (elementedges.Get(elnr)[j-1]) + ned;
      dnums.Elem(1) = enums.Elem(j) + ned;
      mat.AddElementMatrix (dnums, elmat);
      }
      for (j = 1; j <= elfa; j++)
      {
      dnums.Elem(1) =  2*ned + 2*fnums.Get(j) - 1;
      //		dnums.Elem(1) =  2*ned + 2*elementfaces.Get(elnr)[j-1] - 1;
      mat.AddElementMatrix (dnums, elmat);
      dnums.Elem(1)++;
      mat.AddElementMatrix (dnums, elmat);
      }  
      break;
      }
      case BDM1:
      {
      for (j = 1; j <= elfa; j++)
      {
      dnums.Elem(1) =  2*ned + fnums.Get(j);
      mat.AddElementMatrix (dnums, elmat);
      }  
      break;
      }
      case BDM2:
      {
      for (j = 1; j <= eled; j++)
      {
      //		int edge = abs (elementedges.Get(elnr)[j-1]);
      int edge = enums.Get(j);
      dnums.Elem(1) = edge + 2 * ned;
      mat.AddElementMatrix (dnums, elmat);
      }
      for (j = 1; j <= elfa; j++)
      {
      dnums.Elem(1) =  3*ned + 3*fnums.Get(j) - 2;
      //		dnums.Elem(1) =  3*ned + 3*elementfaces.Get(elnr)[j-1] - 2;
      mat.AddElementMatrix (dnums, elmat);
      dnums.Elem(1)++;
      mat.AddElementMatrix (dnums, elmat);
      dnums.Elem(1)++;
      mat.AddElementMatrix (dnums, elmat);
      }  
      break;
      }
      }
      }
    */
  }


  void NedelecFESpace2 :: GetVertexDofNrs (int vnr, Array<int> & dnums) const
  {
    dnums.SetSize(0);
  }

  void NedelecFESpace2 :: GetEdgeDofNrs (int ednr, Array<int> & dnums) const
  {
    cout << "EdgeDofs vom Nedelec2 space: SABINE FRAGEN.." << endl;
    dnums.SetSize(0);
  }

  void NedelecFESpace2 :: GetFaceDofNrs (int fanr, Array<int> & dnums) const
  {
    cout << "FaceDofs vom Nedelec2 space: SABINE FRAGEN.." << endl;
    dnums.SetSize(0);
  }

  void NedelecFESpace2 :: GetInnerDofNrs (int elnr, Array<int> & dnums) const
  {
    cout << "InnerDofs vom Nedelec2 space: SABINE FRAGEN.." << endl;
    dnums.SetSize(0);
  }

  // void NedelecFESpace2 :: 
  // AddGradient (double fac, const BaseVector & pot, BaseVector & grad) const
  // {
  //   const MeshAccess & ma = GetMeshAccess();

  //   int ned = ma.GetNEdges();
  
  //   const BaseSystemVector & svpot = 
  //     dynamic_cast<const BaseSystemVector&> (pot);
  //   BaseSystemVector & svgrad = 
  //     dynamic_cast<BaseSystemVector&> (grad);

  //   int sdim = svpot.SystemDim();
  //   if (sdim != 1)
  //     {
  //       cerr << "NedelecFESpace2::AddGradient not implemented for sdim != 1" << endl;
  //       exit(1);
  //     }

  //   int i, j;

  //   const SystemVector<SysVector1d> & svpot1 = 
  //     dynamic_cast<const SystemVector<SysVector1d>&> (pot);
  //   SystemVector<SysVector1d> & svgrad1 = 
  //     dynamic_cast<SystemVector<SysVector1d>&> (grad);

  //   BitArray usededges(ned);
  
  
  //   for (i = 1; i <= ned; i++)
  //     if (space.FineLevelOfEdge (i) >= level)
  //       {
  // 	int ep1 = space.EdgePoint1(i);
  // 	int ep2 = space.EdgePoint2(i);
	
  // 	for (j = 1; j <= sdim; j++)
  // 	  svgrad1.Elem(i, j) += fac * (svpot1.Get (ep1, j) - svpot1.Get(ep2, j));
  //       }

  // }
  // void NedelecFESpace2 ::  
  // ApplyGradientT (int level, const BaseVector & gradt, BaseVector & pott) const
  // {
  // }


#ifdef PARALLEL
  /*
  // old style constructor
  ParallelNedelecFESpace :: ParallelNedelecFESpace (const MeshAccess & ama,
						    int aorder, int adim, bool acomplex, bool adiscontinuous)
    
    : NedelecFESpace (ama, aorder, adim, acomplex, adiscontinuous)
  {
    name="ParallelNedelecFESpace(hcurl)";
  }
  */
  
  // new style constructor
  ParallelNedelecFESpace :: ParallelNedelecFESpace (const MeshAccess & ama, const Flags& flags, bool parseflags)
    : NedelecFESpace (ama, flags, parseflags)
  {
    name="ParallelNedelecFESpace(hcurl)";
  }


  void ParallelNedelecFESpace :: UpdateParallelDofs_loproc()
  {
    if ( discontinuous )
      {
	*testout << "ParallelNedelecFESpace::UpdateParallelDofs_loproc -- discontinuous" << endl;
	
	const MeshAccess & ma = (*this). GetMeshAccess();
	
	int ndof = GetNDof();
	
	// Find number of exchange dofs
	Array<int> nexdof(ntasks); 
	nexdof = 0;
	
	paralleldofs->SetNExDof(nexdof);
	
// 	paralleldofs->localexchangedof = new Table<int> (nexdof);
// 	paralleldofs->distantexchangedof = new Table<int> (nexdof);
	paralleldofs->sorted_exchangedof = new Table<int> (nexdof);
	return;
      }


    *testout << "NedelecFESpace::UpdateParallelDofs_loproc" << endl;

    const MeshAccess & ma = (*this). GetMeshAccess();

    int ndof = GetNDof();

    // Find number of exchange dofs
    Array<int> nexdof(ntasks); 
    nexdof = 0;

    MPI_Status status;
    MPI_Request * sendrequest = new MPI_Request [ntasks];
    MPI_Request * recvrequest = new MPI_Request [ntasks];

    // number of edge exchange dofs
    for ( int edge = 0; edge < ma.GetNEdges(); edge++ )
      {
	nexdof[id] ++;//= dnums.Size() ; 
	for ( int dest = 1; dest < ntasks; dest ++ )
	  if (  parallelma -> GetDistantEdgeNum ( dest, edge ) >= 0 )
	    { 
	      nexdof[dest] ++; 
	    }
      }

    paralleldofs->SetNExDof(nexdof);

    paralleldofs->sorted_exchangedof = new Table<int> (nexdof);
//     paralleldofs->localexchangedof = new Table<int> (nexdof);
//     paralleldofs->distantexchangedof = new Table<int> (nexdof);



    Array<int> ** owndofs,** distantdofs;
    owndofs = new Array<int> * [ntasks];
    distantdofs = new Array<int> * [ntasks];

    for ( int i = 0; i < ntasks; i++)
      {
	owndofs[i] = new Array<int> (1);
	(*owndofs[i])[0] = ndof;
	distantdofs[i] = new Array<int> (0);
      }

    int exdof = 0;
    Array<int> cnt_nexdof(ntasks);
    cnt_nexdof = 0;

    // *****************
    // Parallel Edge dofs
    // *****************


    // find local and distant dof numbers for edge exchange dofs
    for ( int edge = 0; edge < ma.GetNEdges(); edge++ )
      {
	(*(paralleldofs->sorted_exchangedof))[id][exdof++] = edge;

	for ( int dest = 1; dest < ntasks; dest++ )
	  {
	    int distedge = parallelma -> GetDistantEdgeNum ( dest, edge );
	    if( distedge < 0 ) continue;

	    owndofs[dest]->Append ( distedge );
	    paralleldofs->SetExchangeDof ( dest, edge );
	    paralleldofs->SetExchangeDof ( edge );
	    owndofs[dest]->Append ( edge );
  	  }
      }   


    for ( int dest = 1; dest < ntasks; dest ++ )
      {
	MyMPI_ISend ( *owndofs[dest], dest, sendrequest[dest]);
	MyMPI_IRecv ( *distantdofs[dest], dest, recvrequest[dest]);
      }
    


    int ii = 1;
    for( int dest=1; dest<ntasks; dest++) 
      {
	if ( dest == id ) continue;
	MPI_Wait(recvrequest+dest, &status);
	paralleldofs -> SetDistNDof( dest, (*distantdofs[dest])[0]) ;
	ii = 1;
	while ( ii < distantdofs[dest]->Size() )
	  {
	    int ednum = (*distantdofs[dest])[ii++];
	    (*(paralleldofs->sorted_exchangedof))[dest][ cnt_nexdof[dest] ] = ednum;
// 	    (*(paralleldofs->distantexchangedof))[dest][ cnt_nexdof[dest] ] = (*distantdofs[dest])[ii];
	    ii++; cnt_nexdof[dest]++;
	     
	  }
      }

    for ( int dest = id+1; dest < ntasks; dest++ )
      QuickSort ( (*(paralleldofs->sorted_exchangedof))[dest] );

    for ( int i = 0; i < ntasks; i++ )
      {
	delete distantdofs[i];
	delete owndofs[i];
      }

    delete [] owndofs;
    delete [] distantdofs;
    delete [] sendrequest;
    delete [] recvrequest;
  }

  void ParallelNedelecFESpace :: UpdateParallelDofs_hoproc()
  {
    if ( discontinuous )
      {
	*testout << "NedelecFESpace::UpdateParallelDofs_hoproc -- discontinuous" << endl;
	int ndof = GetNDof();
	// Find number of exchange dofs
	Array<int> nexdof(ntasks);
	nexdof = 0;
	
	const MeshAccess & ma = (*this). GetMeshAccess();
	
	paralleldofs->SetNExDof(nexdof);
	
// 	paralleldofs->localexchangedof = new Table<int> (nexdof);
// 	paralleldofs->distantexchangedof = new Table<int> (nexdof);
	paralleldofs->sorted_exchangedof = new Table<int> (nexdof);
	
	for ( int i = 0; i < ndof; i++ )
	  {
	    paralleldofs->ClearExchangeDof(i);
	    for ( int dest = 0; dest < ntasks; dest++ )
	      paralleldofs->ClearExchangeDof(dest, i);
	  }
	
	
	int * ndof_dist = new int[ntasks];
	MPI_Allgather ( &ndof, 1, MPI_INT, &ndof_dist[1], 
			1, MPI_INT, MPI_HIGHORDER_COMM);
	for ( int dest = 0; dest < ntasks; dest++ )
	  paralleldofs -> SetDistNDof( dest, ndof_dist[dest]) ;
	
	delete [] ndof_dist;
	
	return;
      }


    // ******************************
    // update exchange dofs 
    // ******************************
    *testout << "NedelecFESpace::UpdateParallelDofs_hoproc" << endl;
    // Find number of exchange dofs
    Array<int> nexdof(ntasks);
    nexdof = 0;

    MPI_Status status;
    MPI_Request * sendrequest = new MPI_Request [ntasks];
    MPI_Request * recvrequest = new MPI_Request [ntasks];

    // number of edge exchange dofs
    for ( int edge = 0; edge < ma.GetNEdges(); edge++ )
      {
	if ( !parallelma->IsExchangeEdge ( edge ) ) continue;
	nexdof[id] ++ ;  
	
	for ( int dest = 1; dest < ntasks; dest ++ )
	  if (  parallelma -> GetDistantEdgeNum ( dest, edge ) >= 0 )
	    nexdof[dest] ++ ;  
      }


    nexdof[0] = 0;

    paralleldofs->SetNExDof(nexdof);

//     paralleldofs->localexchangedof = new Table<int> (nexdof);
//     paralleldofs->distantexchangedof = new Table<int> (nexdof);
    paralleldofs->sorted_exchangedof = new Table<int> (nexdof);

    Array<int> ** owndofs, ** distantdofs;
    owndofs = new Array<int>* [ntasks];
    distantdofs = new Array<int>* [ntasks];

    for ( int i = 0; i < ntasks; i++ )
      {
	owndofs[i] = new Array<int>(1);
	(*owndofs[i])[0] = GetNDof();
	distantdofs[i] = new Array<int>(0);
      }
    // *****************
    // Parallel Vertex dofs
    // *****************

    Array<int> cnt_nexdof(ntasks);
    cnt_nexdof = 0;
    int exdof = 0;

   
    // *****************
    // Parallel Edge dofs
    // *****************

    for ( int dest = 0; dest < ntasks; dest++)
      {
	owndofs[dest]->SetSize(1);
	distantdofs[dest]->SetSize(0);
      }
    for ( int edge = 0; edge < ma.GetNEdges(); edge++ )
      if ( parallelma->IsExchangeEdge ( edge ) )
	{
	  (*(paralleldofs->sorted_exchangedof))[id][exdof++] = edge;

	  for ( int dest = 1; dest < ntasks; dest++ )
	    {
	      int distedge = parallelma -> GetDistantEdgeNum ( dest, edge );
	      if( distedge < 0 ) continue;

	      owndofs[dest]->Append ( distedge );
	      owndofs[dest]->Append (int(paralleldofs->IsGhostDof(edge)) );
	      paralleldofs->SetExchangeDof ( dest, edge );
	      paralleldofs->SetExchangeDof ( edge );
	      owndofs[dest]->Append ( edge );
	    }
	}   


    for ( int sender = 1; sender < ntasks; sender ++ )
      {
        if ( id == sender )
          for ( int dest = 1; dest < ntasks; dest ++ )
            if ( dest != id )
              {
		MyMPI_ISend ( *owndofs[dest], dest, sendrequest[dest]);
              }
	  
        if ( id != sender )
          {
	    MyMPI_IRecv ( *distantdofs[sender], sender, recvrequest[sender]);
          }
 	  
	  
      }

    for( int dest=1; dest<ntasks; dest++) 
      {
	if ( dest == id ) continue;
	MPI_Wait(recvrequest+dest, &status);
	paralleldofs -> SetDistNDof( dest, (*distantdofs[dest])[0]) ;
	int ii = 1;
	while ( ii < distantdofs[dest]->Size() )
	  {
	    int ednum = (*distantdofs[dest])[ii++];
	    int isdistghost = (*distantdofs[dest])[ii++];
	    (*(paralleldofs->sorted_exchangedof))[dest][ cnt_nexdof[dest] ] = ednum;
// 	    (*(paralleldofs->distantexchangedof))[dest][ cnt_nexdof[dest] ] = (*distantdofs[dest])[ii];
	    if ( dest < id && !isdistghost )
	      paralleldofs->ismasterdof.Clear ( ednum ) ;
	    ii++; cnt_nexdof[dest]++;
	  }
      }

    for ( int dest = id+1; dest < ntasks; dest++ )
      QuickSort ( (*(paralleldofs->sorted_exchangedof))[dest] );

    for ( int i = 0; i < ntasks; i++ )
      {
	delete distantdofs[i];
	delete owndofs[i];
      }

    delete [] owndofs;
    delete [] distantdofs;
    delete [] recvrequest;
    delete  [] sendrequest;

  }
#endif

  /*



  RaviartThomasFESpace :: 
  RaviartThomasFESpace (const MeshAccess & ama,
  RT_TYPE atype) 
  : FESpace (ama)
  {
  ned = 0;
  type = atype;
  }


  RaviartThomasFESpace :: ~RaviartThomasFESpace ()
  {
  ;
  }

  void RaviartThomasFESpace :: Update()
  {
  cout << "Update Raviart Thomas space" << flush;
  int i, j, k;
  const MeshAccess & ma = GetMeshAccess();

  int level = ma.GetNLevels();
  ne = ma.GetNE();
  int nse = ma.GetNSE();
  int np = ma.GetNP();

  ned = ma.GetNEdges();

  cout << ", ndof = " << GetNDof() << endl;
  }




  int RaviartThomasFESpace :: GetNDof () const
  {
  switch (type)
  {
  case RT0:
  // return 3*ne; 
  return ned;
  case BDM1:
  return 2*ned;
  case BDM1p:
  return 2*ned+ne;
  case BDM2:
  return 3*ned+3*ne;
  case BDM2p:
  return 3*ned+5*ne;
  }
  return 0;
  }

  int RaviartThomasFESpace :: GetNDofLevel (int level) const
  {
  return GetNDof();
  }

  const FiniteElement & RaviartThomasFESpace :: GetFE (int elnr) const
  {
  const MeshAccess & ma = GetMeshAccess();

  switch (ma.GetElType(elnr))
  {
  case ET_TRIG:
  {
  switch (type)
  {
  case RT0:
  return rttrig0;
  case BDM1:
  return bdmtrig1;
  case BDM1p:
  return bdmtrig1plus;
  case BDM2:
  return bdmtrig2;
  case BDM2p:
  return bdmtrig2plus;
  }
  break;
  }
  }
  cerr << "RaviartThomasFESpace, GetFE: unknown type" << endl;
  return rttrig0;
  }
  
  void RaviartThomasFESpace :: GetDofNrs (int elnr, Array<int> & dnums) const
  {
  const MeshAccess & ma = GetMeshAccess();
  int eled, elfa;
  int i, j;

  int ena[12], eoa[12];
  Array<int> enums(12, ena), eorient(12, eoa);

  ma.GetElEdges (elnr, enums, eorient);

  switch (ma.GetElType(elnr))
  {
  case ET_TRIG:
  {
  switch (type)
  {
  case RT0:
  {
  dnums.SetSize(3);
  for (j = 1; j <= 3; j++)
  // dnums.Elem(j) = 3*elnr-3+j;
  dnums.Elem(j) = enums.Get(j);
  break;
  }
  case BDM1:
  {
  dnums.SetSize(6);
  for (j = 1; j <= 3; j++)
  {
  dnums.Elem(j) = enums.Get(j);
  dnums.Elem(j+3) = ned+enums.Get(j);
  }
  break;
  }
  case BDM1p:
  {
  dnums.SetSize(7);
  for (j = 1; j <= 3; j++)
  {
  dnums.Elem(j) = enums.Get(j);
  dnums.Elem(j+3) = ned+enums.Get(j);
  }
  dnums.Elem(7) = 2*ned+elnr;
  break;
  }
  case BDM2:
  {
  dnums.SetSize(12);
  for (j = 1; j <= 3; j++)
  {
  dnums.Elem(j) = enums.Get(j);
  dnums.Elem(j+3) = ned+enums.Get(j);
  dnums.Elem(j+6) = 2*ned+enums.Get(j);
  }
  for (j = 1; j <= 3; j++)
  dnums.Elem(j+9) = 3*ned+(elnr-1)*3+j;
  break;
  }
  case BDM2p:
  {
  dnums.SetSize(14);
  for (j = 1; j <= 3; j++)
  {
  dnums.Elem(j) = enums.Get(j);
  dnums.Elem(j+3) = ned+enums.Get(j);
  dnums.Elem(j+6) = 2*ned+enums.Get(j);
  }
  for (j = 1; j <= 5; j++)
  dnums.Elem(j+9) = 3*ned+(elnr-1)*5+j;
  break;
  }
  }
  }
  }
  }

  const FiniteElement & RaviartThomasFESpace :: GetSFE (int selnr) const
  {
  switch (type)
  {
  case RT0:
  return segm0;
  case BDM1:
  case BDM1p:
  return segm1;
  case BDM2:
  case BDM2p:
  return segm2;
  }
  return segm0;
  }


  void RaviartThomasFESpace :: GetSDofNrs (int selnr, Array<int> & dnums) const
  {
  const MeshAccess & ma = GetMeshAccess();
  int eled;

  int ena[4], eoa[4];
  Array<int> enums(4, ena), eorient(4, eoa);

  ma.GetSElEdges (selnr, enums, eorient);

  switch (type)
  {
  case RT0:
  {
  dnums.SetSize(1);
  dnums.Elem(1) = enums.Elem(1);
  break;
  }
  case BDM1:
  case BDM1p:
  {
  dnums.SetSize(2);
  dnums.Elem(1) = enums.Elem(1);
  dnums.Elem(2) = ned+enums.Elem(1);
  break;
  }
  case BDM2:
  case BDM2p:
  {
  dnums.SetSize(3);
  dnums.Elem(1) = enums.Elem(1);
  dnums.Elem(2) = ned+enums.Elem(1);
  dnums.Elem(3) = 2*ned+enums.Elem(1);
  break;
  }

  //  (*testout) << "sel " << selnr << ": " << dnums.Get(1) << endl;
  }
  }


  void RaviartThomasFESpace :: 
  TransformMatrix (int elnr, DenseMatrix & mat) const
  {
  DenseMatrix trans;
  TransformationMatrix (elnr, trans);

  int i, j;
  int n = trans.Height();
  for (i = 1; i <= n; i++)
  {
  double fac = trans.Get(i, i);
  for (j = 1; j <= n; j++)
  {
  mat.Elem(i, j) *= fac;
  mat.Elem(j, i) *= fac;
  }
  }
  }

  void RaviartThomasFESpace :: 
  TransformSurfMatrix (int elnr, DenseMatrix & mat) const
  {
  DenseMatrix trans;
  SurfTransformationMatrix (elnr, trans);

  DenseMatrix hmat1(trans.Height());
  
  CalcAtB (trans, mat, hmat1);
  Mult (hmat1, trans, mat);
  }

  void RaviartThomasFESpace :: 
  TransformRHSVector (int elnr, Vector & vec) const
  {
  DenseMatrix trans;
  TransformationMatrix (elnr, trans);
  
  Vector hvec(trans.Height());
  
  trans.MultTrans (vec, hvec);
  vec.Set (1, hvec);
  }

  void RaviartThomasFESpace :: 
  TransformSurfRHSVector (int elnr, Vector & vec) const
  {
  DenseMatrix trans;
  SurfTransformationMatrix (elnr, trans);

  Vector hvec(trans.Height());

  trans.MultTrans (vec, hvec);
  vec.Set (1, hvec);
  }


  void RaviartThomasFESpace :: 
  TransformSolVector (int elnr, Vector & vec) const
  {
  DenseMatrix trans;
  TransformationMatrix (elnr, trans);

  Vector hvec(trans.Height());
  
  trans.Mult (vec, hvec);
  vec.Set (1, hvec);
  }


  void RaviartThomasFESpace :: 
  TransformSurfSolVector (int elnr, Vector & vec) const
  {
  DenseMatrix trans;
  SurfTransformationMatrix (elnr, trans);

  Vector hvec(trans.Height());
  
  trans.Mult (vec, hvec);
  vec.Set (1, hvec);
  }





  void RaviartThomasFESpace ::
  TransformationMatrix (int elnr, DenseMatrix & mat) const
  {
  const MeshAccess & ma = GetMeshAccess();
  int nd = GetFE (elnr).GetNDof();
  mat.SetSize (nd * dimension);
  mat.SetScalar (0);
      
  Array<int> enums, eorient;
  ma.GetElEdges (elnr, enums, eorient);

  mat.SetScalar (0);
  int i;

  for (i = 1; i <= mat.Height(); i++)
  mat.Elem(i,i) = 1;

  switch (ma.GetElType(elnr))
  {
  case ET_TRIG:
  switch (type)
  {
  case RT0:
  {
  for (i = 1; i <= 3; i++)
  mat.Elem(i, i) = eorient.Elem(i);
  break;
  }
  case BDM1:
  case BDM1p:
  {
  for (i = 1; i <= 3; i++)
  mat.Elem(i, i) = eorient.Elem(i);
  break;
  }
  case BDM2:
  case BDM2p:
  {
  for (i = 1; i <= 3; i++)
  {
  mat.Elem(i, i) = eorient.Elem(i);
  mat.Elem(i+6, i+6) = eorient.Elem(i);
  }
  break;
  }
  }
  }
  //  (*testout) << "el = " << elnr << ", mat = " << mat << endl;



  {
  int i, j, k, l;
  // copy matrix dimension times
  for (i = nd; i >= 1; i--)
  for (j = nd; j >= 1; j--)
  mat.Elem(dimension*i, dimension*j) = mat.Elem(i, j);
  for (i = 1; i <= nd; i++)
  for (j = 1; j <= nd; j++)
  {
  double val = mat.Elem(dimension*i, dimension*j);
  for (l = 0; l < dimension; l++)
  mat.Elem(dimension*i-l, dimension*j-l) = val;
  }
  }
  }








  void RaviartThomasFESpace ::
  SurfTransformationMatrix (int selnr, DenseMatrix & mat) const
  {
  const MeshAccess & ma = GetMeshAccess();
  int nd = GetSFE (selnr).GetNDof();
  mat.SetSize (nd * dimension);
  mat.SetScalar (0);

  int i;
  for (i = 1; i <= nd*dimension; i++)
  mat.Elem(i, i) = 1;
  }























  RaviartThomasFESpaceBoundary :: 
  RaviartThomasFESpaceBoundary (const MeshAccess & ama,
  RT_TYPE atype) 
  : FESpace (ama)
  {
  ned = 0;
  type = atype;
  }


  RaviartThomasFESpaceBoundary :: ~RaviartThomasFESpaceBoundary ()
  {
  ;
  }

  void RaviartThomasFESpaceBoundary :: Update()
  {
  cout << "Update Boundary Raviart Thomas space" << flush;
  int i, j, k;
  const MeshAccess & ma = GetMeshAccess();

  int level = ma.GetNLevels();
  ne = ma.GetNE();
  int nse = ma.GetNSE();
  int np = ma.GetNP();
  ned = ma.GetNEdges();

  cout << ", ndof = " << GetNDof() << endl;
  }




  int RaviartThomasFESpaceBoundary :: GetNDof () const
  {
  int nse = GetMeshAccess().GetNSE();
  switch (type)
  {
  case RT0:
  return ned;
  case BDM1:
  return 2*ned;
  case BDM2:
  return 3*ned+3*nse;
  case BDM2p:
  return 3*ned+5*ne;
  }
  return 0;
  }

  int RaviartThomasFESpaceBoundary :: GetNDofLevel (int level) const
  {
  return GetNDof();
  }

  const FiniteElement & RaviartThomasFESpaceBoundary :: GetFE (int elnr) const
  {
  const MeshAccess & ma = GetMeshAccess();

  switch (ma.GetElType(elnr))
  {
  case ET_TRIG:
  {
  switch (type)
  {
  case RT0:
  return rttrig0;
  case BDM1:
  return bdmtrig1;
  case BDM2p:
  return bdmtrig2plus;
  }
  break;
  }
  }
  cerr << "RaviartThomasFESpaceBoundary, GetFE: unknown type A" << endl;
  return rttrig0;
  }
  
  void RaviartThomasFESpaceBoundary :: GetDofNrs (int elnr, Array<int> & dnums) const
  {
  dnums.SetSize(0);
  return;

  const MeshAccess & ma = GetMeshAccess();
  int eled, elfa;
  int i, j;

  int ena[12], eoa[12];
  Array<int> enums(12, ena), eorient(12, eoa);

  ma.GetElEdges (elnr, enums, eorient);

  switch (ma.GetElType(elnr))
  {
  case ET_TRIG:
  {
  switch (type)
  {
  case RT0:
  {
  dnums.SetSize(3);
  for (j = 1; j <= 3; j++)
  // dnums.Elem(j) = 3*elnr-3+j;
  dnums.Elem(j) = enums.Get(j);
  break;
  }
  case BDM1:
  {
  dnums.SetSize(6);
  for (j = 1; j <= 3; j++)
  {
  dnums.Elem(j) = enums.Get(j);
  dnums.Elem(j+3) = ned+enums.Get(j);
  }
  break;
  }
  }
  break;
  }
  case ET_QUAD:
  {
  switch (type)
  {
  case RT0:
  {
  dnums.SetSize(4);
  for (j = 1; j <= 4; j++)
  // dnums.Elem(j) = 3*elnr-3+j;
  dnums.Elem(j) = enums.Get(j);
  break;
  }
  }
  break;
  }
  }
  }

  const FiniteElement & RaviartThomasFESpaceBoundary :: GetSFE (int selnr) const
  {
  const MeshAccess & ma = GetMeshAccess();

  switch (ma.GetSElType(selnr))
  {
  case ET_TRIG:
  {
  switch (type)
  {
  case RT0:
  return rttrig0;
  case BDM1:
  return bdmtrig1;
  case BDM2:
  return bdmtrig2;
  case BDM2p:
  return bdmtrig2plus;
  }
  break;
  }
  case ET_QUAD:
  {
  switch (type)
  {
  case RT0:
  return rtquad0;
  case BDM1:
  case BDM2:
  return bdmquad1;

  //	  case BDM2p:
  //	    return bdmtrig2plus;

  }
  //	return rtquad0;
  break;
  }
  }
  cerr << "RaviartThomasFESpaceBoundary, GetSFE: unknown type" << endl;
  return rttrig0;
  }


  void RaviartThomasFESpaceBoundary :: GetSDofNrs (int selnr, Array<int> & dnums) const
  {
  const MeshAccess & ma = GetMeshAccess();
  int eled, elfa;
  int i, j;

  int ena[12], eoa[12];
  Array<int> enums(12, ena), eorient(12, eoa);

  ma.GetSElEdges (selnr, enums, eorient);

  switch (ma.GetSElType(selnr))
  {
  case ET_TRIG:
  {
  switch (type)
  {
  case RT0:
  {
  dnums.SetSize(3);
  for (j = 1; j <= 3; j++)
  dnums.Elem(j) = enums.Get(j);
  break;
  }
  case BDM1:
  {
  dnums.SetSize(6);
  for (j = 1; j <= 3; j++)
  {
  dnums.Elem(j) = enums.Get(j);
  dnums.Elem(j+3) = ned+enums.Get(j);
  }
  break;
  }
  case BDM2:
  {
  dnums.SetSize(12);
  for (j = 1; j <= 3; j++)
  {
  dnums.Elem(j) = enums.Get(j);
  dnums.Elem(j+3) = ned+enums.Get(j);
  dnums.Elem(j+6) = 2*ned+enums.Get(j);
  }
  for (j = 1; j <= 3; j++)
  dnums.Elem(9+j) = 3*ned+3*(selnr-1) + j;
  break;
  }
  }
  break;
  }
  case ET_QUAD:
  {
  switch (type)
  {
  case RT0:
  {
  dnums.SetSize(4);
  for (j = 1; j <= 4; j++)
  dnums.Elem(j) = enums.Get(j);
  break;
  }
  case BDM1:
  case BDM2:
  {
  dnums.SetSize(8);
  for (j = 1; j <= 4; j++)
  {
  dnums.Elem(j) = enums.Get(j);
  dnums.Elem(j+4) = ned+enums.Get(j);
  }
  break;
  }
  }
  break;
  }
  }
  }







  IntTable * RaviartThomasFESpaceBoundary :: 
  CreateSmoothingBlocks () const
  {
  int i, j;

  const MeshAccess & ma = GetMeshAccess();
  int nv = ma.GetNV();
  int ned = ma.GetNEdges();
  int nse = ma.GetNSE();

  IntTable * it;

  Array<int> enums, eorient;
  BitArray bedge(ned);
  bedge.Clear();
  for (i = 1; i <= nse; i++)
  if (DefinedOnBoundary(ma.GetSElIndex(i)))
  {
  ma.GetSElEdges (i, enums, eorient);
  for (j = 1; j <= enums.Size(); j++)
  bedge.Set (enums.Get(j));
  }

  switch (type)
  {
  case RT0:
  {
  it = new IntTable (nv);
	
  for (i = 1; i <= ned; i++)
  //	  if (bedge.Test(i))
  {
  int vi1, vi2;
  ma.GetEdgePNums (i, vi1, vi2);
  it -> AddUnique (vi1, i);
  it -> AddUnique (vi2, i);
  }
  break;
  }
  case BDM1:
  {
  it = new IntTable (ned+nv);
	
  for (i = 1; i <= ned; i++)
  if (bedge.Test(i))
  {
  int vi1, vi2;
  ma.GetEdgePNums (i, vi1, vi2);
  it -> AddUnique (vi1, i);
  it -> AddUnique (vi2, i);
  }
	
  for (i = 1; i <= ned; i++)
  if (bedge.Test(i))
  {
  // it -> AddUnique (i, i);
  it -> AddUnique (nv+i, ned+i);
  }
  break;
  }
  case BDM2:
  {
  it = new IntTable (nv+ned);

  for (i = 1; i <= ned; i++)
  if (bedge.Test(i))
  {
  int vi1, vi2;
  ma.GetEdgePNums (i, vi1, vi2);
  it -> AddUnique (vi1, i);
  it -> AddUnique (vi2, i);
  }
	
  for (i = 1; i <= ned; i++)
  if (bedge.Test(i))
  {
  it -> AddUnique (nv+i, i);
  it -> AddUnique (nv+i, ned+i);
  it -> AddUnique (nv+i, 2*ned+i);
  }
  for (i = 1; i <= nse; i++)
  {
  ma.GetSElEdges (i, enums, eorient);
  for (j = 1; j <= enums.Size(); j++)
  if (bedge.Test(enums.Get(j)))
  {
  it -> AddUnique (nv+enums.Get(j), 3*ned+3*i-2);
  it -> AddUnique (nv+enums.Get(j), 3*ned+3*i-1);
  it -> AddUnique (nv+enums.Get(j), 3*ned+3*i  );
  }
  }
  break;
  }
  default:
  {
  cerr << "RaviartThomasFESpaceBoundary::CreateSmoothingBlocks, undefined type" << endl;
  it = NULL;
  }
  }
  return it;
  }


  void RaviartThomasFESpaceBoundary :: 
  TransformMatrix (int elnr, DenseMatrix & mat) const
  {
  DenseMatrix trans;
  TransformationMatrix (elnr, trans);

  int i, j;
  int n = trans.Height();
  for (i = 1; i <= n; i++)
  {
  double fac = trans.Get(i, i);
  for (j = 1; j <= n; j++)
  {
  mat.Elem(i, j) *= fac;
  mat.Elem(j, i) *= fac;
  }
  }
  }

  void RaviartThomasFESpaceBoundary :: 
  TransformSurfMatrix (int elnr, DenseMatrix & mat) const
  {
  DenseMatrix trans;
  SurfTransformationMatrix (elnr, trans);

  DenseMatrix hmat1(trans.Height());
  
  CalcAtB (trans, mat, hmat1);
  Mult (hmat1, trans, mat);
  }

  void RaviartThomasFESpaceBoundary :: 
  TransformSurfMatrixL (int elnr, DenseMatrix & mat) const
  {
  DenseMatrix trans;
  SurfTransformationMatrix (elnr, trans);

  DenseMatrix hmat1(trans.Height(), mat.Width());
  
  CalcAtB (trans, mat, hmat1);
  mat = hmat1;
  //  Mult (hmat1, trans, mat);
  }

  void RaviartThomasFESpaceBoundary :: 
  TransformSurfMatrixR (int elnr, DenseMatrix & mat) const
  {
  DenseMatrix trans;
  SurfTransformationMatrix (elnr, trans);

  DenseMatrix hmat1(mat.Height(), mat.Width());
  
  hmat1 = mat;
  Mult (hmat1, trans, mat);
  }





  void RaviartThomasFESpaceBoundary :: 
  TransformRHSVector (int elnr, Vector & vec) const
  {
  DenseMatrix trans;
  TransformationMatrix (elnr, trans);
  
  Vector hvec(trans.Height());
  
  trans.MultTrans (vec, hvec);
  vec.Set (1, hvec);
  }

  void RaviartThomasFESpaceBoundary :: 
  TransformSurfRHSVector (int elnr, Vector & vec) const
  {
  DenseMatrix trans;
  SurfTransformationMatrix (elnr, trans);

  Vector hvec(trans.Height());

  trans.MultTrans (vec, hvec);
  vec.Set (1, hvec);
  }


  void RaviartThomasFESpaceBoundary :: 
  TransformSolVector (int elnr, Vector & vec) const
  {
  DenseMatrix trans;
  TransformationMatrix (elnr, trans);

  Vector hvec(trans.Height());
  
  trans.Mult (vec, hvec);
  vec.Set (1, hvec);
  }


  void RaviartThomasFESpaceBoundary :: 
  TransformSurfSolVector (int elnr, Vector & vec) const
  {
  DenseMatrix trans;
  SurfTransformationMatrix (elnr, trans);

  Vector hvec(trans.Height());
  
  trans.Mult (vec, hvec);
  vec.Set (1, hvec);
  }





  void RaviartThomasFESpaceBoundary ::
  TransformationMatrix (int elnr, DenseMatrix & mat) const
  {
  const MeshAccess & ma = GetMeshAccess();
  int nd = GetFE (elnr).GetNDof();
  mat.SetSize (nd);
  mat.SetScalar (0);
      
  Array<int> enums, eorient;
  ma.GetElEdges (elnr, enums, eorient);

  mat.SetScalar (0);
  int i;

  switch (ma.GetElType(elnr))
  {
  case ET_TRIG:
  switch (type)
  {
  case RT0:
  {
  for (i = 1; i <= 3; i++)
  mat.Elem(i, i) = eorient.Elem(i);
  break;
  }
  case BDM1:
  {
  for (i = 1; i <= 3; i++)
  mat.Elem(i, i) = eorient.Elem(i);
  for (i = 4; i <= 6; i++)
  mat.Elem(i, i) = 1;
  break;
  }
  }
  }
  //  (*testout) << "el = " << elnr << ", mat = " << mat << endl;
  }








  void RaviartThomasFESpaceBoundary ::
  SurfTransformationMatrix (int selnr, DenseMatrix & mat) const
  {
  const MeshAccess & ma = GetMeshAccess();
  int nd = GetSFE (selnr).GetNDof();
  mat.SetSize (nd*dimension);
  mat.SetScalar (0);
      
  Array<int> enums, eorient;
  ma.GetSElEdges (selnr, enums, eorient);

  mat.SetScalar (0);
  int i;

  switch (ma.GetSElType(selnr))
  {
  case ET_TRIG:
  {
  switch (type)
  {
  case RT0:
  {
  for (i = 1; i <= 3; i++)
  mat.Elem(i, i) = eorient.Elem(i);
  break;
  }
  case BDM1:
  {
  for (i = 1; i <= 3; i++)
  mat.Elem(i, i) = eorient.Elem(i);
  for (i = 4; i <= 6; i++)
  mat.Elem(i, i) = 1;
  break;
  }
  case BDM2:
  {
  for (i = 1; i <= nd; i++)
  mat.Elem(i, i) = 1;
  for (i = 1; i <= 3; i++)
  {
  mat.Elem(i, i) = eorient.Elem(i);
  mat.Elem(6+i, 6+i) = eorient.Elem(i);
  }
  break;
  }
  }
  break;
  }
  case ET_QUAD:
  {
  switch (type)
  {
  case RT0:
  {
  for (i = 1; i <= 4; i++)
  mat.Elem(i, i) = eorient.Elem(i);
  break;
  }
  case BDM1:
  case BDM2:
  {
  for (i = 1; i <= 4; i++)
  mat.Elem(i, i) = eorient.Elem(i);
  for (i = 5; i <= 8; i++)
  mat.Elem(i, i) = 1;
  break;
  }
  }
  break;
  }
  }

  
  {
  int i, j, k, l;
  // copy matrix dimension times
  for (i = nd; i >= 1; i--)
  for (j = nd; j >= 1; j--)
  mat.Elem(dimension*i, dimension*j) = mat.Elem(i, j);
  for (i = 1; i <= nd; i++)
  for (j = 1; j <= nd; j++)
  {
  double val = mat.Elem(dimension*i, dimension*j);
  for (l = 0; l < dimension; l++)
  mat.Elem(dimension*i-l, dimension*j-l) = val;
  }
  }



  //   const MeshAccess & ma = GetMeshAccess();
  //   int nd = GetSFE (selnr).GetNDof();
  //   mat.SetSize (nd * dimension);
  //   mat.SetScalar (0);

  //   int i;
  //   for (i = 1; i <= nd; i++)
  //     mat.Elem(i, i) = 1;

  }

  */



  // register FESpaces
  namespace hcurlhdives_cpp
  {
    class Init
    { 
    public: 
      Init ();
    };
    
    Init::Init()
    {
      GetFESpaceClasses().AddFESpace ("hcurl", NedelecFESpace::Create);
    }

    
    Init init;
  }



}
