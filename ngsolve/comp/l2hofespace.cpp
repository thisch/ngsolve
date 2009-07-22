/*********************************************************************/
/* File:   l2hofespace.cpp                                         */
/* Author: Start                                                     */
/* Date:   24. Feb. 2003                                             */
/*********************************************************************/

/**
   High Order Finite Element Space for L2
*/

/* ***********************************************
To do: *Internal External Dofs (eliminate internal) 
       *Flag for low-order dofs eliminated ...   
************************* */ 
#include <comp.hpp>
#include <multigrid.hpp>

#include <parallelngs.hpp>


#ifdef PARALLEL
extern MPI_Group MPI_HIGHORDER_WORLD;
extern MPI_Comm MPI_HIGHORDER_COMM;
#endif

using namespace ngmg;

namespace ngcomp
{
  using namespace ngcomp;
  using namespace ngparallel;

  L2HighOrderFESpace ::  
  L2HighOrderFESpace (const MeshAccess & ama, const Flags & flags, bool parseflags)
    : FESpace (ama, flags)
  {
    name="L2HighOrderFESpace(l2ho)";
    
    // defined flags
    DefineNumFlag("relorder");
    DefineDefineFlag("print"); 
    DefineDefineFlag("l2ho");
    
    if (parseflags) CheckFlags(flags);
    
    var_order = 0; 
    
    print = flags.GetDefineFlag("print"); 
    if (flags.NumFlagDefined("order"))
      order =  int (flags.GetNumFlag("order",0));
    else 
      {
	if(flags.NumFlagDefined("relorder"))
	  {
	    order=0; 
	    var_order = 1; 
	    rel_order = int (flags.GetNumFlag("relorder",0));
	  }
	else 
	  order = 0; 
      }
    
    if(flags.GetDefineFlag("variableorder") )
      {
	throw Exception ("Flag 'variableorder' for l2ho is obsolete. \n  Either choose uniform order by -order= .. \n -relorder=.. for relative mesh order "); 
      }
    
    if (ma.GetDimension() == 2)
      evaluator = new MassIntegrator<2> (new ConstantCoefficientFunction(1));
    else
      evaluator = new MassIntegrator<3> (new ConstantCoefficientFunction(1));

    if (dimension > 1)
      evaluator = new BlockBilinearFormIntegrator (*evaluator, dimension);

    fast_pfem = flags.GetDefineFlag ("fast");

    Flags loflags;
    loflags.SetFlag ("order", 0.0);
    loflags.SetFlag ("dim", dimension);
    if (iscomplex) loflags.SetFlag ("complex");

#ifndef PARALLEL
    low_order_space = new ElementFESpace (ma, loflags);
#else
    low_order_space = new ParallelElementFESpace ( ma, loflags);
#endif
    
    prol = new ElementProlongation (*static_cast<ElementFESpace*> (low_order_space));
  }


  L2HighOrderFESpace :: ~L2HighOrderFESpace ()
  {
    ;
  }

  FESpace * L2HighOrderFESpace :: 
  Create (const MeshAccess & ma, const Flags & flags)
  {
    int order = int(flags.GetNumFlag ("order", 0));
    if (order == 0)
      return new ElementFESpace (ma, flags);
    else
      return new L2HighOrderFESpace (ma, flags, true);
  }  

  
  void L2HighOrderFESpace :: Update(LocalHeap & lh)
  {
    if(low_order_space) low_order_space -> Update(lh);
                  
    nel = ma.GetNE();
    order_inner.SetSize(nel); 
 
 
    int p  = (var_order ? 0 : order); 
    order_inner = INT<3>(p,p,p); 
    
    int dim = ma.GetDimension(); 

    if(var_order) 
      for(int i=0;i<nel;i++) 
	{
	  INT<3> el_orders = ma.GetElOrders(i);
	  	  
	  for(int j=0;j<dim;j++)
	    order_inner[i][j] =  int(max2(el_orders[j]+rel_order,0)); 
	} 

    if(print) 
      *testout << " order_inner (l2ho) " << order_inner << endl; 

    ndof = nel;
    
    UpdateDofTables();

    while (ma.GetNLevels() > ndlevel.Size())
      ndlevel.Append (ndof);
    ndlevel.Last() = ndof;

    if(low_order_space) prol->Update();

#ifdef PARALLEL
    UpdateParallelDofs();
#endif
  } 

