#include <comp.hpp>
#include <multigrid.hpp>

#include <parallelngs.hpp>
#include <stdlib.h>

#include <boost/math/special_functions/round.hpp>

namespace ngcomp
{
  using namespace ngmg;


  GridFunction :: GridFunction (const FESpace & afespace, const string & name,
				const Flags & flags)
    : NGS_Object (afespace.GetMeshAccess(), name), fespace(afespace)
  { 
    nested = flags.GetDefineFlag ("nested");
    visual = !flags.GetDefineFlag ("novisual");
    multidim = int (flags.GetNumFlag ("multidim", 1));
    level_updated = -1;
    cacheblocksize = 1;
  }

  GridFunction :: ~GridFunction()
  {
    for (int i = 0; i < vec.Size(); i++)
      delete vec[i];
  }


  void GridFunction :: Update ()
  {
    throw Exception("GridFunction::Update not overloaded");
  }

  void GridFunction :: DoArchive (Archive & archive)
  {
    archive & nested & visual & multidim & level_updated;
    archive & cacheblocksize;

    if (!archive.Output()) Update();
    
    for (int i = 0; i < vec.Size(); i++)
      {
        FlatVector<double> fv = vec[i] -> FVDouble();
        for (int i = 0; i < fv.Size(); i++)
          archive & fv(i);
      }
  }


  bool GridFunction :: IsUpdated () const
  {
    int ndof = GetFESpace().GetNDof();
    for (int i = 0; i < multidim; i++)
      {
	if (!vec[i]) return false;
	if (ndof != vec[i]->Size()) return false;
      }
    return true;
  }

  void GridFunction :: PrintReport (ostream & ost)
  {
    ost << "on space " << GetFESpace().GetName() << endl
	<< "nested = " << nested << endl;
  }

  void GridFunction :: MemoryUsage (Array<MemoryUsageStruct*> & mu) const
  {
    //if (&const_cast<GridFunction&> (*this).GetVector())
    if (&(*this).GetVector())
      {
	int olds = mu.Size();
	//const_cast<GridFunction&> (*this).GetVector().MemoryUsage (mu);
	(*this).GetVector().MemoryUsage (mu);
	for (int i = olds; i < mu.Size(); i++)
	  mu[i]->AddName (string(" gf ")+GetName());
      }
  }



  void GridFunction :: AddMultiDimComponent (BaseVector & v)
  {
    vec.SetSize (vec.Size()+1);
    vec[multidim] = v.CreateVector();
    *vec[multidim] = v;
    multidim++;
  }


  void GridFunction :: Visualize(const string & given_name)
  {
    if (!visual) return;

    /*
      for (int i = 0; i < compgfs.Size(); i++)
      {
      stringstream child_name;
      child_name << given_name << "_" << i+1;
      compgfs[i] -> Visualize (child_name.str());
      }
    */

    const BilinearFormIntegrator * bfi2d = 0, * bfi3d = 0;

    if (ma.GetDimension() == 2)
      {
	bfi2d = fespace.GetIntegrator();
      }
    else
      {
	bfi3d = fespace.GetIntegrator();
	bfi2d = fespace.GetBoundaryIntegrator();
      }

    if (bfi2d || bfi3d)
      {
        netgen::SolutionData * vis;
	if (!fespace.IsComplex())
	  vis = new VisualizeGridFunction<double> (ma, this, bfi2d, bfi3d, 0);
	else
	  vis = new VisualizeGridFunction<Complex> (ma, this, bfi2d, bfi3d, 0);

	Ng_SolutionData soldata;
	Ng_InitSolutionData (&soldata);
	
	soldata.name = given_name.c_str();
	soldata.data = 0;
	soldata.components = vis->GetComponents();
	soldata.iscomplex = vis->IsComplex();
	soldata.draw_surface = bfi2d != 0;
	soldata.draw_volume  = bfi3d != 0;
	soldata.dist = soldata.components;
	soldata.soltype = NG_SOLUTION_VIRTUAL_FUNCTION;
	soldata.solclass = vis;
	Ng_SetSolutionData (&soldata);    
      }
  }



  GridFunction * GridFunction :: 
  GetComponent (int compound_comp) const
  {
    if (!compgfs.Size() || compgfs.Size() < compound_comp)
      throw Exception("GetComponent: compound_comp does not exist!");
    else
      return compgfs[compound_comp];
  }






  template <int N>
  bool MyLess (const Vec<N,int>& a, const Vec<N,int>& b)
  {
    for( int i = 0; i < N; i++)
      {
	if (a[i] < b[i]) return true;
	if (a[i] > b[i]) return false;
      }
    
    return false;  
  }






  
  template <class SCAL>
  void S_GridFunction<SCAL> :: Load (istream & ist)
  {
    if (MyMPI_GetNTasks() == 1)
      { 
	const FESpace & fes = GetFESpace();
	Array<int> dnums;
	
	for (NODE_TYPE nt = NT_VERTEX; nt <= NT_CELL; nt++)
	  {
	    int nnodes = ma.GetNNodes (nt);

	    
	    Array<Vec<8, int> > nodekeys;
	    Array<int> pnums, compress;
	    for(int i = 0; i < nnodes; i++)
	      {
		fes.GetNodeDofNrs (nt, i,  dnums);
		if (dnums.Size() == 0) continue;
		
		switch (nt)
		  {
		  case NT_VERTEX: pnums.SetSize(1); pnums[0] = i; break;
		  case NT_EDGE: ma.GetEdgePNums (i, pnums); break;
		  case NT_FACE: ma.GetFacePNums (i, pnums); break;
		  case NT_CELL: ma.GetElPNums (i, pnums); break;
		  }
		Vec<8> key; 
		key = -1;
		for (int j = 0; j < pnums.Size(); j++)
		  key[j] = pnums[j];
		nodekeys.Append (key);
		compress.Append (i);
	      }
	    
	    nnodes = nodekeys.Size();

	    Array<int> index(nnodes);
	    for( int i = 0; i < index.Size(); i++) index[i] = i;
	    
	    QuickSortI (nodekeys, index, MyLess<8>);
	    
	    for( int i = 0; i < nnodes; i++)
	      {
		fes.GetNodeDofNrs (nt, compress[index[i]],  dnums); 
		Vector<SCAL> elvec(dnums.Size());
		
		for (int k = 0; k < elvec.Size(); k++)
		  if (ist.good())
		    LoadBin<SCAL>(ist, elvec(k));			
	  
		SetElementVector (dnums, elvec);
	      }
	  }
      }
#ifdef PARALLEL	    
    else
      {  
	GetVector() = 0.0;
	GetVector().SetParallelStatus (DISTRIBUTED);    
	LoadNodeType<1,NT_VERTEX> (ist);
	LoadNodeType<2,NT_EDGE> (ist);
	LoadNodeType<4,NT_FACE> (ist);
	LoadNodeType<8,NT_CELL> (ist);
	GetVector().Cumulate();
      }
#endif
  }


