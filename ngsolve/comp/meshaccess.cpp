/*********************************************************************/
/* File:   meshaccess.cpp                                            */
/* Author: Joachim Schoeberl                                         */
/* Date:   25. Mar. 2000                                             */
/*********************************************************************/

/* 
   Access to fe mesh
*/


#include <comp.hpp>
// #include <parallelngs.hpp>
#include <../fem/h1lofe.hpp>





namespace ngcomp
{

  template <int DIMS, int DIMR>
  class NGS_DLL_HEADER Ng_ElementTransformation : public ElementTransformation
  {

  public:

    virtual void SetElement (bool aboundary, int aelnr, int aelindex)
    {
      // boundary = aboundary;
      elnr = aelnr;
      elindex = aelindex;
      // dim = Ng_GetDimension();
      if (DIMS < DIMR)
	iscurved = Ng_IsSurfaceElementCurved (elnr+1);
      else
	iscurved = Ng_IsElementCurved (elnr+1);
    }

    virtual int SpaceDim () const
    {
      return DIMR;
    }
    
    ///
    virtual bool Boundary() const
    {
      return DIMS < DIMR;
    }


    virtual void GetSort (FlatArray<int> sort) const
    {
      int vnums[12];
      if (DIMS < DIMR)
	Ng_GetSurfaceElement (elnr+1, vnums);
      else
	{
	  Ng_Element nel = netgen::Ng_GetElement<DIMS> (elnr);
	  for (int j = 0; j  < nel.vertices.Size(); j++)
	    vnums[j] = nel.vertices[j];
	  // Ng_GetElement (elnr+1, &vnums[0], &np);
	}
    
    switch (eltype)
      {
      case ET_TRIG:
	for (int i = 0; i < 3; i++) sort[i] = i;
	if (vnums[sort[0]] > vnums[sort[1]]) Swap (sort[0], sort[1]);
	if (vnums[sort[1]] > vnums[sort[2]]) Swap (sort[1], sort[2]);
	if (vnums[sort[0]] > vnums[sort[1]]) Swap (sort[0], sort[1]);
	// vnums[sort[0]] < vnums[sort[1]] < vnums[sort[2]]
	break; 

      case ET_TET:
	for (int i = 0; i < 4; i++) sort[i] = i;
	if (vnums[sort[0]] > vnums[sort[1]]) Swap (sort[0], sort[1]);
	if (vnums[sort[2]] > vnums[sort[3]]) Swap (sort[2], sort[3]);
	if (vnums[sort[0]] > vnums[sort[2]]) Swap (sort[0], sort[2]);
	if (vnums[sort[1]] > vnums[sort[3]]) Swap (sort[1], sort[3]);
	if (vnums[sort[1]] > vnums[sort[2]]) Swap (sort[1], sort[2]);

	// vnums[sort[0]] < vnums[sort[1]] < vnums[sort[2]] < vnums[sort[3]]
	break; 

      case ET_PRISM:
	for (int i = 0; i < 6; i++) sort[i] = i;

	if (vnums[sort[0]] > vnums[sort[1]]) Swap (sort[0], sort[1]);
	if (vnums[sort[1]] > vnums[sort[2]]) Swap (sort[1], sort[2]);
	if (vnums[sort[0]] > vnums[sort[1]]) Swap (sort[0], sort[1]);
        
	if (vnums[sort[3]] > vnums[sort[4]]) Swap (sort[3], sort[4]);
	if (vnums[sort[4]] > vnums[sort[5]]) Swap (sort[4], sort[5]);
	if (vnums[sort[3]] > vnums[sort[4]]) Swap (sort[3], sort[4]);
	break;

      default:
	throw Exception ("undefined eltype in ElementTransformation::GetSort()\n");
      }
      
    }




    virtual void CalcJacobian (const IntegrationPoint & ip,
			       FlatMatrix<> dxdxi) const
    {
      if (DIMS < DIMR)
	Ng_GetSurfaceElementTransformation (elnr+1, &ip(0), 0, &dxdxi(0,0));
      else
	Ng_GetElementTransformation (elnr+1, &ip(0), 0, &dxdxi(0,0));
    }
    
    virtual void CalcPoint (const IntegrationPoint & ip,
			    FlatVector<> point) const
    {
      if (DIMS < DIMR)
	Ng_GetSurfaceElementTransformation (elnr+1, &ip(0), &point(0), 0);
      else
	Ng_GetElementTransformation (elnr+1, &ip(0), &point(0), 0);
    }