  void L2HighOrderFESpace :: UpdateDofTables()
  {
    first_element_dof.SetSize(nel+1);
    for (int i = 0; i < nel; i++)
      {
	first_element_dof[i] = ndof;
	INT<3> pi = order_inner[i]; 
	switch (ma.GetElType(i))
	  {
	  case ET_TRIG:
	    ndof += (pi[0]+1)*(pi[0]+2)/2 ;
	    break;
	  case ET_QUAD:
	    ndof += (pi[0]+1)*(pi[1]+1);
	    break;
	  case ET_TET:
	    ndof += (pi[0]+1)*(pi[0]+2)*(pi[0]+3)/6;
	    break;
	  case ET_PRISM:
	    ndof += (pi[0]+1)*(pi[0]+2)*(pi[2]+1)/2;
	    break;
	  case ET_PYRAMID:
	    ndof += 5 + 8*(pi[0]-1) + 2*(pi[0]-1)*(pi[0]-2) + (pi[0]-1)*(pi[0]-1) 
	      + (pi[0]-1)*(pi[0]-2)*(2*pi[0]-3)/6;
	    break;
	  case ET_HEX:
	    ndof += (pi[0]+1)*(pi[1]+1)*(pi[2]+1);
	    break;
          default:  // for the compiler
            break;
	  }
	ndof--; // subtract constant 
      }
    first_element_dof[nel] = ndof;


    if(print) 
      *testout << " first_element dof (l2hofe) " << first_element_dof << endl;  

    while (ma.GetNLevels() > ndlevel.Size())
      ndlevel.Append (ndof);
    ndlevel.Last() = ndof;

    prol->Update();
  }


  const FiniteElement & L2HighOrderFESpace :: GetFE (int elnr, LocalHeap & lh) const
  {
    try
      { 
	FiniteElement * fe;
	typedef IntegratedLegendreMonomialExt T_ORTHOPOL;

	switch (ma.GetElType(elnr))
	  {
	  case ET_TET:
	    { 
	      fe = new (lh.Alloc (sizeof(L2HighOrderFE<ET_TET>))) L2HighOrderFE<ET_TET> (order);
	      break;
	    }
	  case ET_PYRAMID:
	    {
	      fe = new (lh.Alloc (sizeof(H1HighOrderFE<ET_PYRAMID>))) H1HighOrderFE<ET_PYRAMID> (order);
	      break;
	    }
	  case ET_PRISM:
	    {
	      fe = new (lh.Alloc (sizeof(L2HighOrderFE<ET_PRISM>))) L2HighOrderFE<ET_PRISM> (order);
              break;
	    }
	  case ET_HEX:
	    {
	      fe = new (lh.Alloc (sizeof(L2HighOrderFE<ET_HEX>))) L2HighOrderFE<ET_HEX> (order);
              break;
	    }
	  case ET_TRIG:
	    { 
	      fe = new (lh.Alloc (sizeof(L2HighOrderFE<ET_TRIG>))) L2HighOrderFE<ET_TRIG> (order);
	      break;
	    }
	  case ET_QUAD:
	    {
	      fe = new (lh.Alloc (sizeof(L2HighOrderFE<ET_QUAD>))) L2HighOrderFE<ET_QUAD> (order);
	      break;
	    }
	  default:
	    fe = 0; 
	  }
	
	if (!fe)
	  {
	    stringstream str;
	    str << "L2HighOrderFESpace " << GetClassName() 
		<< ", undefined eltype " 
		<< ElementTopology::GetElementName(ma.GetElType(elnr))
		<< ", order = " << order << endl;
	    throw Exception (str.str());
	  }

	ArrayMem<int,12> vnums; // calls GetElPNums -> max 12 for PRISM12
	ma.GetElVertices(elnr, vnums);

        if (dynamic_cast<L2HighOrderFiniteElement<2>* > (fe))
          {
            L2HighOrderFiniteElement<2> * hofe = dynamic_cast<L2HighOrderFiniteElement<2>* > (fe);
 	    hofe-> SetVertexNumbers (vnums); 
	    INT<2> p(order_inner[elnr][0], order_inner[elnr][1]);
	    hofe-> SetOrder(p);
	    hofe-> ComputeNDof(); 
          }
        else
          {
            L2HighOrderFiniteElement<3> * hofe = dynamic_cast<L2HighOrderFiniteElement<3>* > (fe);
 	    hofe-> SetVertexNumbers (vnums); 
	    hofe-> SetOrder(order_inner[elnr]); 
	    hofe-> ComputeNDof(); 
            return *hofe;
          }

	return *fe;
      } 
    catch (Exception & e)
      {
	e.Append ("in L2HoFESpace::GetElement");
	e.Append ("\n");
	throw; 
      }
  }
 