  template <class SCAL>  template <int N, NODE_TYPE NTYPE>
  void  S_GridFunction<SCAL> :: LoadNodeType (istream & ist) 
  {
#ifdef PARALLEL
    int id = MyMPI_GetId();
    int ntasks = MyMPI_GetNTasks();
    
    const FESpace & fes = GetFESpace();
    ParallelDofs & par = fes.GetParallelDofs ();
    
    if(id > 0)
      {
	int nnodes = ma.GetNNodes (NTYPE);
	
	Array<Vec<N+1, int> > nodekeys;
	Array<int> master_nodes;
	Array<int> dnums, pnums;
	
	for(int i = 0; i < nnodes; i++)
	  {
	    fes.GetNodeDofNrs (NTYPE, i,  dnums);
	    
	    if (dnums.Size() == 0) continue;
	    if (!par.IsMasterDof (dnums[0])) continue;
	    
	    master_nodes.Append(i);

	    switch (NTYPE)
	      {
	      case NT_VERTEX: pnums.SetSize(1); pnums[0] = i; break;
	      case NT_EDGE: ma.GetEdgePNums (i, pnums); break;
	      case NT_FACE: ma.GetFacePNums (i, pnums); break;
	      case NT_CELL: ma.GetElPNums (i, pnums); break;
	      }

	    Vec<N+1, int> key;
	    key = -1;
	    for (int j = 0; j < pnums.Size(); j++)
	      key[j] = ma.GetGlobalNodeNum (Node(NT_VERTEX, pnums[j]));
	    key[N] = dnums.Size();
	    
	    nodekeys.Append (key);	
	  }
	
	MyMPI_Send (nodekeys, 0, 12);
	
	Array<SCAL> loc_data;
	MyMPI_Recv (loc_data, 0, 13);
	
	for (int i = 0, cnt = 0; i < master_nodes.Size(); i++)
	  {
	    fes.GetNodeDofNrs (NTYPE, master_nodes[i], dnums); 
	    Vector<SCAL> elvec(dnums.Size());
	    
	    for (int j = 0; j < elvec.Size(); j++)
	      elvec(j) = loc_data[cnt++];
	    
	    SetElementVector (dnums, elvec);
	  }
      }
    else
      {
	Array<Vec<N, int> > nodekeys;
	Array<int> nrdofs_per_node;
	
	Array<Array<int>* > nodenums_of_procs (ntasks-1);
	
	int actual=0;
	for( int proc = 1; proc < ntasks; proc++)
	  {
	    Array<Vec<N+1, int> > nodenums_proc;
	    MyMPI_Recv(nodenums_proc,proc,12);
	    nodenums_of_procs[proc-1] = new Array<int> (nodenums_proc.Size());
	    
	    for (int j=0; j < nodenums_proc.Size(); j++)
	      {
		Vec<N, int>  key;
		for (int k = 0; k < N; k++)
		  key[k] = nodenums_proc[j][k];
		
		nodekeys.Append (key);
		
		nrdofs_per_node.Append (nodenums_proc[j][N]);
		
		(*nodenums_of_procs[proc-1])[j]=actual++;
	      }
	  }
	
	int nnodes = nodekeys.Size();

	Array<int> index(nnodes);
	for( int i = 0; i < index.Size(); i++) index[i] = i;

	QuickSortI (nodekeys, index, MyLess<N>);

	Array<int> inverse_index(nnodes);
	for (int i = 0; i < index.Size(); i++ ) 	
	  inverse_index[index[i]] = i;
	
	int ndofs = 0;
	Array<int> first_node_dof (nnodes+1);
	
	for (int i = 0; i < nnodes; i++)
	  {
	    first_node_dof[i] = ndofs;
	    ndofs += nrdofs_per_node[index[i]];
	  }
	first_node_dof[nnodes] = ndofs;
	
	Array<SCAL> node_data(ndofs);     

	for (int i = 0; i < ndofs; i++)
	  if (ist.good())
	    LoadBin<SCAL> (ist, node_data[i]);
	
	for (int proc = 1; proc < ntasks; proc++)
	  {
	    Array<SCAL> loc_data (0);
	    Array<int> & nodenums_proc = *nodenums_of_procs[proc-1];

	    for (int i = 0; i < nodenums_proc.Size(); i++)
	      {
		int node = inverse_index[nodenums_proc[i]];
		loc_data.Append (node_data.Range (first_node_dof[node], first_node_dof[node+1]));
	      }
	    MyMPI_Send(loc_data,proc,13); 		
	  }
      }
#endif	
  }
  


  template <class SCAL>
  void S_GridFunction<SCAL> :: Save (ostream & ost) const
  {
    int ntasks = MyMPI_GetNTasks();
    const FESpace & fes = GetFESpace();
  
    if (ntasks == 1)
      {
	Array<int> dnums;	    
	for (NODE_TYPE nt = NT_VERTEX; nt <= NT_CELL; nt++)
	  {
	    int nnodes = ma.GetNNodes (nt);

	    Array<Vec<8, int> > nodekeys;
	    Array<int> pnums, compress;
	    for(int i = 0; i < nnodes; i++)
	      {
		fes.GetNodeDofNrs (nt, i,  dnums);
		if (dnums.Size() == 0) continue;
		
		switch (nt)
		  {
		  case NT_VERTEX: pnums.SetSize(1); pnums[0] = i; break;
		  case NT_EDGE: ma.GetEdgePNums (i, pnums); break;
		  case NT_FACE: ma.GetFacePNums (i, pnums); break;
		  case NT_CELL: ma.GetElPNums (i, pnums); break;
		  }
		Vec<8> key; 
		key = -1;
		for (int j = 0; j < pnums.Size(); j++)
		  key[j] = pnums[j];
		nodekeys.Append (key);
		compress.Append (i);
	      }
	    
	    nnodes = nodekeys.Size();

	    Array<int> index(nnodes);
	    for( int i = 0; i < index.Size(); i++) index[i] = i;
	    
	    QuickSortI (nodekeys, index, MyLess<8>);


	    for( int i = 0; i < nnodes; i++)
	      {
		fes.GetNodeDofNrs (nt, compress[index[i]],  dnums); 
		Vector<SCAL> elvec(dnums.Size());
		GetElementVector (dnums, elvec);
		
		for (int j = 0; j < elvec.Size(); j++)
		  SaveBin<SCAL>(ost, elvec(j));			
	      }
	  }
      }
#ifdef PARALLEL	 
    else
      {  
	GetVector().Cumulate();        
	SaveNodeType<1,NT_VERTEX>(ost);
	SaveNodeType<2,NT_EDGE>  (ost);
	SaveNodeType<4,NT_FACE>  (ost);
	SaveNodeType<8,NT_CELL>  (ost);
      }
#endif
  }





#ifdef PARALLEL
  template <typename T>
  inline void MyMPI_Gather (T d, MPI_Comm comm = ngs_comm)
  {
    static Timer t("dummy - gather"); RegionTimer r(t);
    
    MPI_Gather (&d, 1, MyGetMPIType<T>(), 
		NULL, 1, MyGetMPIType<T>(), 0, comm);
  }

  template <typename T>
  inline void MyMPI_GatherRoot (FlatArray<T> d, MPI_Comm comm = ngs_comm)
  {
    static Timer t("dummy - gather"); RegionTimer r(t);

    d[0] = T(0);
    MPI_Gather (MPI_IN_PLACE, 1, MyGetMPIType<T>(), 
		&d[0], 1, MyGetMPIType<T>(), 0,
		comm);
  }
#endif



