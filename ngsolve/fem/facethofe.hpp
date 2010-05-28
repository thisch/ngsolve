#ifndef FILE_FACETHOFE
#define FILE_FACETHOFE

/*********************************************************************/
/* File:   facethofe.hpp                                             */
/* Author: A. Sinwel, H. Egger, J. Schoeberl                         */
/* Date:   2008                                                      */
/*********************************************************************/


#include "l2hofe.hpp"
#include "tscalarfe.cpp"



namespace ngfem
{

  //------------------------------------------------------------
  class FacetVolumeTrig : public FacetVolumeFiniteElement<2>,
			  public T_ScalarFiniteElement2<FacetVolumeTrig, ET_TRIG>
  {
  protected:
    L2HighOrderFE<ET_SEGM> facets2[3];
  public:
    FacetVolumeTrig(); 
    virtual void ComputeNDof();

    template<typename Tx, typename TFA>  
    void T_CalcShape (Tx x[2], TFA & shape) const
    {
      ;
    }
  };


  // --------------------------------------------------------
  class FacetVolumeQuad : public FacetVolumeFiniteElement<2>,
			  public T_ScalarFiniteElement2<FacetVolumeQuad, ET_QUAD>
  {
  protected:
    L2HighOrderFE<ET_SEGM> facets2[4];
  public:
    FacetVolumeQuad();
    virtual void ComputeNDof();


    template<typename Tx, typename TFA>  
    void T_CalcShape (Tx x[2], TFA & shape) const
    {
      ;
    }

  };

  // --------------------------------------------------------
  class FacetVolumeTet : 
    public FacetVolumeFiniteElement<3>, 
    public T_ScalarFiniteElement2<FacetVolumeTet, ET_TET>,
    public ET_trait<ET_TET> 
  {
  protected:
    L2HighOrderFE<ET_TRIG> facets2[4];
  public:
    FacetVolumeTet(); 
    virtual void ComputeNDof();

    template<typename Tx, typename TFA>  
    void T_CalcShape (Tx hx[3], TFA & shape) const
    {
      Tx lami[4] = { hx[0], hx[1], hx[2], 1-hx[0]-hx[1]-hx[2] };

      INT<4> f = GetFaceSort (facenr, vnums);
      Tx x = lami[f[0]], y=lami[f[1]];
      
      int n = order;
      ArrayMem<Tx, 20> polx(n+1), poly(n+1);
      
      ScaledLegendrePolynomial (n, 2*x+y-1, 1-y, polx);

      for (int i = 0; i < ndof; i++)
	shape[i] = 0.0;

      int ii = first_facet_dof[facenr];
      for (int i = 0; i <= n; i++)
	{
	  JacobiPolynomial (n-i, 2*y-1, 2*i+1, 0, poly);
	  for (int j = 0; j <= n-i; j++)
	    shape[ii++] = polx[i] * poly[j];
	}
    }
  };

  // --------------------------------------------------------
  class FacetVolumeHex : public FacetVolumeFiniteElement<3>,
			 public T_ScalarFiniteElement2<FacetVolumeHex, ET_HEX>
  {
  protected:
    L2HighOrderFE<ET_QUAD> facetsq[6];
  public:
    FacetVolumeHex();
    virtual void ComputeNDof();

    template<typename Tx, typename TFA>  
    void T_CalcShape (Tx x[3], TFA & shape) const
    {
      ;
    }

  };

  // --------------------------------------------------------
  class FacetVolumePrism : public FacetVolumeFiniteElement<3>,
			   public T_ScalarFiniteElement2<FacetVolumePrism, ET_PRISM>
  {
  protected:
    L2HighOrderFE<ET_TRIG> facetst[2];
    L2HighOrderFE<ET_QUAD> facetsq[3];

  public:
    FacetVolumePrism();
    virtual void ComputeNDof();

    template<typename Tx, typename TFA>  
    void T_CalcShape (Tx x[3], TFA & shape) const
    {
      ;
    }

  };


  // --------------------------------------------------------
  class FacetVolumePyramid : public FacetVolumeFiniteElement<3>,
			     public T_ScalarFiniteElement2<FacetVolumePyramid, ET_PYRAMID>
  {
  protected:
    int tnr, qnr; // active facets
    L2HighOrderFE<ET_TRIG> trig;
    L2HighOrderFE<ET_QUAD> quad;
  public:
    FacetVolumePyramid() : FacetVolumeFiniteElement<3> (ET_PYRAMID) {  qnr=tnr=-1; };
  
    // virtual void CalcShape (const IntegrationPoint & ip, FlatVector<> shape) const; // maybe convenient for shape tester??
    virtual void CalcFacetShape (int afnr, const IntegrationPoint & ip, FlatVector<> shape) const; 
    virtual void SetFacet(int afnr) const;
  
    virtual void ComputeNDof();
    //     virtual const FiniteElement & GetFacetFE(int fnr, LocalHeap& lh) const;

    template<typename Tx, typename TFA>  
    void T_CalcShape (Tx x[3], TFA & shape) const
    {
      ;
    }

  };

}

#endif