  const FiniteElement & L2HighOrderFESpace :: GetSFE (int elnr, LocalHeap & lh) const
  {
    FiniteElement * fe = 0;
    
    switch (ma.GetSElType(elnr))
      {
      case ET_SEGM:
	fe = new FE_SegmDummy; break;
      case ET_TRIG:
	fe = new FE_SegmDummy ; break;
      case ET_QUAD:
	fe = new FE_SegmDummy; break;
      default:
	fe = 0;
      }
    
    if (!fe)
      {
	stringstream str;
	str << "FESpace " << GetClassName() 
	    << ", undefined surface eltype " << ma.GetSElType(elnr) 
	    << ", order = " << order << endl;
	throw Exception (str.str());
      }
    return *fe;
  }
 

  int L2HighOrderFESpace :: GetNDof () const
  {
    return ndof;
  }

  int L2HighOrderFESpace :: GetNDofLevel (int level) const
  {
    return ndlevel[level];
  }


  void L2HighOrderFESpace :: GetDofNrs (int elnr, Array<int> & dnums) const
  {
    dnums.SetSize(0);
    dnums.Append (elnr); // lowest_order 
    int first = first_element_dof[elnr];
    int neldofs = first_element_dof[elnr+1] - first;

    for (int j = 0; j < neldofs; j++)
      dnums.Append (first+j);
    
    if (!DefinedOn (ma.GetElIndex (elnr)))
      dnums = -1;
  }
  
  void  L2HighOrderFESpace :: GetExternalDofNrs (int elnr, Array<int> & dnums) const
  {
    if (!eliminate_internal) 
      {
	GetDofNrs (elnr, dnums);
	return;
      }
    dnums.SetSize(0);
  }


  void L2HighOrderFESpace :: 
  GetSDofNrs (int elnr, Array<int> & dnums) const
  {
    dnums.SetSize (0);
  }
  
  Table<int> * L2HighOrderFESpace :: 
  CreateSmoothingBlocks (const Flags & precflags) const
  {
    int i, j, first;
    Array<int> cnt(nel);
    cnt = 0;
    for (i = 0; i < nel; i++)
      cnt[i] = first_element_dof[i+1]-first_element_dof[i];
	
    Table<int> & table = *new Table<int> (cnt);
    
    for (i = 0; i < nel; i++)
      {
	first = first_element_dof[i];
	for (j = 0; j < cnt[i]; j++)
	  table[i][j] = first+j;
      }
    return &table;
  }

  void L2HighOrderFESpace :: GetWireBasketDofNrs (int elnr, Array<int> & dnums) const
  {
    dnums.SetSize(0);
    dnums.Append (elnr);
  }

  void  L2HighOrderFESpace :: GetVertexDofNrs (int vnr, Array<int> & dnums) const
  { 
    dnums.SetSize(0);
  }
  
  void  L2HighOrderFESpace ::GetEdgeDofNrs (int ednr, Array<int> & dnums) const
  { 
    dnums.SetSize(0); 
  }
  
  void  L2HighOrderFESpace ::GetFaceDofNrs (int fanr, Array<int> & dnums) const
  { 
    dnums.SetSize(0); 
  }
  
  void  L2HighOrderFESpace ::GetInnerDofNrs (int elnr, Array<int> & dnums) const
  { 
    GetDofNrs ( elnr, dnums ); 
  }