  template <class SCAL>  template <int N, NODE_TYPE NTYPE>
  void  S_GridFunction<SCAL> :: SaveNodeType (ostream & ost) const
  {
#ifdef PARALLEL
    int id = MyMPI_GetId();
    int ntasks = MyMPI_GetNTasks();
    
    const FESpace & fes = GetFESpace();
    ParallelDofs & par = fes.GetParallelDofs ();
    
    if(id > 0)
      { 
	int nnodes = ma.GetNNodes (NTYPE);
	
	Array<Vec<N+1,int> > nodenums;
	Array<SCAL> data;
    
	Array<int> dnums, pnums;
    
	for (int i = 0; i < nnodes; i++)
	  {
	    fes.GetNodeDofNrs (NTYPE, i,  dnums);
	    
	    if (dnums.Size() == 0) continue;
	    if (!par.IsMasterDof (dnums[0])) continue;      

	    switch (NTYPE)
	      {
	      case NT_VERTEX: pnums.SetSize(1); pnums[0] = i; break;
	      case NT_EDGE: ma.GetEdgePNums (i, pnums); break;
	      case NT_FACE: ma.GetFacePNums (i, pnums); break;
	      case NT_CELL: ma.GetElPNums (i, pnums); break;
	      }

	    Vec<N+1, int> points;
	    points = -1;
	    for( int j = 0; j < pnums.Size(); j++)
	      points[j] = ma.GetGlobalNodeNum (Node(NT_VERTEX, pnums[j]));
	    points[N] = dnums.Size();
	    
	    nodenums.Append(points);
	    
	    Vector<SCAL> elvec(dnums.Size());
	    GetElementVector (dnums, elvec);
	    
	    for( int j = 0; j < dnums.Size(); j++)
	      data.Append(elvec(j));
	  }    
	
	MyMPI_Gather (nodenums.Size());
	MyMPI_Gather (data.Size());

	MyMPI_Send(nodenums,0,22);
	MyMPI_Send(data,0,23);
      }
    else
      {
	Array<Vec<N,int> > points(0);
	Array<Vec<2,int> > positions(0);
	

	Array<int> size_nodes(ntasks), size_data(ntasks);
	MyMPI_GatherRoot (size_nodes);
	MyMPI_GatherRoot (size_data);

	Array<MPI_Request> requests;

	Table<Vec<N+1,int> > table_nodes(size_nodes);
	for (int p = 1; p < ntasks; p++)
	  requests.Append (MyMPI_IRecv (table_nodes[p], p, 22));

	Table<SCAL> table_data(size_data);
	for (int p = 1; p < ntasks; p++)
	  requests.Append (MyMPI_IRecv (table_data[p], p, 23));
	MyMPI_WaitAll (requests);

	FlatArray<SCAL> data = table_data.AsArray();


	int size = 0;
	for( int proc = 1; proc < ntasks; proc++)
	  {
	    FlatArray<Vec<N+1,int> > locpoints = table_nodes[proc];
	    Vec<N,int>  temp;
	    
	    for( int j = 0; j < locpoints.Size(); j++ )
	      {
		int nodesize = locpoints[j][N];
		
		positions.Append (Vec<2,int> (size, nodesize));
		
		for( int k = 0; k < N; k++)
		  temp[k] = locpoints[j][k];
		
		points.Append( temp );
		size += nodesize;
	      }
	  }    
	
	Array<int> index(points.Size());
	for( int i = 0; i < index.Size(); i++) index[i] = i;
	
	static Timer ts ("Save Gridfunction, sort");
	static Timer tw ("Save Gridfunction, write");
	ts.Start();
	QuickSortI (points, index, MyLess<N>);
	ts.Stop();

	tw.Start();
	for( int i = 0; i < points.Size(); i++)
	  {
	    int start = positions[index[i]][0];
	    int end = positions[index[i]][1];
	    
	    for( int j = 0; j < end; j++)
	      SaveBin<SCAL>(ost, data[start++]);
	  }
	tw.Stop();	
      }
#endif	   
  }





  template <class SCAL>
  S_ComponentGridFunction<SCAL> :: 
  S_ComponentGridFunction (const S_GridFunction<SCAL> & agf_parent, int acomp)
    : S_GridFunction<SCAL> (*dynamic_cast<const CompoundFESpace&> (agf_parent.GetFESpace())[acomp], 
			    agf_parent.GetName()+"."+ToString (acomp+1), Flags()), 
      gf_parent(agf_parent), comp(acomp)
  { 
    this->SetVisual(agf_parent.GetVisual());
    const CompoundFESpace * cfe = dynamic_cast<const CompoundFESpace *>(&this->GetFESpace());
    if (cfe)
      {
	int nsp = cfe->GetNSpaces();
	this->compgfs.SetSize(nsp);
	for (int i = 0; i < nsp; i++)
	  this->compgfs[i] = new S_ComponentGridFunction<SCAL> (*this, i);
      }    
    this->Visualize (this->name);
  }

  template <class SCAL>
  S_ComponentGridFunction<SCAL> :: 
  ~S_ComponentGridFunction ()
  {
    this -> vec = NULL;  // base-class desctructor must not delete the vector
  }


  template <class SCAL>
  void S_ComponentGridFunction<SCAL> :: Update()
  {
    const CompoundFESpace & cfes = dynamic_cast<const CompoundFESpace&> (gf_parent.GetFESpace());

    this -> vec.SetSize (gf_parent.GetMultiDim());
    for (int i = 0; i < gf_parent.GetMultiDim(); i++)
      (this->vec)[i] = gf_parent.GetVector(i).Range (cfes.GetRange(comp));
  
    this -> level_updated = this -> ma.GetNLevels();

    for (int i = 0; i < this->compgfs.Size(); i++)
      this->compgfs[i]->Update();
  }



  template <class TV>
  T_GridFunction<TV> ::
  T_GridFunction (const FESpace & afespace, const string & aname, const Flags & flags)
    : S_GridFunction<TSCAL> (afespace, aname, flags)
  {
    vec.SetSize (this->multidim);
    vec = 0;

    const CompoundFESpace * cfe = dynamic_cast<const CompoundFESpace *>(&this->GetFESpace());
    if (cfe)
      {
	int nsp = cfe->GetNSpaces();
	compgfs.SetSize(nsp);
	for (int i = 0; i < nsp; i++)
	  compgfs[i] = new S_ComponentGridFunction<TSCAL> (*this, i);
      }    

    this->Visualize (this->name);
  }

  template <class TV>
  T_GridFunction<TV> :: ~T_GridFunction()
  {
    ;
  }


  template <class TV>
  void T_GridFunction<TV> :: Update () 
  {
    try
      {
        if (this->GetFESpace().GetLevelUpdated() < this->ma.GetNLevels())
          {
            LocalHeap llh (1000000, "gridfunction update");
            const_cast<FESpace&> (this->GetFESpace()).Update (llh);
            const_cast<FESpace&> (this->GetFESpace()).FinalizeUpdate (llh);
          }



	int ndof = this->GetFESpace().GetNDof();

	for (int i = 0; i < this->multidim; i++)
	  {
	    if (vec[i] && ndof == vec[i]->Size())
	      break;
	    
	    BaseVector * ovec = vec[i];
	
#ifdef PARALLEL
	    if ( & this->GetFESpace().GetParallelDofs() )
	      vec[i] = new ParallelVVector<TV> (ndof, &this->GetFESpace().GetParallelDofs(), CUMULATED);
	    else
#endif
 	      vec[i] = new VVector<TV> (ndof);
	    

	    (*vec[i]) = TSCAL(0);

	    if (this->nested && ovec && this->GetFESpace().GetProlongation())
	      {
		*vec[i]->Range (0, ovec->Size()) += (*ovec);

		const_cast<ngmg::Prolongation&> (*this->GetFESpace().GetProlongation()).Update();
		
		this->GetFESpace().GetProlongation()->ProlongateInline
		  (this->GetMeshAccess().GetNLevels()-1, *vec[i]);
	      }

	    //	    if (i == 0)
            // cout << "visualize" << endl;
            // Visualize (this->name);
	    
	    delete ovec;
	  }
	
	this -> level_updated = this -> ma.GetNLevels();

	// const CompoundFESpace * cfe = dynamic_cast<const CompoundFESpace *>(&GridFunction :: GetFESpace());
	// if (cfe)
	for (int i = 0; i < compgfs.Size(); i++)
	  compgfs[i]->Update();
      }
    catch (exception & e)
      {
	Exception e2 (e.what());
	e2.Append ("\nIn GridFunction::Update()\n");
	throw e2;
      }
    catch (Exception & e)
      {
	e.Append ("In GridFunction::Update()\n");
	throw e;
      }    
  }
 


  GridFunction * CreateGridFunction (const FESpace * space,
				     const string & name, const Flags & flags)
  {
    GridFunction * gf = 
      CreateVecObject  <T_GridFunction, GridFunction> // , const FESpace, const string, const Flags>
      (space->GetDimension() * int(flags.GetNumFlag("cacheblocksize",1)), 
       space->IsComplex(), *space, name, flags);
  
    gf->SetCacheBlockSize(int(flags.GetNumFlag("cacheblocksize",1)));
    
    return gf;
  }












  GridFunctionCoefficientFunction :: 
  GridFunctionCoefficientFunction (const GridFunction & agf, const double &acomp)
    : gf(agf), diffop (NULL), comp (acomp) 
  {
    diffop = gf.GetFESpace().GetEvaluator();
  }

  GridFunctionCoefficientFunction :: 
  GridFunctionCoefficientFunction (const GridFunction & agf, 
				   const DifferentialOperator * adiffop, const double &acomp)
    : gf(agf), diffop (adiffop), comp (acomp) 
  {
    ;
  }

