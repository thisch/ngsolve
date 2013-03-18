#ifndef FILE_HCURLHOFE_
#define FILE_HCURLHOFE_  

/*********************************************************************/
/* File:   hcurlhofe.hpp                                             */
/* Author: Sabine Zaglmayr                                           */
/* Date:   20. Maerz 2003                                            */
/*                                                                   */
/* AutoCurl - revision: J. Schoeberl, March 2009                     */
/*********************************************************************/
   
namespace ngfem
{

/**
   High order H(curl) finite element of dimension D
 */
template <int D>
class HCurlHighOrderFiniteElement : public HCurlFiniteElement<D> 
{
protected:
  int vnums[8]; 
  int order_edge[12];
  INT<2> order_face[6];
  INT<3> order_cell;

  bool usegrad_edge[12]; 
  bool usegrad_face[6]; 
  bool usegrad_cell; 

  bool discontinuous;
  
public:
  // HCurlHighOrderFiniteElement (ELEMENT_TYPE aeltype);
  HCurlHighOrderFiniteElement () { discontinuous = false; }

  void SetVertexNumber (int nr, int vnum) { vnums[nr] = vnum; }
  void SetOrderEdge (int nr, int order) { order_edge[nr] = order; }
  void SetOrderFace (int nr, INT<2> order) { order_face[nr] = order; }

  void SetUseGradEdge(int nr, bool uge) { usegrad_edge[nr] = uge; }
  void SetUseGradFace(int nr, bool ugf) { usegrad_face[nr] = ugf; }

  /// assignes vertex numbers
  template <typename TA> 
  void SetVertexNumbers (const TA & avnums)
  { for (int i = 0; i < avnums.Size(); i++) vnums[i] = avnums[i]; }

  void SetOrderCell (INT<3> oi) { order_cell = oi; }

  /// set isotropic or anisotropic face orders
  template <typename TA>
  void SetOrderFace (const TA & of)
  { for (int i = 0; i < of.Size(); i++) order_face[i] = of[i]; }

  /// set edge orders
  template <typename TA>
  void SetOrderEdge (const TA & oe)
  { for (int i = 0; i < oe.Size(); i++) order_edge[i] = oe[i]; }

  /// use edge-gradients
  template <typename TA>
  void SetUseGradEdge (const TA & uge)
  { for (int i = 0; i < uge.Size(); i++) usegrad_edge[i] = uge[i]; }

  /// use face-gradients
  template <typename TA>
  void SetUseGradFace (const TA & ugf)
  { for (int i = 0; i < ugf.Size(); i++) usegrad_face[i] = ugf[i]; }

  void SetUseGradCell (bool ugc) 
  { usegrad_cell = ugc; }

  void SetDiscontinuous ( bool adiscont ) { discontinuous = adiscont; }
  virtual void ComputeNDof () = 0;
};



  /** 
      HCurlHighOrderFE of shape ET.
      The template specialization provides the shape functions.
  */
  template <ELEMENT_TYPE ET> class HCurlHighOrderFE;
  

  /**
     HCurlHighOrderFE of shape ET.
     provides access functions, shape funcitons are provided by CalcShape template
  */
  template <ELEMENT_TYPE ET, typename SHAPES = HCurlHighOrderFE<ET> >
class T_HCurlHighOrderFiniteElement 
  : public HCurlHighOrderFiniteElement<ET_trait<ET>::DIM>, public ET_trait<ET> 

{
protected:
  enum { DIM = ET_trait<ET>::DIM };
  
  using HCurlFiniteElement<DIM>::DIM_CURL;
  using HCurlFiniteElement<DIM>::ndof;
  using HCurlFiniteElement<DIM>::order;
  using HCurlFiniteElement<DIM>::eltype;
  // using HCurlFiniteElement<DIM>::dimspace;

  using HCurlHighOrderFiniteElement<DIM>::vnums;
  using HCurlHighOrderFiniteElement<DIM>::order_edge;
  using HCurlHighOrderFiniteElement<DIM>::order_face;
  using HCurlHighOrderFiniteElement<DIM>::order_cell;

  using HCurlHighOrderFiniteElement<DIM>::usegrad_edge;
  using HCurlHighOrderFiniteElement<DIM>::usegrad_face;
  using HCurlHighOrderFiniteElement<DIM>::usegrad_cell;

  using HCurlHighOrderFiniteElement<DIM>::discontinuous;


  using ET_trait<ET>::N_VERTEX;
  using ET_trait<ET>::N_EDGE;
  using ET_trait<ET>::N_FACE;
  using ET_trait<ET>::FaceType;
  using ET_trait<ET>::GetEdgeSort;
  using ET_trait<ET>::GetFaceSort;

  typedef IntegratedLegendreMonomialExt T_ORTHOPOL;

  // typedef TrigShapesInnerLegendre T_TRIGSHAPES;
  // typedef TrigShapesInnerJacobi T_TRIGSHAPES;

public:

  T_HCurlHighOrderFiniteElement () 
  {
    for (int i = 0; i < N_VERTEX; i++)
      vnums[i] = i;
    eltype = ET;
  }

  T_HCurlHighOrderFiniteElement (int aorder);

  virtual void ComputeNDof();

  virtual void CalcShape (const IntegrationPoint & ip, 
                          FlatMatrixFixWidth<DIM> shape) const;

  virtual void CalcCurlShape (const IntegrationPoint & ip, 
                              FlatMatrixFixWidth<DIM_CURL> curlshape) const;

  virtual void CalcMappedShape (const MappedIntegrationPoint<DIM,DIM> & mip,
                                FlatMatrixFixWidth<DIM> shape) const;

  virtual void CalcMappedCurlShape (const MappedIntegrationPoint<DIM,DIM> & mip,
                                    FlatMatrixFixWidth<DIM_CURL> curlshape) const;

  /*
  virtual Vec <DIM_CURL_TRAIT<ET_trait<ET>::DIM>::DIM>
  EvaluateCurlShape (const IntegrationPoint & ip, 
                     FlatVector<double> x,
                     LocalHeap & lh) const;
  */
};