  L2SurfaceHighOrderFESpace ::  
  L2SurfaceHighOrderFESpace (const MeshAccess & ama, const Flags & flags, bool parseflags)
    : FESpace (ama, flags)
  {
    name="L2SurfaceHighOrderFESpace(l2surf)";
    // defined flags 
    DefineDefineFlag("l2surf");

    if(parseflags) CheckFlags(flags);
    
    
    if(flags.NumFlagDefined("relorder"))
      throw Exception("Variable order not implemented for L2SurfaceHighOrderFESpace"); 
    
    segm = new H1HighOrderFE<ET_SEGM> (order);
    trig = new H1HighOrderFE<ET_TRIG> (order);
    quad = new H1HighOrderFE<ET_QUAD> (order);

    if (ma.GetDimension() == 2)
      {
	boundary_evaluator = 
	  new RobinIntegrator<2> (new ConstantCoefficientFunction(1));
      }
    else
      {
	boundary_evaluator = 
	  new RobinIntegrator<3> (new ConstantCoefficientFunction(1));
      }

    if (dimension > 1)
      {
	boundary_evaluator = 
	  new BlockBilinearFormIntegrator (*boundary_evaluator, dimension);
      }
  }

  L2SurfaceHighOrderFESpace :: ~L2SurfaceHighOrderFESpace ()
  {
    ;
  }

  FESpace * L2SurfaceHighOrderFESpace :: 
  Create (const MeshAccess & ma, const Flags & flags)
  {
    return new L2SurfaceHighOrderFESpace (ma, flags, true);
  }

  void L2SurfaceHighOrderFESpace :: Update(LocalHeap & lh)
  {
    nel = ma.GetNSE();

    ndof = 0;
    first_element_dof.SetSize(nel+1);
    for (int i = 0; i < nel; i++)
      {
	first_element_dof[i] = ndof;
	switch (ma.GetSElType(i))
	  {
	  case ET_SEGM:
	    ndof += order+1;
	    break;
	  case ET_TRIG:
	    ndof += (order+1)*(order+2)/2;
	    break;
	  case ET_QUAD:
	    ndof += (order+1)*(order+1);
	    break;
	  default:
	    ;
	  }
      }
    first_element_dof[nel] = ndof;
  }

  const FiniteElement & L2SurfaceHighOrderFESpace :: GetSFE (int elnr, LocalHeap & lh) const
  {
    FiniteElement * fe = 0;
    
    switch (ma.GetSElType(elnr))
      {
      case ET_SEGM:
	fe = segm; break;
      case ET_TRIG:
	fe = trig; break;
      case ET_QUAD:
	fe = quad; break;
      default:
	fe = 0;
      }
    
    ArrayMem<int,12> vnums; // calls GetElPNums -> max 12 for PRISM12
    ma.GetSElVertices(elnr, vnums);

    if (!fe)
      {
	stringstream str;
	str << "L2SurfaceHighOrderFESpace " << GetClassName() 
	    << ", undefined eltype " 
	    << ElementTopology::GetElementName(ma.GetSElType(elnr))
	    << ", order = " << order << endl;
	throw Exception (str.str());
      }

    dynamic_cast<H1HighOrderFiniteElement<2>*> (fe) -> SetVertexNumbers (vnums);

    return *fe;
  }
 
  const FiniteElement & L2SurfaceHighOrderFESpace :: GetFE (int elnr, LocalHeap & lh) const
  {
    throw Exception ("Volume elements not available for L2SurfaceHighOrderFESpace");
  }
 
  int L2SurfaceHighOrderFESpace :: GetNDof () const
  {
    return ndof;
  }

  void L2SurfaceHighOrderFESpace :: GetSDofNrs (int elnr, Array<int> & dnums) const
  {
    dnums.SetSize(0);
    int first = first_element_dof[elnr];
    int neldofs = first_element_dof[elnr+1] - first;
    for (int j = 0; j < neldofs; j++)
      dnums.Append (first+j);

    if (!DefinedOn (ma.GetSElIndex (elnr)))
      dnums = -1;
  }
  
  void L2SurfaceHighOrderFESpace :: 
  GetDofNrs (int elnr, Array<int> & dnums) const
  {
    dnums.SetSize (0);
  }
  
  Table<int> * L2SurfaceHighOrderFESpace :: 
  CreateSmoothingBlocks ( int type) const
  {
    int i, j, first;
    Array<int> cnt(nel);
    cnt = 0;
    for (i = 0; i < nel; i++)
      cnt[i] = first_element_dof[i+1]-first_element_dof[i];
	
    Table<int> & table = *new Table<int> (cnt);
    
    for (i = 0; i < nel; i++)
      {
	first = first_element_dof[i];
	for (j = 0; j < cnt[i]; j++)
	  table[i][j] = first+j;
      }
    return &table;
  }