  GridFunctionCoefficientFunction :: 
  ~GridFunctionCoefficientFunction ()
  {
    ;
  }

  int GridFunctionCoefficientFunction::Dimension() const
  { 
    if (diffop) return diffop->Dim();
    return gf.GetFESpace().GetIntegrator()->DimFlux();
  }

  bool GridFunctionCoefficientFunction::IsComplex() const
  { 
    return gf.GetFESpace().IsComplex(); 
  }


  double GridFunctionCoefficientFunction :: 
  Evaluate (const BaseMappedIntegrationPoint & ip) const
  {
    VectorMem<10> flux(Dimension());
    Evaluate (ip, flux);
    return flux(0);
  }

  void GridFunctionCoefficientFunction :: 
  Evaluate (const BaseMappedIntegrationPoint & ip, FlatVector<> result) const
  {
    LocalHeapMem<100000> lh2 ("GridFunctionCoefficientFunction, Eval 2");
    static Timer timer ("GFCoeffFunc::Eval-scal");
    RegionTimer reg (timer);
    const int compi = boost::math::iround(comp);

    const ElementTransformation & trafo = ip.GetTransformation();
    
    int elnr = trafo.GetElementNr();
    bool boundary = trafo.Boundary();

    const FESpace & fes = gf.GetFESpace();
    const MeshAccess & ma = fes.GetMeshAccess();

    if (!trafo.BelongsToMesh (&ma))
      {
        IntegrationPoint rip;
        int elnr = ma.FindElementOfPoint 
          (static_cast<const DimMappedIntegrationPoint<2>&> (ip).GetPoint(),
           rip, true);  // buildtree not yet threadsafe
        const ElementTransformation & trafo2 = ma.GetTrafo(elnr, boundary, lh2);
        return Evaluate (trafo2(rip, lh2), result);
      }


    if (!fes.DefinedOn (trafo.GetElementIndex(), boundary))
      { 
        result = 0.0; 
        return;
      }
    
    const FiniteElement & fel = fes.GetFE (elnr, boundary, lh2);
    int dim = fes.GetDimension();
    
    ArrayMem<int, 50> dnums;
    fes.GetDofNrs (elnr, boundary, dnums);
    
    VectorMem<50> elu(dnums.Size()*dim);

    gf.GetElementVector (compi, dnums, elu);
    fes.TransformVec (elnr, boundary, elu, TRANSFORM_SOL);

    if (diffop)
      diffop->Apply (fel, ip, elu, result, lh2);
    else
      fes.GetIntegrator(boundary) -> CalcFlux (fel, ip, elu, result, false, lh2);
  }

  void GridFunctionCoefficientFunction :: 
  Evaluate (const BaseMappedIntegrationPoint & ip, FlatVector<Complex> result) const
  {
    LocalHeapMem<100000> lh2 ("GridFunctionCoefficientFunction, Eval complex");
    static Timer timer ("GFCoeffFunc::Eval-scal");
    RegionTimer reg (timer);
    const int compi = boost::math::iround(comp);


    const int elnr = ip.GetTransformation().GetElementNr();
    bool boundary = ip.GetTransformation().Boundary();

    const FESpace & fes = gf.GetFESpace();

    if (!fes.DefinedOn (ip.GetTransformation().GetElementIndex(), boundary))
      { 
        result = 0.0; 
        return;
      }
    
    const FiniteElement & fel = fes.GetFE (elnr, boundary, lh2);
    int dim = fes.GetDimension();
    
    ArrayMem<int, 50> dnums;
    fes.GetDofNrs (elnr, boundary, dnums);
    
    VectorMem<50, Complex> elu(dnums.Size()*dim);

    gf.GetElementVector (compi, dnums, elu);
    fes.TransformVec (elnr, boundary, elu, TRANSFORM_SOL);

    if (diffop)
      diffop->Apply (fel, ip, elu, result, lh2);
    else
      fes.GetIntegrator(boundary) -> CalcFlux (fel, ip, elu, result, false, lh2);
  }


  void GridFunctionCoefficientFunction :: 
  Evaluate (const BaseMappedIntegrationRule & ir, FlatMatrix<double> values) const
  {
    LocalHeapMem<100000> lh2("GridFunctionCoefficientFunction - Evalute 3");
    static Timer timer ("GFCoeffFunc::Eval-vec");
    RegionTimer reg (timer);
    const int compi = boost::math::iround(comp);

    const ElementTransformation & trafo = ir.GetTransformation();
    
    int elnr = trafo.GetElementNr();
    bool boundary = trafo.Boundary();

    const FESpace & fes = gf.GetFESpace();

    if (!trafo.BelongsToMesh ((void*)(&fes.GetMeshAccess())))
      {
        for (int i = 0; i < ir.Size(); i++)
          Evaluate (ir[i], values.Row(i));
        return;
      }
    
    if (!fes.DefinedOn(trafo.GetElementIndex(), boundary)) 
      { 
        values = 0.0; 
        return;
      }
    
    const FiniteElement & fel = fes.GetFE (elnr, boundary, lh2);
    int dim = fes.GetDimension();

    ArrayMem<int, 50> dnums;
    fes.GetDofNrs (elnr, boundary, dnums);
    
    VectorMem<50> elu(dnums.Size()*dim);

    gf.GetElementVector (compi, dnums, elu);
    fes.TransformVec (elnr, boundary, elu, TRANSFORM_SOL);

    if (diffop)
      diffop->Apply (fel, ir, elu, values, lh2);
    else
      fes.GetIntegrator(boundary) ->CalcFlux (fel, ir, elu, values, false, lh2);
  }





  template <class SCAL>
  VisualizeGridFunction<SCAL> ::
  VisualizeGridFunction (const MeshAccess & ama,
			 const GridFunction * agf,
			 const BilinearFormIntegrator * abfi2d,
			 const BilinearFormIntegrator * abfi3d,
			 bool aapplyd)

    : SolutionData (agf->GetName(), -1, agf->GetFESpace().IsComplex()),
      ma(ama), gf(dynamic_cast<const S_GridFunction<SCAL>*> (agf)), 
      applyd(aapplyd) // , lh(10000013, "VisualizedGridFunction 2")
  { 
    if(abfi2d)
      bfi2d.Append(abfi2d);
    if(abfi3d)
      bfi3d.Append(abfi3d);

    if (abfi2d) components = abfi2d->DimFlux();
    if (abfi3d) components = abfi3d->DimFlux();
    if (iscomplex) components *= 2;
    multidimcomponent = 0;
  }

  template <class SCAL>
  VisualizeGridFunction<SCAL> ::
  VisualizeGridFunction (const MeshAccess & ama,
			 const GridFunction * agf,
			 const Array<BilinearFormIntegrator *> & abfi2d,
			 const Array<BilinearFormIntegrator *> & abfi3d,
			 bool aapplyd)

    : SolutionData (agf->GetName(), -1, agf->GetFESpace().IsComplex()),
      ma(ama), gf(dynamic_cast<const S_GridFunction<SCAL>*> (agf)), 
      applyd(aapplyd) // , lh(10000002, "VisualizeGridFunction")
  { 
    for(int i=0; i<abfi2d.Size(); i++)
      bfi2d.Append(abfi2d[i]);
    for(int i=0; i<abfi3d.Size(); i++)
      bfi3d.Append(abfi3d[i]);
    

    if (bfi2d.Size()) components = bfi2d[0]->DimFlux();
    if (bfi3d.Size()) components = bfi3d[0]->DimFlux();

    if (iscomplex) components *= 2;
    multidimcomponent = 0;
  }

  template <class SCAL>
  VisualizeGridFunction<SCAL> :: ~VisualizeGridFunction ()
  {
    ;
  }
  