  template <ELEMENT_TYPE ET> class HCurlHighOrderFE_Shape;
 
  template <ELEMENT_TYPE ET> 
  class HCurlHighOrderFE : public T_HCurlHighOrderFiniteElement<ET, HCurlHighOrderFE_Shape<ET> >
  {
  public:
    HCurlHighOrderFE () { ; }
    HCurlHighOrderFE (int aorder) 
      : T_HCurlHighOrderFiniteElement<ET, HCurlHighOrderFE_Shape<ET> > (aorder)
    {
      this->ComputeNDof();
    }
  };



/// A segment high order H(curl) element
template <>
class HCurlHighOrderFE_Shape<ET_SEGM>:  public HCurlHighOrderFE<ET_SEGM>
{
public:
  template<typename Tx, typename TFA>  
  void T_CalcShape (Tx hx[2], TFA & shape) const; 
};

/// A triangular high order H(curl) element
template <>
class HCurlHighOrderFE_Shape<ET_TRIG> : public HCurlHighOrderFE<ET_TRIG>
{
private:
  typedef TrigShapesInnerLegendre T_INNERSHAPES; 
public:
  template<typename Tx, typename TFA>  
  void T_CalcShape (Tx hx[2], TFA & shape) const; 
};

/// A quadrilateral high order H(curl) element
template <>
class HCurlHighOrderFE_Shape<ET_QUAD> : public HCurlHighOrderFE<ET_QUAD>
{
public:
  template<typename Tx, typename TFA>  
  void T_CalcShape (Tx hx[2], TFA & shape) const; 
};

/// A tetrahedral high order H(curl) element
template <>
class HCurlHighOrderFE_Shape<ET_TET> : public HCurlHighOrderFE<ET_TET>
{
private:
  typedef TetShapesFaceLegendre T_FACESHAPES; 
  typedef TetShapesInnerLegendre T_INNERSHAPES; 
public:
  template<typename Tx, typename TFA>  
  void T_CalcShape (Tx hx[3], TFA & shape) const; 
};

/// A hexahedral high order H(curl) element
template <>
class HCurlHighOrderFE_Shape<ET_HEX> : public HCurlHighOrderFE<ET_HEX>
{
public:
  template<typename Tx, typename TFA>  
  void T_CalcShape (Tx hx[3], TFA & shape) const; 
};

/// A prismatic high order H(curl) element
template <>
class HCurlHighOrderFE_Shape<ET_PRISM> : public HCurlHighOrderFE<ET_PRISM>
{
private:
  typedef TrigShapesInnerLegendre T_TRIGFACESHAPES;

public:
  template<typename Tx, typename TFA>  
  void T_CalcShape (Tx hx[3], TFA & shape) const; 
};

/// A pyramidal high order H(curl) element
template <>
class HCurlHighOrderFE_Shape<ET_PYRAMID> : public HCurlHighOrderFE<ET_PYRAMID>
{
  typedef TrigShapesInnerLegendre T_TRIGFACESHAPES;  
public:
  template<typename Tx, typename TFA>  
  void T_CalcShape (Tx hx[3], TFA & shape) const; 
};

}

#endif