  void  L2SurfaceHighOrderFESpace :: GetVertexDofNrs (int vnr, Array<int> & dnums) const
  { dnums.SetSize(0); return; }
  
  void  L2SurfaceHighOrderFESpace ::GetEdgeDofNrs (int ednr, Array<int> & dnums) const
  { dnums.SetSize(0); return; }
  
  void  L2SurfaceHighOrderFESpace ::GetFaceDofNrs (int fanr, Array<int> & dnums) const
  { GetDofNrs ( fanr, dnums ); return; }
  
  void  L2SurfaceHighOrderFESpace ::GetInnerDofNrs (int elnr, Array<int> & dnums) const
  { GetDofNrs ( elnr, dnums ); return; }
    

#ifdef PARALLEL

void L2HighOrderFESpace :: UpdateParallelDofs_hoproc()
  {
    
     // ******************************
     // update exchange dofs 
     // ******************************
    *testout << "L2HOFESpace::UpdateParallelDofs_hoproc" << endl;
    // Find number of exchange dofs
    Array<int> nexdof(ntasks);
    nexdof = 0;

    const MeshAccess & ma = (*this). GetMeshAccess();

    MPI_Request * sendrequest = new MPI_Request[ntasks];
    MPI_Request * recvrequest = new MPI_Request[ntasks];
    MPI_Status status;

    // + number of inner exchange dofs
    for ( int el = 0; el < ma.GetNE(); el++ )
      {
	if ( !parallelma->IsExchangeElement ( el ) ) continue;
	
	Array<int> dnums;
	GetInnerDofNrs ( el, dnums );
	nexdof[id] += dnums.Size() ; 

	for ( int dest = 1; dest < ntasks; dest ++ )
	  if (  parallelma -> GetDistantElNum ( dest, el ) >= 0 )
	    nexdof[dest] += dnums.Size() ; 
      }

    nexdof[0] = LowOrderFESpace() . GetNDof();

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
	(*owndofs[i])[0] = ndof;
	distantdofs[i] = new Array<int>(0);
      }



    Array<int> cnt_nexdof(ntasks);
    cnt_nexdof = 0;
    int exdof = 0;


    // *****************
    // Parallel Element dofs
    // *****************