  template <class SCAL>
  bool VisualizeGridFunction<SCAL> :: 
  GetValue (int elnr, double lam1, double lam2, double lam3,
            double * values) 
  { 
    // static Timer t("visgf::GetValue");
    // RegionTimer reg(t);
    // t.AddFlops (1);

    try
      {
	LocalHeapMem<100000> lh("visgf::getvalue");
	if (!bfi3d.Size()) return false;
	if (gf -> GetLevelUpdated() < ma.GetNLevels()) return false;
	const FESpace & fes = gf->GetFESpace();

	int dim     = fes.GetDimension();
        ElementId ei(VOL,elnr);

	if ( !fes.DefinedOn(ei)) return false;

	HeapReset hr(lh);
	
	ElementTransformation & eltrans = ma.GetTrafo (ei, lh);
	const FiniteElement & fel = fes.GetFE (ei, lh);

	Array<int> dnums (fel.GetNDof(), lh);
	fes.GetDofNrs (ei, dnums);

	FlatVector<SCAL> elu(dnums.Size() * dim, lh);

	if(gf->GetCacheBlockSize() == 1)
	  {
	    gf->GetElementVector (multidimcomponent, dnums, elu);
	  }
	else
	  {
	    FlatVector<SCAL> elu2(dnums.Size()*dim*gf->GetCacheBlockSize(),lh);
	    //gf->GetElementVector (multidimcomponent, dnums, elu2);
	    gf->GetElementVector (0, dnums, elu2);
	    for(int i=0; i<elu.Size(); i++)
	      elu[i] = elu2[i*gf->GetCacheBlockSize()+multidimcomponent];
	  }

	fes.TransformVec (elnr, 0, elu, TRANSFORM_SOL);

	IntegrationPoint ip(lam1, lam2, lam3, 0);
	MappedIntegrationPoint<3,3> mip (ip, eltrans);

        for (int i = 0; i < components; i++) values[i] = 0;

        bool ok = false;
	for(int j = 0; j < bfi3d.Size(); j++)
          if (bfi3d[j]->DefinedOn(ma.GetElIndex(ei)))
            {
              HeapReset hr(lh);
              FlatVector<SCAL> flux(bfi3d[j] -> DimFlux(), lh);
              bfi3d[j]->CalcFlux (fel, mip, elu, flux, applyd, lh);
              
              for (int i = 0; i < components; i++)
                values[i] += ((double*)(void*)&flux(0))[i];
              ok = true;
            }

        return ok; 
      }
    
    catch (Exception & e)
      {
        cout << "GetValue caught exception" << endl
             << e.What();
        return false;
      }
    catch (exception & e)
      {
        cout << "GetValue caught exception" << endl
             << typeid(e).name() << endl;
        return false;
      }
  }


  template <class SCAL>
  bool VisualizeGridFunction<SCAL> :: 
  GetValue (int elnr, 
            const double xref[], const double x[], const double dxdxref[],
            double * values) 
  { 
    static Timer t("visgf::GetValue2");
    RegionTimer reg(t);
    
    try
      {
	LocalHeapMem<100000> lh("visgf::getvalue");
        
	if (!bfi3d.Size()) return 0;
	if (gf -> GetLevelUpdated() < ma.GetNLevels()) return 0;
	
	const FESpace & fes = gf->GetFESpace();
	
	int dim     = fes.GetDimension();
        ElementId ei(VOL,elnr);
	
	if ( !fes.DefinedOn(ei) ) return 0;
	
	const FiniteElement * fel = &fes.GetFE (ei, lh);

	Array<int> dnums(fel->GetNDof(), lh);
	fes.GetDofNrs (ei, dnums);

	FlatVector<SCAL> elu (fel->GetNDof() * dim, lh);

	if(gf->GetCacheBlockSize() == 1)
	  {
	    gf->GetElementVector (multidimcomponent, dnums, elu);
	  }
	else
	  {
	    FlatVector<SCAL> elu2(dnums.Size()*dim*gf->GetCacheBlockSize(),lh);
	    //gf->GetElementVector (multidimcomponent, dnums, elu2);
	    gf->GetElementVector (0, dnums, elu2);
	    for(int i=0; i<elu.Size(); i++)
	      elu[i] = elu2[i*gf->GetCacheBlockSize()+multidimcomponent];
	  }
	fes.TransformVec (elnr, 0, elu, TRANSFORM_SOL);
	
	
	HeapReset hr(lh);
	
	Vec<3> vx;
	Mat<3,3> mdxdxref;
	for (int i = 0; i < 3; i++)
	  {
	    vx(i) = x[i];
	    for (int j = 0; j < 3; j++)
	      mdxdxref(i,j) = dxdxref[3*i+j];
	  }
	
	ElementTransformation & eltrans = ma.GetTrafo (elnr, false, lh);
	IntegrationPoint ip(xref[0], xref[1], xref[2], 0);
	MappedIntegrationPoint<3,3> sip (ip, eltrans, vx, mdxdxref);
	
        for (int i = 0; i < components; i++) values[i] = 0;
        bool ok = false;

	for(int j = 0; j < bfi3d.Size(); j++)
          if (bfi3d[j]->DefinedOn(ma.GetElIndex(ei)))
            {
              FlatVector<SCAL> flux (bfi3d[j]->DimFlux(), lh);
              bfi3d[j]->CalcFlux (*fel, sip, elu, flux, applyd, lh); 
              
              for (int i = 0; i < components; i++)
                values[i] += ((double*)(void*)&flux(0))[i];
              ok = true;
            }
	
	return ok; 
      }
    catch (Exception & e)
      {
        cout << "GetValue 2 caught exception" << endl
             << e.What();
        return false;
      }
    catch (exception & e)
      {
        cout << "GetValue 2 caught exception" << endl
             << typeid(e).name() << endl;
        return false;
      }
  }



  template <class SCAL>
  bool VisualizeGridFunction<SCAL> ::
  GetMultiValue (int elnr, int facetnr, int npts,
		 const double * xref, int sxref,
		 const double * x, int sx,
		 const double * dxdxref, int sdxdxref,
		 double * values, int svalues)
  {
    static Timer t("visgf::GetMultiValue");
    static Timer t1("visgf::GetMultiValue - 1");
    RegionTimer reg(t);

    try
      {
        if (!bfi3d.Size()) return 0;
        if (gf -> GetLevelUpdated() < ma.GetNLevels()) return 0;

        const FESpace & fes = gf->GetFESpace();
        int dim = fes.GetDimension();
	
        // HeapReset hr(lh);
        LocalHeapMem<100000> lh("visgf::GetMultiValue");

	ElementTransformation & eltrans = ma.GetTrafo (elnr, false, lh);
	const FiniteElement * fel = &fes.GetFE (elnr, lh);


	ArrayMem<int,200> dnums(fel->GetNDof());
	VectorMem<200,SCAL> elu (fel->GetNDof() * dim);

	fes.GetDofNrs (elnr, dnums);

        if(gf->GetCacheBlockSize() == 1)
          {
            gf->GetElementVector (multidimcomponent, dnums, elu);
          }
        else
          {
            FlatVector<SCAL> elu2(dnums.Size()*dim*gf->GetCacheBlockSize(),lh);
            gf->GetElementVector (0, dnums, elu2);
            for(int i=0; i<elu.Size(); i++)
              elu[i] = elu2[i*gf->GetCacheBlockSize()+multidimcomponent];
          }
        
        fes.TransformVec (elnr, false, elu, TRANSFORM_SOL);
        
	if (!fes.DefinedOn(eltrans.GetElementIndex()))return 0;

	IntegrationRule ir; 
	ir.SetAllocSize(npts);
	for (int i = 0; i < npts; i++)
	  ir.Append (IntegrationPoint (xref[i*sxref], xref[i*sxref+1], xref[i*sxref+2]));

	MappedIntegrationRule<3,3> mir(ir, eltrans, 1, lh);

	for (int k = 0; k < npts; k++)
	  {
	    Mat<3,3> & mdxdxref = *new((double*)(dxdxref+k*sdxdxref)) Mat<3,3>;
	    FlatVec<3> vx((double*)x + k*sx);
	    mir[k] = MappedIntegrationPoint<3,3> (ir[k], eltrans, vx, mdxdxref);
	  }

	for (int k = 0; k < npts; k++)
	  for (int i = 0; i < components; i++)
	    values[k*svalues+i] = 0.0;

        bool isdefined = false;
	for(int j = 0; j < bfi3d.Size(); j++)
	  {
            if (!bfi3d[j]->DefinedOn(eltrans.GetElementIndex())) continue;
            isdefined = true;

	    HeapReset hr(lh);
            
	    FlatMatrix<SCAL> flux(npts, bfi3d[j]->DimFlux(), lh);
            t1.Start();
	    bfi3d[j]->CalcFlux (*fel, mir, elu, flux, applyd, lh);

            t1.Stop();
            t1.AddFlops (elu.Size()*mir.Size());
	    for (int k = 0; k < npts; k++)
	      for (int i = 0; i < components; i++)
		values[k*svalues+i] += ((double*)(void*)&flux(k,0))[i];
          }

        return isdefined;
      }
    catch (Exception & e)
      {
        cout << "GetMultiValue caught exception" << endl
             << e.What();
        return 0;
      }
    catch (exception & e)
      {
        cout << "GetMultiValue caught exception" << endl
             << typeid(e).name() << endl;
        return false;
      }
  }


