    virtual void CalcPointJacobian (const IntegrationPoint & ip,
				    FlatVector<> point, FlatMatrix<> dxdxi) const
    {
      if (DIMS < DIMR)
	Ng_GetSurfaceElementTransformation (elnr+1, &ip(0), &point(0), &dxdxi(0,0));
      else
	Ng_GetElementTransformation (elnr+1, &ip(0), &point(0), &dxdxi(0,0));
    }

    virtual BaseMappedIntegrationPoint & operator() (const IntegrationPoint & ip, LocalHeap & lh) const
    {
      return *new (lh) MappedIntegrationPoint<DIMS,DIMR> (ip, *this);
    }

    virtual BaseMappedIntegrationRule & operator() (const IntegrationRule & ir, LocalHeap & lh) const
    {
      return *new (lh) MappedIntegrationRule<DIMS,DIMR> (ir, *this, lh);
    }    

    virtual void CalcMultiPointJacobian (const IntegrationRule & ir,
					 BaseMappedIntegrationRule & bmir) const
    {
      MappedIntegrationRule<DIMS,DIMR> & mir = static_cast<MappedIntegrationRule<DIMS,DIMR> &> (bmir);
      netgen::Ng_MultiElementTransformation <DIMS,DIMR> (elnr, ir.Size(),
							 &ir[0](0), &ir[1](0)-&ir[0](0),
							 &mir[0].Point()(0), 
							 &mir[1].Point()(0)-&mir[0].Point()(0), 
							 &mir[0].Jacobian()(0,0), 
							 &mir[1].Jacobian()(0,0)-&mir[0].Jacobian()(0,0));

      for (int i = 0; i < ir.Size(); i++)
	mir[i].Compute();
    }
  };
  












  MeshAccess :: MeshAccess ()
  {
    ngstd::testout = netgen::testout;
    Ng_UpdateTopology();  // for netgen/ngsolve stand alone (pillendose)
    UpdateBuffers();
  }


  MeshAccess :: ~MeshAccess ()
  {
    ;
  }

  void MeshAccess :: UpdateBuffers()
  {
    dim = Ng_GetDimension();
    if (dim == -1)
      for (int i = 0; i < 4; i++)  
        {
          nnodes[i] = 0;
          nelements[i] = 0;
	  nnodes_cd[i] = 0;
	  nelements_cd[i] = 0;
        }
    else
      {
	for (int i = 0; i < 4; i++)  
	  {
	    nnodes[i] = Ng_GetNNodes(i);
	    nelements[i] = Ng_GetNElements(i);
	  }
	for (int i = 0; i <= dim; i++)
	  {
	    nnodes_cd[i] = nnodes[dim-i];
	    nelements_cd[i] = nelements[dim-i];
	  }
      }

    nlevels = Ng_GetNLevels(); 


    ndomains = -1;
    int ne = GetNE(); 
    for (int i = 0; i < ne; i++)
      { 
	int ind = GetElIndex(i);
	if (ind > ndomains) ndomains = ind;
      }
    ndomains++;

    nboundaries = -1;
    int nse = GetNSE(); 
    for (int i = 0; i < nse; i++)
      { 
	int ind = GetSElIndex(i);
	if (ind > nboundaries) nboundaries = ind;
      }
    nboundaries++;
  }


  void MeshAccess::GetTopologicElement (int elnr, TopologicElement & topel) const
  {
    int help[54];
    int nnodes;

    nnodes = Ng_GetElementClosureNodes (dim, elnr, 
                                        NodeSet (NT_VERTEX, NT_EDGE, NT_FACE, NT_CELL), help);
        
    topel.SetElementType (GetElType(elnr));
    topel.Clear();
    for (int i = 0; i < nnodes; i++)
      topel.AddNode (Node (NODE_TYPE (help[2*i]), help[2*i+1]));
    
    /*    
    // 2D case
    ArrayMem<int, 12> nums;
    
    topel.SetElementType (GetElType(elnr));
    topel.Clear();

    GetElVertices (elnr, nums);
    for (int j = 0; j < nums.Size(); j++)
      topel.AddNode (Node (NT_VERTEX, nums[j]));

    GetElEdges (elnr, nums);
    for (int j = 0; j < nums.Size(); j++)
      topel.AddNode (Node (NT_EDGE, nums[j]));

    if (GetDimension() == 3)
      {
        GetElFaces (elnr, nums);
        for (int j = 0; j < nums.Size(); j++)
          topel.AddNode (Node (NT_FACE, nums[j]));
        
        topel.AddNode (Node (NT_CELL, elnr));
      }
    else
      topel.AddNode (Node (NT_FACE, elnr));
    */
  }