    for ( int el = 0; el < ma.GetNE(); el++ )
      if ( parallelma->IsExchangeElement ( el ) )
	{
	  Array<int> dnums;
	  GetDofNrs ( el, dnums );
	  if ( dnums.Size() == 0 ) continue;

	  for ( int i=0; i<dnums.Size(); i++ )
	    (*(paralleldofs->sorted_exchangedof))[id][exdof++] = dnums[i];

	  for ( int dest = 1; dest < ntasks; dest++ )
	    {
	      int distel = parallelma -> GetDistantElNum ( dest, el );
	      if( distel < 0 ) continue;

	      owndofs[dest]->Append ( distel );
	      owndofs[dest]->Append (int(paralleldofs->IsGhostDof(dnums[0])) );
	      for ( int i=0; i<dnums.Size(); i++)
		{
		  paralleldofs->SetExchangeDof ( dest, dnums[i] );
		  paralleldofs->SetExchangeDof ( dnums[i] );
		  owndofs[dest]->Append ( dnums[i] );
		}
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

    int ii = 1;
    for( int dest=1; dest<ntasks; dest++) 
      {
	if ( dest == id ) continue;
	MPI_Wait ( recvrequest + dest, &status );
	paralleldofs -> SetDistNDof( dest, (*distantdofs[dest])[0]) ;
	// low order nedelec dofs first
	ii = 1;
	while ( ii < distantdofs[dest]->Size() )
	  {
	    int elnum = (*distantdofs[dest])[ii++];
	    int isdistghost = (*distantdofs[dest])[ii++];
	    Array<int> dnums;
	    GetDofNrs (elnum, dnums);
// 	    (*(paralleldofs->localexchangedof))[dest][ cnt_nexdof[dest] ] = dnums[0];
// 	    (*(paralleldofs->distantexchangedof))[dest][ cnt_nexdof[dest] ] = (*distantdofs[dest])[ii];
	    (*(paralleldofs->sorted_exchangedof))[dest][ cnt_nexdof[dest] ] = dnums[0];
	    if ( dest < id && !isdistghost )
	      paralleldofs->ismasterdof.Clear ( dnums[0] ) ;
	    ii += dnums.Size();
	    cnt_nexdof[dest]++;
	  }
	// then the high order dofs, without nedelecs
	ii = 1;
	while ( ii < distantdofs[dest]->Size() )
	  {
	    int elnum = (*distantdofs[dest])[ii++];
	    int isdistghost = (*distantdofs[dest])[ii++];
	    Array<int> dnums;
	    GetDofNrs (elnum, dnums);
	    ii++; 
	    for ( int i=1; i<dnums.Size(); i++)
	      {
// 		(*(paralleldofs->localexchangedof))[dest][ cnt_nexdof[dest] ] = dnums[i];
// 		(*(paralleldofs->distantexchangedof))[dest][ cnt_nexdof[dest] ] = (*distantdofs[dest])[ii];
		(*(paralleldofs->sorted_exchangedof))[dest][ cnt_nexdof[dest] ] = dnums[i];
		if ( dest < id && !isdistghost )
		  paralleldofs->ismasterdof.Clear ( dnums[i] ) ;
		ii++; cnt_nexdof[dest]++;
	      }
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

    int ndof_lo = low_order_space->GetNDof();

    // all dofs are exchange dofs
    nexdof = ndof_lo;
 
    exdof = 0;
    cnt_nexdof = 0;


    // *****************
    // Parallel Element dofs
    // *****************

    owndofs[0]->SetSize(1);
    (*owndofs[0])[0] = ndof;
    distantdofs[0]->SetSize(0);
    
    // find local and distant dof numbers for vertex exchange dofs
    for ( int el = 0; el < ma.GetNE(); el++ )
      {
	int dest = 0;
	int distel = parallelma -> GetDistantElNum ( dest, el );
	owndofs[0]->Append ( distel );
	paralleldofs->SetExchangeDof ( dest, el );
	owndofs[0]->Append ( el );
      }   
    
    int dest = 0;
    MyMPI_ISend ( *owndofs[0], dest, sendrequest[dest]);
    MyMPI_IRecv ( *distantdofs[0], dest, recvrequest[dest]);
    MPI_Wait ( recvrequest + dest, &status);

    paralleldofs -> SetDistNDof( dest, (*distantdofs[dest])[0]) ;
    ii = 1;
    while ( ii < distantdofs[0]->Size() )
      {
	int elnum = (*distantdofs[0])[ii++];
// 	(*(paralleldofs->localexchangedof))[dest][ cnt_nexdof[dest] ] = elnum;
// 	(*(paralleldofs->distantexchangedof))[dest][ cnt_nexdof[dest] ] = (*distantdofs[0])[ii];
	(*(paralleldofs->sorted_exchangedof))[dest][ cnt_nexdof[dest] ] = elnum;
	ii++; cnt_nexdof[dest]++;
      }

    for ( int dest = id+1; dest < ntasks; dest++ )
      BubbleSort ( (*(paralleldofs->sorted_exchangedof))[dest] );

    for ( int i = 0; i < 1; i++ )
      {
	delete distantdofs[i];
	delete  owndofs[i];
      }


    delete [] owndofs;
    delete [] distantdofs;
    delete [] sendrequest;
    delete [] recvrequest;


  }


  void L2HighOrderFESpace :: UpdateParallelDofs_loproc()
  {
    *testout << "L2HOFESpace::UpdateParallelDofs_loproc" << endl;

    const MeshAccess & ma = (*this). GetMeshAccess();

    int ndof = GetNDof();

    // Find number of exchange dofs
    Array<int> nexdof(ntasks); 
    nexdof = 0;

    paralleldofs->SetNExDof(nexdof);

//     paralleldofs->localexchangedof = new Table<int> (nexdof);
//     paralleldofs->distantexchangedof = new Table<int> (nexdof);
    paralleldofs->sorted_exchangedof = new Table<int> (nexdof);

 
  }
#endif // PARALLEL



  // register FESpaces
  namespace l2hofespace_cpp
  {
  
    class Init
    { 
    public: 
      Init ();
    };
    
    Init::Init()
    {
      GetFESpaceClasses().AddFESpace ("l2", L2HighOrderFESpace::Create);
      GetFESpaceClasses().AddFESpace ("l2ho", L2HighOrderFESpace::CreateHO);
      GetFESpaceClasses().AddFESpace ("l2surf", L2SurfaceHighOrderFESpace::Create);
    }
    
    Init init;
  }
}