  template <class SCAL>
  bool VisualizeGridFunction<SCAL> :: GetSurfValue (int elnr, int facetnr, 
                                                    double lam1, double lam2, 
                                                    double * values) 
  { 
    static Timer t("visgf::GetSurfValue");
    RegionTimer reg(t);


    try
      {
	if (!bfi2d.Size()) return 0;
	if (gf -> GetLevelUpdated() < ma.GetNLevels()) return 0;

	bool bound = (ma.GetDimension() == 3);
	const FESpace & fes = gf->GetFESpace();

	int dim = fes.GetDimension();

	LocalHeapMem<100000> lh("VisGF::GetSurfValue");
	const FiniteElement * fel = &fes.GetFE (elnr, bound, lh);

	ArrayMem<int, 100> dnums;
	fes.GetDofNrs (elnr, bound, dnums);

	FlatVector<SCAL> elu (dnums.Size() * dim, lh);

	if(gf->GetCacheBlockSize() == 1)
	  {
	    gf->GetElementVector (multidimcomponent, dnums, elu);
	  }
	else
	  {
	    FlatVector<SCAL> elu2(dnums.Size()*dim*gf->GetCacheBlockSize(),lh);
	    //gf->GetElementVector (multidimcomponent, dnums, elu2);
	    gf->GetElementVector (0, dnums, elu2);
	    for(int i=0; i<elu.Size(); i++)
	      elu[i] = elu2[i*gf->GetCacheBlockSize()+multidimcomponent];
	  }

	fes.TransformVec (elnr, bound, elu, TRANSFORM_SOL);

	ElementTransformation & eltrans = ma.GetTrafo (elnr, bound, lh);
	if (!fes.DefinedOn(eltrans.GetElementIndex(), bound)) return false;

	IntegrationPoint ip(lam1, lam2, 0, 0);
	ip.FacetNr() = facetnr;

	BaseMappedIntegrationPoint & mip = eltrans(ip, lh);
	for(int j = 0; j < bfi2d.Size(); j++)
	  {
	    FlatVector<SCAL> flux(bfi2d[j]->DimFlux(), lh);
	    bfi2d[j]->CalcFlux (*fel, mip, elu, flux, applyd, lh);
	
	    for (int i = 0; i < components; i++)
	      {
		if(j == 0) values[i] = 0;
		values[i] += ((double*)(void*)&flux(0))[i];
	      }
	  }

	return true;
      }
    catch (Exception & e)
      {
        cout << "GetSurfaceValue caught exception" << endl
             << e.What();

        return 0;
      }
      
  }




  template <class SCAL>
  bool VisualizeGridFunction<SCAL> :: 
  GetSurfValue (int elnr, int facetnr, 
		const double xref[], const double x[], const double dxdxref[],
		double * values) 
  { 
    static Timer t("visgf::GetSurfValue 2");
    RegionTimer reg(t);

    // cout << "VisGF::GetSurfValue2" << endl;
    try
      {
        if (!bfi2d.Size()) return 0;
        if (gf -> GetLevelUpdated() < ma.GetNLevels()) return 0;

        bool bound = (ma.GetDimension() == 3);
        const FESpace & fes = gf->GetFESpace();

        int dim     = fes.GetDimension();

	// lh.CleanUp();
        LocalHeapMem<100000> lh("VisGF::GetSurfValue");
	const FiniteElement * fel = &fes.GetFE (elnr, bound, lh);
	
	Array<int> dnums(fel->GetNDof(), lh);
	FlatVector<SCAL> elu (fel->GetNDof()*dim, lh);

	fes.GetDofNrs (elnr, bound, dnums);
	
	if(gf->GetCacheBlockSize() == 1)
	  {
	    gf->GetElementVector (multidimcomponent, dnums, elu);
	  }
	else
	  {
	    FlatVector<SCAL> elu2(dnums.Size()*dim*gf->GetCacheBlockSize(),lh);
	    //gf->GetElementVector (multidimcomponent, dnums, elu2);
	    gf->GetElementVector (0, dnums, elu2);
	    for(int i=0; i<elu.Size(); i++)
	      elu[i] = elu2[i*gf->GetCacheBlockSize()+multidimcomponent];
	  }
	
	fes.TransformVec (elnr, bound, elu, TRANSFORM_SOL);
	
	HeapReset hr(lh);
	ElementTransformation & eltrans = ma.GetTrafo (elnr, bound, lh);
        if (!fes.DefinedOn(eltrans.GetElementIndex(), bound)) return false;

        IntegrationPoint ip(xref[0], xref[1], 0, 0);
	ip.FacetNr() = facetnr;
        if (bound)
          {
            // Vec<3> vx;
            Mat<3,2> mdxdxref;
            for (int i = 0; i < 3; i++)
              {
                // vx(i) = x[i];
                for (int j = 0; j < 2; j++)
                  mdxdxref(i,j) = dxdxref[2*i+j];
              }
            MappedIntegrationPoint<2,3> mip (ip, eltrans, (double*)x, mdxdxref); 
            for (int i = 0; i < components; i++)
              values[i] = 0.0;
            for(int j = 0; j<bfi2d.Size(); j++)
              {
		FlatVector<SCAL> flux(bfi2d[j]->DimFlux(), lh);
                bfi2d[j]->CalcFlux (*fel, mip, elu, flux, applyd, lh);
                for (int i = 0; i < components; i++)
                  values[i] += ((double*)(void*)&flux(0))[i];
              }
          }
        else
          {
            // Vec<2> vx;
            Mat<2,2> mdxdxref;
            for (int i = 0; i < 2; i++)
              {
                // vx(i) = x[i];
                for (int j = 0; j < 2; j++)
                  mdxdxref(i,j) = dxdxref[2*i+j];
              }
            MappedIntegrationPoint<2,2> mip (ip, eltrans, (double*)x, mdxdxref); 

            for (int i = 0; i < components; i++)
              values[i] = 0.0;
            for(int j = 0; j<bfi2d.Size(); j++)
              {
                FlatVector<SCAL> flux(bfi2d[j]->DimFlux(), lh);
                bfi2d[j]->CalcFlux (*fel, mip, elu, flux, applyd, lh);
                for (int i = 0; i < components; i++)
                  values[i] += ((double*)(void*)&flux(0))[i];
              }
          }

        return 1; 
      }
    catch (Exception & e)
      {
        cout << "GetSurfaceValue2 caught exception" << endl
             << e.What();

        return 0;
      }
      
  }