  int MeshAccess :: GetNDomains () const
  {
    return ndomains;
    /*
    int maxind = -1;
    int ne = GetNE(); 
    for (int i = 0; i < ne; i++)
      { 
	int ind = GetElIndex(i);
	if (ind > maxind) maxind = ind;
      }
    return maxind+1;
    */
  }


  int MeshAccess :: GetNBoundaries () const
  {
    return nboundaries;
    /*
    int maxind = -1;
    int nse = GetNSE(); 
    for (int i = 0; i < nse; i++)
      { 
	int ind = GetSElIndex(i);
	if (ind > maxind) maxind = ind;
      }
    
    return maxind+1;
    */
  }


  
  ELEMENT_TYPE MeshAccess :: GetElType (int elnr) const
  {
    return ConvertElementType (GetElement(elnr).GetType());
    /*
    switch (GetElement(elnr).GetType())
      {
      case NG_TRIG:
      case NG_TRIG6:
        {
          return ET_TRIG;
          break;
        }

    case NG_QUAD:
    case NG_QUAD6:
      {
	return ET_QUAD;
	break;
      }

    case NG_TET:
    case NG_TET10:
      {
	return ET_TET;
	break;
      }
    case NG_PRISM:
    case NG_PRISM12:
      {
	return ET_PRISM;
	break;
      }
    case NG_PYRAMID:
      {
	return ET_PYRAMID;
	break;
      }
    case NG_HEX:
      {
	return ET_HEX;
	break;
      }

    default:
      {
	cerr << "MeshAccess::GetElType undefined type "
	     << int(GetElement(elnr).GetType()) << endl;
      }
    }
  return ET_TET;
    */
  }


ELEMENT_TYPE MeshAccess :: GetSElType (int elnr) const
{
  return ConvertElementType (GetSElement(elnr).GetType());

  /*
  switch (GetSElement(elnr).GetType())
    {
    case NG_SEGM:
    case NG_SEGM3:
      {
	return ET_SEGM;
	break;
      }

    case NG_TRIG:
    case NG_TRIG6:
      {
	return ET_TRIG;
	break;
      }

    case NG_QUAD:
    case NG_QUAD6:
      {
	return ET_QUAD;
	break;
      }
    default:
      {
	throw Exception ("GetSElType: Unhandled element type");
      }
    }


  stringstream str;
  str << "MeshAccess::GetSElType undefined type "
      << int(ngel.GetType()) << endl;
  throw Exception (str.str());
  */
}


  
  // von astrid
  int MeshAccess :: GetElNV ( int elnr ) const 
  {
    return GetElement(elnr).vertices.Size();
  } 




void MeshAccess :: GetElPNums (int elnr, Array<int> & pnums) const
{
  pnums = ArrayObject (GetElement(elnr).points);
}


void MeshAccess :: GetSegmentPNums (int snr, Array<int> & pnums) const
{
  pnums.SetSize(3);
  int np;
  Ng_GetSegment (snr+1, &pnums[0], &np);
  pnums.SetSize(np);

  for (int i = 0; i < np; i++)
    pnums[i]--;
}


int MeshAccess :: GetSegmentIndex (int snr) const
{
  return Ng_GetSegmentIndex(snr+1);
}

  /*
void MeshAccess :: GetElVertices (int elnr, Array<int> & vnums) const
{
  vnums = ArrayObject (GetElement(elnr).vertices);
}
  */
  
void MeshAccess :: 
GetElEdges (int elnr, Array<int> & ednums, Array<int> & orient) const
{
  ednums.SetSize (12);
  orient.SetSize (12);
  int ned = 
    Ng_GetElement_Edges (elnr+1, &ednums[0], &orient[0]);
  ednums.SetSize (ned);
  orient.SetSize (ned);
  for (int i = 0; i < ned; i++)
    ednums[i]--;
}

