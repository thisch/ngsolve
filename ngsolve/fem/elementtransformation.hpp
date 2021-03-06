#ifndef FILE_ELEMENTTRANSFORMATION
#define FILE_ELEMENTTRANSFORMATION

/*********************************************************************/
/* File:   elementtransformation.hpp                                 */
/* Author: Joachim Schoeberl                                         */
/* Date:   25. Mar. 2000                                             */
/*********************************************************************/



/*
  Transformation from reference element to actual element
*/



namespace ngfem
{

  /**
     Transformation from reference element to physical element.
  */
  class NGS_DLL_HEADER ElementTransformation
  {
  protected:
    /// element number
    int elnr;
    /// material property
    int elindex;
    /// geometry of element
    ELEMENT_TYPE eltype;

    bool higher_integration_order;
    /// is the element curved ?
    bool iscurved;

  public:
    ///
    ElementTransformation () { higher_integration_order = false; } 
    ///
    virtual ~ElementTransformation() { ; } 
    /// set data: is it a boundary, element number, and element index
    virtual void SetElement (bool /* aboundary */, int aelnr, int aelindex)
    {
      elnr = aelnr;
      elindex = aelindex;
    }
    /// set geometric type of element
    void SetElementType (ELEMENT_TYPE aet) { eltype = aet; }
    /// return element number
    int GetElementNr () const { return elnr; }
    /// return element index
    int GetElementIndex () const { return elindex; }
    /// return element geometry type 
    ELEMENT_TYPE GetElementType () const { return eltype; }
    /// calculates the Jacobi matrix
    virtual void CalcJacobian (const IntegrationPoint & ip,
			       FlatMatrix<> dxdxi) const = 0;

    /// calculate the mapped point
    virtual void CalcPoint (const IntegrationPoint & ip,
			    FlatVector<> point) const = 0;

    /// calculate point and Jacobi matrix
    virtual void CalcPointJacobian (const IntegrationPoint & ip,
				    FlatVector<> point, FlatMatrix<> dxdxi) const = 0;

    /// Calculate points and Jacobimatrices in all points of integrationrule
    virtual void CalcMultiPointJacobian (const IntegrationRule & ir,
					 BaseMappedIntegrationRule & mir) const = 0;

    /// Calcs the normal vector in ip
    void CalcNormalVector (const IntegrationPoint & ip,
			   FlatVector<> nv,
			   LocalHeap & lh) const
    {
      if (Boundary())
	{
	  if (SpaceDim() == 2)
	    {
	      Mat<2,1> dxdxi;
	      CalcJacobian (ip, dxdxi);
	      // Ng_GetSurfaceElementTransformation (elnr+1, &ip(0), 0, &dxdxi(0));
	      double len = sqrt (sqr (dxdxi(0,0)) + sqr(dxdxi(1,0)));
	      nv(0) = -dxdxi(1,0) / len; //SZ 
	      nv(1) = dxdxi(0,0) / len;
	    }
	  else
	    {
	      Mat<3,2> dxdxi;
	      CalcJacobian (ip, dxdxi);
	      // Ng_GetSurfaceElementTransformation (elnr+1, &ip(0), 0, &dxdxi(0));
	      nv(0) = dxdxi(1,0) * dxdxi(2,1) - dxdxi(2,0) * dxdxi(1,1);
	      nv(1) = dxdxi(2,0) * dxdxi(0,1) - dxdxi(0,0) * dxdxi(2,1);
	      nv(2) = dxdxi(0,0) * dxdxi(1,1) - dxdxi(1,0) * dxdxi(0,1);
	      nv /= L2Norm (nv);
	    }
	}
    }
  
  
    /// returns space dimension of physical elements
    virtual int SpaceDim () const = 0;

    /// is it a mapping for boundary elements ?
    virtual bool Boundary() const = 0;

    void SetHigherIntegrationOrder(void) {higher_integration_order = true;}
    void UnSetHigherIntegrationOrder(void) {higher_integration_order = false;}
    bool HigherIntegrationOrderSet(void) const 
    {
      return higher_integration_order;
    }

    /// has the element non-constant Jacobian ?
    virtual bool IsCurvedElement() const 
    {
      return false;
    }