  template <class SCAL>
  bool VisualizeGridFunction<SCAL> ::
  GetMultiSurfValue (int elnr, int facetnr, int npts,
                     const double * xref, int sxref,
                     const double * x, int sx,
                     const double * dxdxref, int sdxdxref,
                     double * values, int svalues)
  {
    static Timer t("visgf::GetMultiSurfValue");
    static Timer ta("visgf::GetMultiSurfValue a");
    static Timer tb("visgf::GetMultiSurfValue b");
    static Timer tc("visgf::GetMultiSurfValue c");
    RegionTimer reg(t);

    try
      {
        if (!bfi2d.Size()) return 0;
        if (gf -> GetLevelUpdated() < ma.GetNLevels()) return 0;

        bool bound = (ma.GetDimension() == 3);
	
        const FESpace & fes = gf->GetFESpace();
        int dim = fes.GetDimension();
        
        
        LocalHeapMem<100000> lh("visgf::getmultisurfvalue");

	ElementTransformation & eltrans = ma.GetTrafo (elnr, bound, lh);

        ArrayMem<int, 100> dnums;
	fes.GetDofNrs (elnr, bound, dnums);
	const FiniteElement * fel = &fes.GetFE (elnr, bound, lh);

        FlatVector<SCAL> elu(dnums.Size() * dim, lh);

        if(gf->GetCacheBlockSize() == 1)
          {
            gf->GetElementVector (multidimcomponent, dnums, elu);
          }
        else
          {
            FlatVector<SCAL> elu2(dnums.Size()*dim*gf->GetCacheBlockSize(),lh);
            gf->GetElementVector (0, dnums, elu2);
            for(int i=0; i<elu.Size(); i++)
              elu[i] = elu2[i*gf->GetCacheBlockSize()+multidimcomponent];
          }
        
        fes.TransformVec (elnr, bound, elu, TRANSFORM_SOL);

        if (!fes.DefinedOn(eltrans.GetElementIndex(), bound)) return false;
        
	SliceMatrix<> mvalues(npts, components, svalues, values);
	mvalues = 0;

	IntegrationRule ir(npts, lh);
	for (int i = 0; i < npts; i++)
	  {
	    ir[i] = IntegrationPoint (xref[i*sxref], xref[i*sxref+1]);
	    ir[i].FacetNr() = facetnr;
	  }
        
        if (bound)
          {
	    MappedIntegrationRule<2,3> mir(ir, eltrans, 1, lh);

	    for (int k = 0; k < npts; k++)
	      {
		Mat<2,3> & mdxdxref = *new((double*)(dxdxref+k*sdxdxref)) Mat<2,3>;
		FlatVec<3> vx( (double*)x + k*sx);
		mir[k] = MappedIntegrationPoint<2,3> (ir[k], eltrans, vx, mdxdxref);
	      }

            bool isdefined = false;
	    for(int j = 0; j < bfi2d.Size(); j++)
	      {
                if (!bfi2d[j]->DefinedOn(eltrans.GetElementIndex())) continue;
                isdefined = true;

		FlatMatrix<SCAL> flux(npts, bfi2d[j]->DimFlux(), lh);
		bfi2d[j]->CalcFlux (*fel, mir, elu, flux, applyd, lh);
		for (int k = 0; k < npts; k++)
		  mvalues.Row(k) += FlatVector<> (components, &flux(k,0));
	      }
            return isdefined;
          }
        else
          {
            ta.Start();
	    MappedIntegrationRule<2,2> mir(ir, eltrans, 1, lh);
            
	    for (int k = 0; k < npts; k++)
	      {
		Mat<2,2> & mdxdxref = *new((double*)(dxdxref+k*sdxdxref)) Mat<2,2>;
		FlatVec<2> vx( (double*)x + k*sx);
		mir[k] = MappedIntegrationPoint<2,2> (ir[k], eltrans, vx, mdxdxref);
	      }

            ta.Stop();
            bool isdefined = false;
	    for(int j = 0; j < bfi2d.Size(); j++)
	      {
                if (!bfi2d[j]->DefinedOn(eltrans.GetElementIndex())) continue;
                isdefined = true;

		FlatMatrix<SCAL> flux(npts, bfi2d[j]->DimFlux(), lh);
                tc.Start();
		bfi2d[j]->CalcFlux (*fel, mir, elu, flux, applyd, lh);
                tc.Stop();

		for (int k = 0; k < npts; k++)
		  mvalues.Row(k) += FlatVector<> (components, &flux(k,0));
	      }
            return isdefined;
          }
      }
    catch (Exception & e)
      {
        cout << "GetMultiSurfaceValue caught exception" << endl
             << e.What();

        return 0;
      }
  }




  template <class SCAL>
  bool VisualizeGridFunction<SCAL> ::
  GetSegmentValue (int segnr, double xref, double * values)
  {
    if (ma.GetDimension() != 1) return false;

    LocalHeapMem<100000> lh("visgf::getsegmentvalue");

    const FESpace & fes = gf->GetFESpace();
    const DifferentialOperator * eval = fes.GetEvaluator (VOL);
    FlatVector<> fvvalues (eval->Dim(), values);

    const FiniteElement & fel = fes.GetFE (segnr, VOL, lh);
    Array<int> dnums(fel.GetNDof(), lh);
    fes.GetDofNrs (segnr, dnums);

    FlatVector<> elvec(fes.GetDimension()*dnums.Size(), lh);
    gf->GetElementVector (dnums, elvec);
    
    const ElementTransformation & trafo = ma.GetTrafo (segnr, VOL, lh);
    IntegrationPoint ip (xref);

    eval -> Apply (fel, trafo(ip, lh), elvec, fvvalues, lh);

    return true;
  }










  
  template <class SCAL>
  void VisualizeGridFunction<SCAL> :: 
  Analyze(Array<double> & minima, Array<double> & maxima, Array<double> & averages, int component)
  {
    cout << "VisGF::Analyze" << endl;
    int ndomains = 0;

    if (bfi3d.Size()) 
      ndomains = ma.GetNDomains();
    else if(bfi2d.Size()) 
      ndomains = ma.GetNBoundaries();

    Array<double> volumes(ndomains);

    Analyze(minima,maxima,averages,volumes,component);
    
    for(int i=0; i<ndomains; i++)
      {
	if(component == -1)
	  for(int j=0; j<components; j++)
	    averages[i*components+j] /= volumes[i];
	else
	  averages[i] /= volumes[i];
      }
  }
  