  /*
void MeshAccess :: 
GetElEdges (int elnr, Array<int> & ednums) const
{
  ednums = ArrayObject (GetElement(elnr).edges);
}
  */
void MeshAccess :: 
GetSElEdges (int elnr, Array<int> & ednums) const
{
  ednums = ArrayObject (GetSElement(elnr).edges);
}

void MeshAccess :: 
GetSElEdges (int selnr, Array<int> & ednums, Array<int> & orient) const
{
  ednums.SetSize (4);
  orient.SetSize (4);
  int ned = 
    Ng_GetSurfaceElement_Edges (selnr+1, &ednums[0], &orient[0]);
  ednums.SetSize (ned);
  orient.SetSize (ned);

  for (int i = 0; i < ned; i++)
    ednums[i]--;
}



void MeshAccess :: GetEdgeElements (int enr, Array<int> & elnums) const
{
  // old routine: Slow !!!
  /* 
  int nel = GetNE();
  int edges[8];
  elnums.SetSize (0);
  for (int i = 0; i < nel; i++)
    {
      int ned = Ng_GetElement_Edges (i+1, edges, 0);
      for (int j = 0; j < ned; j++)
	if (edges[j]-1 == enr)
	  elnums.Append (i);
    }
  */ 
  
  // fast (he)
  elnums.SetSize(0);
  Array<int> pnums, velems0, velems1; 
  GetEdgePNums(enr, pnums);
  GetVertexElements(pnums[0], velems0);
  GetVertexElements(pnums[1], velems1);
  
  //   now compare
  for (int i=0; i<velems0.Size(); i++) {
    for (int j=0; j<velems1.Size(); j++) {
      if (velems0[i] == velems1[j]) {
        //  hope that this is a fine edge !! 
        // (run several tests and compared with old routine. seems ok)
        elnums.Append(velems0[i]);
        continue;
      }
    }
  }
  // fast function should go into Netgen - part (JS)
}
  
  
void MeshAccess :: 
GetElFaces (int elnr, Array<int> & fnums) const
{
  fnums = ArrayObject (GetElement(elnr).faces);
}

void MeshAccess :: 
GetElFaces (int elnr, Array<int> & fnums, Array<int> & orient) const
{
  fnums.SetSize (6);
  orient.SetSize (6);
  int nfa = 
    Ng_GetElement_Faces (elnr+1, &fnums[0], &orient[0]);
  fnums.SetSize (nfa);
  orient.SetSize (nfa);

  for (int i = 0; i < nfa; i++)
    fnums[i]--;
}


int MeshAccess :: 
GetSElFace (int selnr) const
{
  return Ng_GetSurfaceElement_Face (selnr+1, 0)-1;
}
  
void MeshAccess :: 
GetSElFace (int selnr, int & fnum, int & orient) const
{
  fnum = Ng_GetSurfaceElement_Face (selnr+1, &orient);
  fnum--;
}

// string MeshAccess :: GetSElBCName (const int elnr) const
//     {
// 	char auxstr[256];
// 	string retval;
// 	Ng_GetSurfaceElementBCName (elnr+1,auxstr);
// 	retval = auxstr;
// 	return retval;
//     }

// string MeshAccess :: GetBCNumBCName (const int bcnr) const
//     {
// 	char auxstr[256];
// 	string retval;
// 	Ng_GetBCNumBCName(bcnr,auxstr);
// 	retval = auxstr;
// 	return retval;
//     }
  
void MeshAccess :: GetFacePNums (int fnr, Array<int> & pnums) const
{
  pnums.SetSize(4);
  int nv = Ng_GetFace_Vertices (fnr+1, &pnums[0]);
  pnums.SetSize(nv);
  for (int i = 0; i < nv; i++) 
    pnums[i]--;
}

 
void MeshAccess :: GetFaceEdges (int fnr, Array<int> & edges) const
{
  edges.SetSize(4);
  int ned = Ng_GetFace_Edges (fnr+1, &edges[0]);
  edges.SetSize(ned);
  for (int i = 0; i < ned; i++) 
    edges[i]--;
}
 

void MeshAccess :: GetFaceElements (int fnr, Array<int> & elnums) const
{
  /*
  int nel = GetNE();
  int faces[8];
  elnums.SetSize (0);
  for (int i = 0; i < nel; i++)
    {
      int nfa = Ng_GetElement_Faces (i+1, faces, 0);
      for (int j = 0; j < nfa; j++)
	if (faces[j]-1 == fnr)
	  elnums.Append (i);
    }
  */
  ArrayMem<int, 9> vnums;
  GetFacePNums(fnr, vnums);

  ArrayMem<int, 50> vels;
  GetVertexElements (vnums[0], vels);

  int faces[8];
  elnums.SetSize (0);
  for (int i = 0; i < vels.Size(); i++)
    {
      int nfa = Ng_GetElement_Faces (vels[i]+1, faces, 0);
      for (int j = 0; j < nfa; j++)
	if (faces[j]-1 == fnr)
	  elnums.Append (vels[i]);
    }
}


void MeshAccess :: GetEdgePNums (int fnr, int & pn1, int & pn2) const
{
  int v2[2];
  Ng_GetEdge_Vertices (fnr+1, v2);
  pn1 = v2[0]-1;
  pn2 = v2[1]-1;
}

void MeshAccess :: GetEdgePNums (int fnr, Array<int> & pnums) const
{
  pnums.SetSize(2);
  Ng_GetEdge_Vertices (fnr+1, &pnums[0]);
  pnums[0] -= 1;
  pnums[1] -= 1;
}

  
void MeshAccess :: GetSElPNums (int selnr, Array<int> & pnums) const
{
  Ng_Element ngel = (dim == 2) 
    ? Ng_GetElement<1> (selnr)
    : Ng_GetElement<2> (selnr);
    
  pnums.SetSize(ngel.points.Size());
  for (int j = 0; j < ngel.points.Size(); j++)
    pnums[j] = ngel.points[j];  

  /*
  pnums.SetSize (NG_SURFACE_ELEMENT_MAXPOINTS);
  NG_ELEMENT_TYPE typ = Ng_GetSurfaceElement (selnr+1, &pnums[0]);
  switch (typ)
    {
    case NG_SEGM: pnums.SetSize(2); break;
    case NG_SEGM3: pnums.SetSize(3); break;

    case NG_TRIG: pnums.SetSize(3); break;
    case NG_TRIG6: pnums.SetSize(6); break;
    case NG_QUAD: pnums.SetSize(4); break;
    case NG_QUAD6: pnums.SetSize(6); break;

    default:
      {
	cerr << "MeshAccess::GetSElPNums undefined type "
	     << int(Ng_GetSurfaceElement (selnr, &pnums[0])) << endl;
      }

    }
  for (int i = 0; i < pnums.Size(); i++) pnums[i]--;
  */
}


void MeshAccess :: GetSElVertices (int selnr, Array<int> & vnums) const
{
  vnums = ArrayObject (GetSElement(selnr).vertices);
}



// some utility for Facets
void MeshAccess :: GetElFacets (int elnr, Array<int> & fnums) const
{
  if (dim == 2)
    {
      Ng_Element ngel = Ng_GetElement<2> (elnr);
      fnums.SetSize(ngel.edges.Size());
      for (int j = 0; j < ngel.edges.Size(); j++)
	fnums[j] = ngel.edges[j];
    }
  else
    {
      Ng_Element ngel = Ng_GetElement<3> (elnr);
      fnums.SetSize(ngel.faces.Size());
      for (int j = 0; j < ngel.faces.Size(); j++)
	fnums[j] = ngel.faces[j];
    }
      
  /*
  if (GetDimension() == 2) GetElEdges(elnr, fnums);
  else GetElFaces(elnr, fnums);
  */
} 
    
void MeshAccess :: GetSElFacets (int selnr, Array<int> & fnums) const
{
  if (GetDimension() == 2) 
    GetSElEdges(selnr, fnums);
  else
  {
    fnums.SetSize(1);
    fnums[0] = GetSElFace(selnr);
  }
}

void MeshAccess :: GetFacetPNums (int fnr, Array<int> & pnums) const
{
  if (GetDimension() == 2)
    GetEdgePNums(fnr, pnums);
  else
    GetFacePNums(fnr, pnums);
}

ELEMENT_TYPE MeshAccess :: GetFacetType (int fnr) const
{
  if (GetDimension() == 2)
    return ET_SEGM;
  ArrayMem<int, 4> pnums;
  GetFacePNums(fnr, pnums);
  return (pnums.Size() == 3) ? ET_TRIG : ET_QUAD;
}






void MeshAccess::PushStatus (const char * str) const
{ 
  Ng_PushStatus (str);
}
void MeshAccess::PopStatus () const
{ 
  Ng_PopStatus ();
}
void MeshAccess::SetThreadPercentage (double percent) const
{ 
  Ng_SetThreadPercentage (percent); 
}
void MeshAccess :: GetStatus (string & str, double & percent) const
{
  char * s;
  Ng_GetStatus(&s,percent);
  str = s;
}



void MeshAccess :: SetTerminate(void) const
{
  Ng_SetTerminate();
}
void MeshAccess :: UnSetTerminate(void) const
{
  Ng_UnSetTerminate();
}
bool MeshAccess :: ShouldTerminate(void) const
{
  return (Ng_ShouldTerminate() != 0);
}


void MeshAccess :: GetVertexElements (int vnr, Array<int> & elnrs) const
{
  int nel = Ng_GetNVertexElements (vnr+1);
  elnrs.SetSize (nel);
  Ng_GetVertexElements (vnr+1, &elnrs[0]);
  for (int j = 0; j < nel; j++)
    elnrs[j]--;
}






  

void MeshAccess ::
GetElementTransformation (int elnr, ElementTransformation & eltrans) const
{
  delete eltrans.specific;
  if (GetDimension() == 2)
    eltrans.specific = new Ng_ElementTransformation<2,2> ();
  else
    eltrans.specific = new Ng_ElementTransformation<3,3> ();


  int elind = Ng_GetElementIndex (elnr+1)-1;
  eltrans.SetElement (0, elnr, elind);
  eltrans.SetElementType (GetElType(elnr));

  if(higher_integration_order.Size() == GetNE() && higher_integration_order[elnr])
    eltrans.SetHigherIntegrationOrder();
  else
    eltrans.UnSetHigherIntegrationOrder();

  eltrans.specific->SetElement (0, elnr, elind);
  eltrans.specific->SetElementType (GetElType(elnr));
}



void MeshAccess ::
GetSurfaceElementTransformation (int elnr, ElementTransformation & eltrans) const
{
  delete eltrans.specific;
  if (GetDimension() == 2)
    eltrans.specific = new Ng_ElementTransformation<1,2> ();
  else
    eltrans.specific = new Ng_ElementTransformation<2,3> ();


  int elind = Ng_GetSurfaceElementIndex (elnr+1)-1;
  eltrans.SetElement (1, elnr, elind);
  eltrans.SetElementType (GetSElType(elnr));


  eltrans.specific->SetElement (1, elnr, elind);
  eltrans.specific->SetElementType (GetSElType(elnr));
}    


ElementTransformation & MeshAccess :: GetTrafo (int elnr, bool boundary, LocalHeap & lh) const

{
  ElementTransformation * eltrans;  

  if (!boundary)
    {
      if (GetDimension() == 2)
	eltrans = new (lh) Ng_ElementTransformation<2,2> ();
      else
	eltrans = new (lh) Ng_ElementTransformation<3,3> ();
	
      int elind = Ng_GetElementIndex (elnr+1)-1;
      eltrans->SetElement (0, elnr, elind);
      eltrans->SetElementType (GetElType(elnr));
      
      if(higher_integration_order.Size() == GetNE() && higher_integration_order[elnr])
	eltrans->SetHigherIntegrationOrder();
      else
	eltrans->UnSetHigherIntegrationOrder();
    }
  else
    {
      if (GetDimension() == 2) 
	eltrans = new (lh) Ng_ElementTransformation<1,2> ();
      else
	eltrans = new (lh) Ng_ElementTransformation<2,3> ();
      
      int elind = Ng_GetSurfaceElementIndex (elnr+1)-1;
      eltrans->SetElement (1, elnr, elind);
      eltrans->SetElementType (GetSElType(elnr));
    }

  return *eltrans;
}








  
double MeshAccess :: ElementVolume (int elnr) const
{
  static FE_Trig0 trig0;
  static FE_Quad0 quad0;
  static FE_Tet0 tet0;
  static FE_Prism0 prism0;
  static FE_Pyramid0 pyramid0;
  static FE_Hex0 hex0;
  
  const FiniteElement * fe = NULL;
  switch (GetElType (elnr))
    {
    case ET_TRIG: fe = &trig0; break;
    case ET_QUAD: fe = &quad0; break;
    case ET_TET: fe = &tet0; break;
    case ET_PYRAMID: fe = &pyramid0; break;
    case ET_PRISM: fe = &prism0; break;
    case ET_HEX: fe = &hex0; break;
    default:
      {
	cerr << "ElementVolume not implemented for el " << GetElType(elnr) << endl;
      }
    }
  
  char d[10000];
  LocalHeap lh(d, 10000, "MeshAccess - elementvolume");

  ElementTransformation trans;
  GetElementTransformation (elnr, trans, lh);
  ConstantCoefficientFunction ccf(1);


  if (GetDimension() == 2)
    {
      SourceIntegrator<2> si( &ccf );
      FlatVector<> elvec(fe->GetNDof(), lh);
      si.CalcElementVector (*fe, trans, elvec, lh);
      return elvec(0);
    }
  else
    {
      SourceIntegrator<3> si( &ccf );
      FlatVector<> elvec(fe->GetNDof(), lh);
      si.CalcElementVector (*fe, trans, elvec, lh);
      return elvec(0);
    }
}


  
double MeshAccess :: SurfaceElementVolume (int selnr) const
{
  static FE_Trig0 trig0;
  static FE_Quad0 quad0;

  const FiniteElement * fe;
  switch (GetSElType (selnr))
    {
    case ET_TRIG: fe = &trig0; break;
    case ET_QUAD: fe = &quad0; break;
    default:
      {
	cerr << "SurfaceElementVolume not implemented for el " << GetElType(selnr) << endl;
	return 0;
      }
    }

  char d[10000];
  LocalHeap lh(d, 10000, "MeshAccess - surfaceelementvolume");

  ElementTransformation trans;
  GetSurfaceElementTransformation (selnr, trans, lh);
  ConstantCoefficientFunction ccf(1);

  if (GetDimension() == 2)
    {
      NeumannIntegrator<2> si( &ccf );
      FlatVector<> elvec (fe->GetNDof(), lh);
      si.CalcElementVector (*fe, trans, elvec, lh);
      return elvec(0);
    }
  else
    {
      NeumannIntegrator<3> si( &ccf );
      FlatVector<> elvec (fe->GetNDof(), lh);
      si.CalcElementVector (*fe, trans, elvec, lh);
      return elvec(0);
    }
}

/*
int MeshAccess :: GetNLevels() const
{
  return Ng_GetNLevels();
}
*/



void MeshAccess :: SetPointSearchStartElement(const int el) const
{
  Ng_SetPointSearchStartElement(el+1);
}

int MeshAccess :: FindElementOfPoint (FlatVector<double> point,
				      IntegrationPoint & ip, 
				      bool build_searchtree,
				      const int index) const
{
  Array<int> dummy(1);
  dummy[0] = index;
  return FindElementOfPoint(point,ip,build_searchtree,&dummy);
}

int MeshAccess :: FindElementOfPoint (FlatVector<double> point,
				      IntegrationPoint & ip,
				      bool build_searchtree,
				      const Array<int> * const indices) const
{
  int elnr;
  double lami[3];
  if(indices != NULL && indices->Size()>0)
    {
      int * dummy = new int[indices->Size()];
      for(int i=0; i<indices->Size(); i++) dummy[i] = (*indices)[i]+1;

      elnr = Ng_FindElementOfPoint (&point(0), lami, build_searchtree,dummy,indices->Size());

      delete [] dummy;
    }
  else
    {  
      elnr = Ng_FindElementOfPoint (&point(0), lami, build_searchtree);
    }

  if (elnr == 0) return -1;

  for (int k = 0; k < 3; k++)
    ip(k) = lami[k];

  return elnr-1;
}


int MeshAccess :: FindSurfaceElementOfPoint (FlatVector<double> point,
					     IntegrationPoint & ip, 
					     bool build_searchtree,
					     const int index) const
{
  Array<int> dummy(1);
  dummy[0] = index;
  return FindSurfaceElementOfPoint(point,ip,build_searchtree,&dummy);
}

int MeshAccess :: FindSurfaceElementOfPoint (FlatVector<double> point,
					     IntegrationPoint & ip,
					     bool build_searchtree,
					     const Array<int> * const indices) const
{
  int elnr;
  double lami[3];
  if(indices != NULL && indices->Size()>0)
    {
      int * dummy = new int[indices->Size()];
      for(int i=0; i<indices->Size(); i++) dummy[i] = (*indices)[i]+1;

      elnr = Ng_FindSurfaceElementOfPoint (&point(0), lami, build_searchtree,dummy,indices->Size());

      delete [] dummy;
    }
  else
    {  
      elnr = Ng_FindSurfaceElementOfPoint (&point(0), lami, build_searchtree);
    }

  if (elnr == 0) return -1;

  for (int k = 0; k < 3; k++)
    ip(k) = lami[k];


  return elnr-1;
}


int MeshAccess :: GetNPairsPeriodicVertices () const 
{
  return Ng_GetNPeriodicVertices(0);
}

int MeshAccess :: GetNPairsPeriodicVertices (int idnr) const 
{
  return Ng_GetNPeriodicVertices(idnr);
}
 
void MeshAccess :: GetPeriodicVertices ( Array<INT<2> > & pairs) const
{
  int npairs;

  npairs = Ng_GetNPeriodicVertices (0);
  pairs.SetSize (npairs);

  Ng_GetPeriodicVertices (0,&pairs[0][0]);
  for (int i = 0; i < pairs.Size(); i++)
    {
      pairs[i][0]--;
      pairs[i][1]--;
    }
}
 
void MeshAccess :: GetPeriodicVertices (int idnr, Array<INT<2> > & pairs) const
{
  int npairs;

  npairs = Ng_GetNPeriodicVertices (idnr);

  pairs.SetSize (npairs);

  Ng_GetPeriodicVertices (idnr,&pairs[0][0]);

  for (int i = 0; i < pairs.Size(); i++)
    {
      pairs[i][0]--;
      pairs[i][1]--;
    }
}




int MeshAccess :: GetNPairsPeriodicEdges () const 
{
  return Ng_GetNPeriodicEdges(0);
}
 
void MeshAccess :: GetPeriodicEdges ( Array<INT<2> > & pairs) const
{
  int npairs;

  npairs = Ng_GetNPeriodicEdges (0);
  pairs.SetSize (npairs);

  Ng_GetPeriodicEdges (0,&pairs[0][0]);
  for (int i = 0; i < pairs.Size(); i++)
    {
      pairs[i][0]--;
      pairs[i][1]--;
    }
}

int MeshAccess :: GetNPairsPeriodicEdges (int idnr) const 
{
  return Ng_GetNPeriodicEdges(idnr);
}
 
void MeshAccess :: GetPeriodicEdges (int idnr, Array<INT<2> > & pairs) const
{
  int npairs;

  npairs = Ng_GetNPeriodicEdges (idnr);
  pairs.SetSize (npairs);

  Ng_GetPeriodicEdges (idnr,&pairs[0][0]);
  for (int i = 0; i < pairs.Size(); i++)
    {
      pairs[i][0]--;
      pairs[i][1]--;
    }
}

/*
///// Added by Roman Stainko ....
void MeshAccess::GetVertexElements( int vnr, Array<int>& elems) const
{
  int nel = Ng_GetVertex_NElements( vnr+1 );
  elems.SetSize( nel );
  
  Ng_GetVertex_Elements( vnr+1, &elems[0] );
  
  for( int i=0; i<nel; i++ )
    elems[i]--;
 
}
*/

///// Added by Roman Stainko ....
void MeshAccess::GetVertexSurfaceElements( int vnr, Array<int>& elems) const
{
  int nel = Ng_GetVertex_NSurfaceElements( vnr+1 );
  elems.SetSize( nel );
  
  Ng_GetVertex_SurfaceElements( vnr+1, &elems[0] );

  for( int i=0; i<nel; i++ )
    elems[i]--;
}



void MeshAccess::SetHigherIntegrationOrder(int elnr)
{
  if(higher_integration_order.Size() != GetNE())
    {
      higher_integration_order.SetSize(GetNE());
      higher_integration_order = false;
    }
  higher_integration_order[elnr] = true;
}
void MeshAccess::UnSetHigherIntegrationOrder(int elnr)
{
  if(higher_integration_order.Size() != GetNE())
    {
      higher_integration_order.SetSize(GetNE());
      higher_integration_order = false;
    }
  higher_integration_order[elnr] = false;
}


void MeshAccess :: InitPointCurve(double red, double green, double blue) const
{
  Ng_InitPointCurve(red, green, blue);
}

void MeshAccess :: AddPointCurvePoint(const Vec<3> & point) const
{
  Ng_AddPointCurvePoint(&(point(0)));
}

}