    virtual void GetSort (FlatArray<int> sort) const
    { ; }

    /// return a mapped integration point on localheap
    virtual BaseMappedIntegrationPoint & operator() (const IntegrationPoint & ip, LocalHeap & lh) const = 0;

    /// return a mapped integration rule on localheap
    virtual BaseMappedIntegrationRule & operator() (const IntegrationRule & ir, LocalHeap & lh) const = 0;

    virtual bool BelongsToMesh (const void * mesh) const { return true; }
    virtual const void * GetMesh () const { return NULL; }
    
  private:
    ElementTransformation (const ElementTransformation & eltrans2) { ; }
    ElementTransformation & operator= (const ElementTransformation & eltrans2) 
    { return *this; }
  };











  /*
    Transformation from reference element to physical element.
    Uses finite element fel to describe mapping
  */
  template <int DIMS, int DIMR>
  class NGS_DLL_HEADER FE_ElementTransformation : public ElementTransformation
  {
    /// finite element defining transformation.
    const ScalarFiniteElement<DIMS> * fel;

    /// matrix with points, dim * np
    Matrix<> pointmat;
    ///
    // bool pointmat_ownmem;

    /// normal vectors (only surfelements)
    FlatMatrix<> nvmat;
  public:
    /// type of element, np x dim point-matrix
    FE_ElementTransformation (ELEMENT_TYPE type, FlatMatrix<> pmat);
    ///
    FE_ElementTransformation ();

    ///
    ~FE_ElementTransformation ();

    ///
    virtual void SetElement (const FiniteElement * afel, int aelnr, int aelindex)
    {
      fel = static_cast<const ScalarFiniteElement<DIMS>*> (afel); 
      elnr = aelnr; 
      elindex = aelindex;
      SetElementType (fel->ElementType());
      pointmat.SetSize (DIMR, fel->GetNDof());
    }

    ///
    const FiniteElement & GetElement () const { return *fel; }

  
    ELEMENT_TYPE GetElementType () const { return fel->ElementType(); }
  
    ///
    virtual void CalcJacobian (const IntegrationPoint & ip,
			       FlatMatrix<> dxdxi) const;

    ///
    virtual void CalcPoint (const IntegrationPoint & ip, 
			    FlatVector<> point) const;

    ///
    virtual void CalcPointJacobian (const IntegrationPoint & ip,
				    FlatVector<> point, 
				    FlatMatrix<> dxdxi) const;




    virtual void CalcMultiPointJacobian (const IntegrationRule & ir,
					 BaseMappedIntegrationRule & bmir) const;

    ///
    // const FlatMatrix<> & PointMatrix () const { return pointmat; }
    ///
    FlatMatrix<> PointMatrix () const { return pointmat; }
    ///
    /*
    void AllocPointMatrix (int spacedim, int vertices)
    {
      if (pointmat_ownmem) delete [] &pointmat(0,0);
      pointmat.AssignMemory (spacedim, vertices, new double[spacedim*vertices]);
      pointmat_ownmem = 1;
    }
    */
    ///
    const FlatMatrix<> & NVMatrix () const { return nvmat; }

    template <typename T>
    void CalcNormalVector (const IntegrationPoint & ip,
			   T & nv,
			   LocalHeap & lh) const
    {
      for (int i = 0; i < nvmat.Height(); i++)
	nv(i) = nvmat(i,0) ;
    }

    ///
    int SpaceDim () const
    {
      return pointmat.Height(); 
    }

    bool Boundary(void) const
    {
      return pointmat.Height() != ElementTopology::GetSpaceDim (fel->ElementType());
    }

    void GetSort (FlatArray<int> sort) const
    { ; }


    virtual BaseMappedIntegrationPoint & operator() (const IntegrationPoint & ip, LocalHeap & lh) const
    {
      return *new (lh) MappedIntegrationPoint<DIMS,DIMR> (ip, *this);
    }

    virtual BaseMappedIntegrationRule & operator() (const IntegrationRule & ir, LocalHeap & lh) const
    {
      return *new (lh) MappedIntegrationRule<DIMS,DIMR> (ir, *this, lh);
    }

  };






}




#endif