  template <class SCAL>
  void VisualizeGridFunction<SCAL> :: 
  Analyze(Array<double> & minima, Array<double> & maxima, Array<double> & averages_times_volumes, Array<double> & volumes, int component)
  {
    cout << "VisGF::Analyze2" << endl;
    const FESpace & fes = gf->GetFESpace();

    int domain;
    double *val;
    int pos;
    double vol;

    /*
      int ndomains;
      if(bfi3d.Size()) ndomains = ma.GetNDomains();
      else if(bfi2d.Size()) ndomains = ma.GetNBoundaries();
    */

    Array<double> posx;
    Array<double> posy;
    Array<double> posz;
    ELEMENT_TYPE cache_type = ET_SEGM;
	
    LocalHeapMem<10000> lh2("Gridfunction - Analyze");
	
    val = new double[components];
			

    for(int i=0; i<minima.Size(); i++)
      {
	minima[i] = 1e100;
	maxima[i] = -1e100;
	averages_times_volumes[i] = 0;
      }

    for(int i=0; i<volumes.Size(); i++) volumes[i] = 0;
    
    void * heapp = lh2.GetPointer();
    if(bfi3d.Size())
      {
	for(int i=0; i<ma.GetNE(); i++)
	  {
	    const FiniteElement & fel = fes.GetFE(i,lh2);
	    
	    domain = ma.GetElIndex(i);
	    
	    vol = ma.ElementVolume(i);
	    
	    volumes[domain] += vol;
	    
	    if(fel.ElementType() != cache_type)
	      {
		posx.DeleteAll(); posy.DeleteAll(); posz.DeleteAll(); 
		switch(fel.ElementType())
		  {
		  case ET_TET:
		    posx.Append(0.25); posy.Append(0.25); posz.Append(0.25); 
		    posx.Append(0); posy.Append(0); posz.Append(0); 
		    posx.Append(1); posy.Append(0); posz.Append(0);
		    posx.Append(0); posy.Append(1); posz.Append(0);
		    posx.Append(0); posy.Append(0); posz.Append(1);
		    break;
		  case ET_HEX:
		    posx.Append(0.5); posy.Append(0.5); posz.Append(0.5);  
		    posx.Append(0); posy.Append(0); posz.Append(0); 
		    posx.Append(0); posy.Append(0); posz.Append(1);
		    posx.Append(0); posy.Append(1); posz.Append(0);
		    posx.Append(0); posy.Append(1); posz.Append(1);  
		    posx.Append(1); posy.Append(0); posz.Append(0); 
		    posx.Append(1); posy.Append(0); posz.Append(1);
		    posx.Append(1); posy.Append(1); posz.Append(0);
		    posx.Append(1); posy.Append(1); posz.Append(1);
		    break;
                  default:
                    {
                      bool firsttime = 1;
                      if (firsttime)
                        {
                          cerr << "WARNING::VisGridFunction::Analyze: unsupported element "
                               << ElementTopology::GetElementName(fel.ElementType()) << endl;
                          firsttime = 0;
                        }
                      break;
                    } 
		  }
		cache_type = fel.ElementType();
	      }
	    
	    
	    for(int k=0; k<posx.Size(); k++)
	      {
		GetValue(i,posx[k],posy[k],posz[k],val);


		if(component == -1)
		  {
		    for(int j=0; j<components; j++)
		      {
			pos = domain*components+j;
			if(val[j] > maxima[pos]) maxima[pos] = val[j];
			if(val[j] < minima[pos]) minima[pos] = val[j];
			if(k==0) averages_times_volumes[pos] += val[j]*vol;
		      }
		  }
		else
		  {
		    pos = domain;
		    if(val[component] > maxima[pos]) maxima[pos] = val[component];
		    if(val[component] < minima[pos]) minima[pos] = val[component];
		    if(k==0) averages_times_volumes[pos] += val[component]*vol;
		  }
	      }
	    lh2.CleanUp(heapp);
	  }
      }
    else if (bfi2d.Size())
      {
	for(int i=0; i<ma.GetNSE(); i++)
	  {
	    const FiniteElement & fel = fes.GetSFE(i,lh2);

	    domain = ma.GetSElIndex(i);

	    vol = ma.SurfaceElementVolume(i);

	    volumes[domain] += vol;

	    if(fel.ElementType() != cache_type)
	      {
		posx.DeleteAll(); posy.DeleteAll(); posz.DeleteAll(); 
		switch(fel.ElementType())
		  {
		  case ET_POINT:  // ??
		  case ET_SEGM: 
		  case ET_TET: case ET_HEX: case ET_PRISM: case ET_PYRAMID:
		    break;
		    
		  case ET_TRIG:
		    posx.Append(0.33); posy.Append(0.33);
		    posx.Append(0); posy.Append(0);
		    posx.Append(0); posy.Append(1);
		    posx.Append(1); posy.Append(0);
		    break;
		  case ET_QUAD:
		    posx.Append(0.5); posy.Append(0.5);
		    posx.Append(0); posy.Append(0);
		    posx.Append(0); posy.Append(1); 
		    posx.Append(1); posy.Append(0);
		    posx.Append(1); posy.Append(1);
		    break;
		  }
		cache_type = fel.ElementType();
	      }
	    for(int k=0; k<posx.Size(); k++)
	      {
		GetSurfValue(i,-1, posx[k],posy[k],val);
		if(component == -1)
		  {
		    for(int j=0; j<components; j++)
		      {
			pos = domain*components+j;
			if(val[j] > maxima[pos]) maxima[pos] = val[j];
			if(val[j] < minima[pos]) minima[pos] = val[j];
			if(k==0) averages_times_volumes[pos] += val[j]*vol;
		      }
		  }
		else
		  {
		    pos = domain;
		    if(val[component] > maxima[pos]) maxima[pos] = val[component];
		    if(val[component] < minima[pos]) minima[pos] = val[component];
		    if(k==0) averages_times_volumes[pos] += val[component]*vol;
		  }
	      }
	    lh2.CleanUp(heapp);
	  }
      }

    delete [] val;
  }







  VisualizeCoefficientFunction :: 
  VisualizeCoefficientFunction (const MeshAccess & ama,
				const CoefficientFunction * acf)
    : SolutionData ("coef", acf->Dimension(), false /* complex */),
      ma(ama), cf(acf)
  { ; }
  
  VisualizeCoefficientFunction :: ~VisualizeCoefficientFunction ()
  {
    ;
  }
  
  bool VisualizeCoefficientFunction :: GetValue (int elnr, 
						 double lam1, double lam2, double lam3,
						 double * values) 
  {
    LocalHeapMem<100000> lh("viscf::GetValue");
    IntegrationPoint ip(lam1, lam2, lam3);
    ElementTransformation & trafo = ma.GetTrafo (elnr, VOL, lh);
    BaseMappedIntegrationPoint & mip = trafo(ip, lh);
    if (!cf -> IsComplex())
      cf -> Evaluate (mip, FlatVector<>(GetComponents(), values));
    else
      cf -> Evaluate (mip, FlatVector<Complex>(GetComponents(), values));
    return true; 
  }
  
  bool VisualizeCoefficientFunction :: 
  GetValue (int elnr, 
	    const double xref[], const double x[], const double dxdxref[],
	    double * values) 
  {
    LocalHeapMem<100000> lh("viscf::GetValue xref");
    IntegrationPoint ip(xref[0],xref[1],xref[2]);
    ElementTransformation & trafo = ma.GetTrafo (elnr, VOL, lh);
    BaseMappedIntegrationPoint & mip = trafo(ip, lh);
    if (!cf -> IsComplex())
      cf -> Evaluate (mip, FlatVector<>(GetComponents(), values));
    else
      cf -> Evaluate (mip, FlatVector<Complex>(GetComponents(), values));      
    return true; 
  }

  bool VisualizeCoefficientFunction :: 
  GetMultiValue (int elnr, int npts,
		 const double * xref, int sxref,
		 const double * x, int sx,
		 const double * dxdxref, int sdxdxref,
		 double * values, int svalues)
  {
    cout << "visualizecoef, GetMultiValue not implemented" << endl;
    return false;
  }
  
  bool VisualizeCoefficientFunction ::  
  GetSurfValue (int elnr, int facetnr,
		double lam1, double lam2, 
		double * values) 
  {
    LocalHeapMem<100000> lh("viscf::GetSurfValue");
    IntegrationPoint ip(lam1, lam2);
    ip.FacetNr() = facetnr;
    bool bound = ma.GetDimension() == 3;
    ElementTransformation & trafo = ma.GetTrafo (elnr, bound, lh);
    BaseMappedIntegrationPoint & mip = trafo(ip, lh);
    if (!cf -> IsComplex())
      cf -> Evaluate (mip, FlatVector<>(GetComponents(), values));
    else
      cf -> Evaluate (mip, FlatVector<Complex>(GetComponents(), values));
    return true; 
  }

  bool VisualizeCoefficientFunction ::  GetSurfValue (int selnr, int facetnr, 
						      const double xref[], const double x[], const double dxdxref[],
						      double * values)
  {
    cout << "visualizecoef, getsurfvalue (xref) not implemented" << endl;
    return false;
  }

  bool VisualizeCoefficientFunction ::  
  GetMultiSurfValue (int selnr, int facetnr, int npts,
		     const double * xref, int sxref,
		     const double * x, int sx,
		     const double * dxdxref, int sdxdxref,
		     double * values, int svalues)
  {
    for (int i = 0; i < npts; i++)
      GetSurfValue (selnr, facetnr, xref[i*sxref], xref[i*sxref+1], &values[i*svalues]);
    return true;
  }


  void VisualizeCoefficientFunction ::  
  Analyze(Array<double> & minima, Array<double> & maxima, Array<double> & averages, 
	  int component)
  {
    cout << "visualizecoef, analyze1 not implemented" << endl;
  }

  void VisualizeCoefficientFunction :: 
  Analyze(Array<double> & minima, Array<double> & maxima, Array<double> & averages_times_volumes, 
	  Array<double> & volumes, int component)
  {
    cout << "visualizecoef, analyzed2 not implemented" << endl;
  }










  template class T_GridFunction<double>;
  template class T_GridFunction<Vec<2> >;
  template class T_GridFunction<Vec<3> >;
  template class T_GridFunction<Vec<4> >;
  

  template class  VisualizeGridFunction<double>;
  template class  VisualizeGridFunction<Complex>;

}