#ifdef PARALLEL
 
namespace ngcomp
{
  int MeshAccess ::GetGlobalNodeNum (Node node) const
  {
    switch (node.GetType())
      {
      case NT_VERTEX: return NgPar_GetDistantPNum ( 0, node.GetNr() ); 
      case NT_EDGE: return NgPar_GetDistantEdgeNum ( 0, node.GetNr() ); 
      case NT_FACE: return NgPar_GetDistantFaceNum ( 0, node.GetNr() ); 
      case NT_CELL: return NgPar_GetDistantElNum ( 0, node.GetNr() ); 
      }
    return -1;
  }
  
  
  int MeshAccess :: GetDistantNodeNums (Node node,
					ngstd::Array<int[2]> & distnums ) const
  {
    distnums.SetSize( NgPar_GetNDistantNodeNums(node.GetType(), node.GetNr()) );
    return NgPar_GetDistantNodeNums ( node.GetType(), node.GetNr(), &distnums[0][0] );
  }
}

#else

namespace ngcomp
{
  int MeshAccess ::GetGlobalNodeNum (Node node) const
  {
    return -1;
  }

  int MeshAccess :: GetDistantNodeNums (Node node, 
					ngstd::Array<int[2]> & distnums ) const
  {
    distnums.SetSize (0);
    return 0;
  }
}

#endif






