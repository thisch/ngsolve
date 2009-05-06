#ifndef FILE_INTEGRATOR
#define FILE_INTEGRATOR

/*********************************************************************/
/* File:   integrator.hpp                                            */
/* Author: Joachim Schoeberl                                         */
/* Date:   25. Mar. 2000                                             */
/*********************************************************************/

namespace ngfem
{

  /*
    bilinear-form and linear-form integrators
  */


  /**
     Base class for linear-form and bilinear-form integrators.
     Provides integration order, restriction to subdomains
  */
  class Integrator
  {
  protected:
    /// define only on some sub-domains
    BitArray definedon;

    /// if >= 0, use this order of integration
    int integration_order;

    // lower bound for the integration order
    int higher_integration_order;

    /// if >= 0, use this order of integration for all terms
    static int common_integration_order;


    /// fast integration ?
    bool fast;

    /// plane element and constant coefficients 
    bool const_coef;
  
    /// check fast integration correct ?
    bool checkfast;

    ///
    string name;

    /// integration only along curve
    Array < FlatVector < double > * > curve_ips;
    Array < FlatVector < double > * > curve_ip_tangents;
    Array <int> continuous_curveparts;
  
    int cachecomp;

  
  protected:
    void DeleteCurveIPs ( void );

  public:
    /// constructor
    Integrator() throw ();

    /// destructor
    virtual ~Integrator();

    /// integrates on the boundary, or on the domain ?
    virtual bool BoundaryForm () const = 0;

    /// Is Integrator defined on this sub-domain ?
    bool DefinedOn (int mat) const;

    /// defined only on some subdomains
    void SetDefinedOn (const BitArray & adefinedon);

    bool DefinedOnSubdomainsOnly() const
    { return definedon.Size() != 0; }



    /// use exactly this integration order for all integrals
    static void SetCommonIntegrationOrder (int cio)
    { 
      common_integration_order = cio; 
    }

    static int GetCommonIntegrationOrder ()
    { 
      return common_integration_order;
    }

    /// set minimal integration order
    void SetHigherIntegrationOrder (int io)
    {
      higher_integration_order = io; 
    }

    /// set integration order
    void SetIntegrationOrder (int io)
    {
      integration_order = io; 
    }

    /// returns integration order
    int GetIntegrationOrder (void) const
    {
      return integration_order;
    }

    /// use fast (tensor product) integration, if available
    void SetFastIntegration (bool afast = 1, bool acheck = 0) 
    { fast = afast; checkfast = acheck; }

    /// benefit from constant coefficient
    void SetConstantCoefficient (bool acc = 1)
    { const_coef = acc; }

    /// dimension of element
    virtual int DimElement () const { return -1; }

    /// dimension of space
    virtual int DimSpace () const { return -1; }

    /// 
    void SetName (const string & aname);
    ///
    virtual string Name () const;


    // added by MW:   Markus, müssen die virtual sein (JS) ??
    virtual bool IntegrationAlongCurve (void) const
    { return curve_ips.Size() > 0; }

    virtual void SetIntegrationAlongCurve ( const int npoints );

    virtual void UnSetIntegrationAlongCurve ( void );

    virtual int NumCurvePoints(void) const
    { return curve_ips.Size(); }

    virtual FlatVector<double> & CurvePoint(const int i)
    { return *(curve_ips[i]); }

    virtual const FlatVector<double> & CurvePoint(const int i) const
    { return *(curve_ips[i]); }

  
    virtual FlatVector<double> & CurvePointTangent(const int i)
    { return *(curve_ip_tangents[i]); }

    virtual const FlatVector<double> & CurvePointTangent(const int i) const
    { return *(curve_ip_tangents[i]); }

    virtual int GetNumCurveParts(void) const;
    virtual int GetStartOfCurve(const int i) const;
    virtual int GetEndOfCurve(const int i) const;

    virtual void AppendCurvePoint(const FlatVector<double> & point);
    virtual void AppendCurvePoint(const FlatVector<double> & point, const FlatVector<double> & tangent);
    virtual void SetCurveClearance(void);


  
    virtual void SetCacheComp(const int comp)
    { cachecomp = comp; }

    virtual int CacheComp(void) const
    { return cachecomp; }

    virtual void SetFileName(const string & filename)
    {
      cerr << "SetFileName not defined for Integrator base class" << endl;
    }
    

  };




  /**
     A BilinearFormIntegrator computes the element matrices. Different
     equations are provided by derived classes. An Integrator can be defined
     in the domain or at the boundary.
  */
  class BilinearFormIntegrator : public Integrator
  {
  public:
    // typedef double TSCAL;
    ///
    BilinearFormIntegrator () throw ();
    ///
    virtual ~BilinearFormIntegrator ();

    /// components of flux
    virtual int DimFlux () const { return -1; }

    /**
       Assembles element matrix.
       Result is in elmat, memory is allocated by functions on LocalHeap.
    */
    virtual void
    AssembleElementMatrix (const FiniteElement & fel,
			   const ElementTransformation & eltrans, 
			   FlatMatrix<double> & elmat,
			   LocalHeap & locheap) const = 0;

    virtual void
    AssembleElementMatrix (const FiniteElement & fel,
			   const ElementTransformation & eltrans, 
			   FlatMatrix<Complex> & elmat,
			   LocalHeap & locheap) const;

    virtual void
    AssembleElementMatrixIndependent (const FiniteElement & bfel_master,
				      const FiniteElement & bfel_master_element,				    
				      const FiniteElement & bfel_slave,
				      const ElementTransformation & eltrans_master, 
				      const ElementTransformation & eltrans_master_element, 
				      const ElementTransformation & eltrans_slave,
				      const IntegrationPoint & ip_master,
				      const IntegrationPoint & ip_master_element,
				      const IntegrationPoint & ip_slave,
				      FlatMatrix<double> & elmat,
				      LocalHeap & locheap) const
    {;}
    virtual void
    ApplyElementMatrixIndependent (const FiniteElement & bfel_master,
				   const FiniteElement & bfel_master_element,				    
				   const FiniteElement & bfel_slave,
				   const ElementTransformation & eltrans_master, 
				   const ElementTransformation & eltrans_master_element, 
				   const ElementTransformation & eltrans_slave,
				   const IntegrationPoint & ip_master,
				   const IntegrationPoint & ip_master_element,
				   const IntegrationPoint & ip_slave,
				   const FlatVector<double> & elx,
				   Vector<double> & result,
				   LocalHeap & locheap) const
    {;}
    virtual void
    AssembleElementMatrixIndependent (const FiniteElement & bfel_master,
				      const FiniteElement & bfel_master_element,				    
				      const FiniteElement & bfel_slave,
				      const ElementTransformation & eltrans_master, 
				      const ElementTransformation & eltrans_master_element, 
				      const ElementTransformation & eltrans_slave,
				      const IntegrationPoint & ip_master,
				      const IntegrationPoint & ip_master_element,
				      const IntegrationPoint & ip_slave,
				      FlatMatrix<Complex> & elmat,
				      LocalHeap & locheap) const
    {
      FlatMatrix<double> rmat;
      AssembleElementMatrixIndependent(bfel_master,bfel_master_element,bfel_slave,
				       eltrans_master, eltrans_master_element, eltrans_slave,
				       ip_master, ip_master_element, ip_slave,
				       rmat, locheap);
      elmat.AssignMemory(rmat.Height(), rmat.Width(), locheap);
      elmat = rmat;
    }

    virtual void
    AssembleElementMatrixIndependent (const FiniteElement & bfel_master,
				      const FiniteElement & bfel_slave,
				      const ElementTransformation & eltrans_master, 
				      const ElementTransformation & eltrans_slave,
				      const IntegrationPoint & ip_master,
				      const IntegrationPoint & ip_slave,
				      FlatMatrix<double> & elmat,
				      LocalHeap & locheap) const
    {;}
    virtual void
    AssembleElementMatrixIndependent (const FiniteElement & bfel_master,
				      const FiniteElement & bfel_slave,
				      const ElementTransformation & eltrans_master, 
				      const ElementTransformation & eltrans_slave,
				      const IntegrationPoint & ip_master,
				      const IntegrationPoint & ip_slave,
				      FlatMatrix<Complex> & elmat,
				      LocalHeap & locheap) const
    {
      FlatMatrix<double> rmat;
      AssembleElementMatrixIndependent(bfel_master,bfel_slave,
				       eltrans_master, eltrans_slave,
				       ip_master, ip_slave,
				       rmat, locheap);
      elmat.AssignMemory(rmat.Height(), rmat.Width(), locheap);
      elmat = rmat;
    }


    virtual void
    AssembleElementMatrixDiag (const FiniteElement & fel,
			       const ElementTransformation & eltrans, 
			       FlatVector<double> & diag,
			       LocalHeap & lh) const;




    virtual void
    AssembleLinearizedElementMatrix (const FiniteElement & fel, 
				     const ElementTransformation & eltrans, 
				     FlatVector<double> & elveclin,
				     FlatMatrix<double> & elmat,
				     LocalHeap & locheap) const;

    virtual void
    AssembleLinearizedElementMatrix (const FiniteElement & fel, 
				     const ElementTransformation & eltrans, 
				     FlatVector<Complex> & elveclin,
				     FlatMatrix<Complex> & elmat,
				     LocalHeap & locheap) const;


    virtual void *  
    PrecomputeData (const FiniteElement & fel, 
		    const ElementTransformation & eltrans, 
		    LocalHeap & locheap) const { return 0; }
  

    virtual void 
    ApplyElementMatrix (const FiniteElement & fel, 
			const ElementTransformation & eltrans, 
			const FlatVector<double> & elx, 
			FlatVector<double> & ely,
			void * precomputed,
			LocalHeap & locheap) const;

    virtual void 
    ApplyElementMatrix (const FiniteElement & fel, 
			const ElementTransformation & eltrans, 
			const FlatVector<Complex> & elx, 
			FlatVector<Complex> & ely,
			void * precomputed,
			LocalHeap & locheap) const;


    template < int S, class T>
    void ApplyElementMatrix (const FiniteElement & fel, 
			     const ElementTransformation & eltrans, 
			     const FlatVector< Vec<S,T> > & elx, 
			     FlatVector< Vec<S,T> > & ely,
			     void * precomputed,
			     LocalHeap & locheap) const
    {
      //cout << "call baseclass ApplyElementMatrix, type = " << typeid(*this).name() << endl;
      FlatMatrix<T> mat;
      AssembleElementMatrix (fel, eltrans, mat, locheap);
      ely = mat * elx;
    }


    virtual void 
    ApplyLinearizedElementMatrix (const FiniteElement & fel, 
				  const ElementTransformation & eltrans, 
				  const FlatVector<double> & ellin,
				  const FlatVector<double> & elx, 
				  FlatVector<double> & ely,
				  LocalHeap & locheap) const;

    virtual void 
    ApplyLinearizedElementMatrix (const FiniteElement & fel, 
				  const ElementTransformation & eltrans, 
				  const FlatVector<Complex> & ellin, 
				  const FlatVector<Complex> & elx, 
				  FlatVector<Complex> & ely,
				  LocalHeap & locheap) const;



    virtual double Energy (const FiniteElement & fel, 
			   const ElementTransformation & eltrans, 
			   const FlatVector<double> & elx, 
			   LocalHeap & locheap) const;

    virtual double Energy (const FiniteElement & fel, 
			   const ElementTransformation & eltrans, 
			   const FlatVector<Complex> & elx, 
			   LocalHeap & locheap) const;


    /*
   ///
   virtual FlatMatrix<double> 
   AssembleMixedElementMatrix (const FiniteElement & fel1, 
   const FiniteElement & fel2, 
   const ElementTransformation & eltrans, 
   LocalHeap & locheap) const;
   {
   cerr << "AssembleMixedElementMatrix called for base class" << endl;
   return FlatMatrix<TSCAL> (0,0,0);
   }

   ///
   virtual void 
   ApplyMixedElementMatrix (const FiniteElement & fel1, 
   const FiniteElement & fel2, 
   const ElementTransformation & eltrans, 
   const FlatVector<TSCAL> & elx, 
   FlatVector<TSCAL> & ely,
   LocalHeap & locheap) const;
   {
   ely = AssembleMixedElementMatrix (fel1, fel2, eltrans, locheap) * elx;
   }
    */

    virtual void
    CalcFlux (const FiniteElement & fel,
	      const ElementTransformation & eltrans,
	      const IntegrationPoint & ip,
	      const FlatVector<double> & elx, 
	      FlatVector<double> & flux,
	      bool applyd,
	      LocalHeap & lh) const;

    virtual void
    CalcFlux (const FiniteElement & fel,
	      const ElementTransformation & eltrans,
	      const IntegrationPoint & ip,
	      const FlatVector<Complex> & elx, 
	      FlatVector<Complex> & flux,
	      bool applyd,
	      LocalHeap & lh) const;

    virtual void
    CalcFlux (const FiniteElement & fel,
	      const BaseSpecificIntegrationPoint & bsip,
	      const FlatVector<double> & elx, 
	      FlatVector<double> & flux,
	      bool applyd,
	      LocalHeap & lh) const;


    virtual void
    CalcFlux (const FiniteElement & fel,
	      const BaseSpecificIntegrationPoint & bsip,
	      const FlatVector<Complex> & elx, 
	      FlatVector<Complex> & flux,
	      bool applyd,
	      LocalHeap & lh) const;

    virtual void
    CalcFluxMulti (const FiniteElement & fel,
		   const BaseSpecificIntegrationPoint & bsip,
		   int m,
		   const FlatVector<double> & elx, 
		   FlatVector<double> & flux,
		   bool applyd,
		   LocalHeap & lh) const;


    virtual void
    CalcFluxMulti (const FiniteElement & fel,
		   const BaseSpecificIntegrationPoint & bsip,
		   int m,
		   const FlatVector<Complex> & elx, 
		   FlatVector<Complex> & flux,
		   bool applyd,
		   LocalHeap & lh) const;


    virtual void
    ApplyBTrans (const FiniteElement & fel,
		 // const ElementTransformation & eltrans,
		 // const IntegrationPoint & ip,
		 const BaseSpecificIntegrationPoint & bsip,
		 const FlatVector<double> & elx, 
		 FlatVector<double> & ely,
		 LocalHeap & lh) const;

    virtual void
    ApplyBTrans (const FiniteElement & fel,
		 // const ElementTransformation & eltrans,
		 // const IntegrationPoint & ip,
		 const BaseSpecificIntegrationPoint & bsip,
		 const FlatVector<Complex> & elx, 
		 FlatVector<Complex> & ely,
		 LocalHeap & lh) const;

    virtual void ApplyDMat (const FiniteElement & bfel,
			    const BaseSpecificIntegrationPoint & bsip,
			    const FlatVector<double> & elx, 
			    FlatVector<double> & eldx,
			    LocalHeap & lh) const;

    virtual void ApplyDMat (const FiniteElement & bfel,
			    const BaseSpecificIntegrationPoint & bsip,
			    const FlatVector<Complex> & elx, 
			    FlatVector<Complex> & eldx,
			    LocalHeap & lh) const;
  

    virtual const IntegrationRule & GetIntegrationRule (const FiniteElement & fel,
							const bool use_higher_integration_order = false) const;
    /*
      {
      cerr << "GetIntegrationRule  called for class " 
      << typeid(*this).name()
      << endl;
      static IntegrationRule dummy; return dummy;
      }
    */
  };






  // returns coef * Identity-matrix
  class RegularizationBilinearFormIntegrator : public BilinearFormIntegrator
  {
    CoefficientFunction * coef;
  public:
    RegularizationBilinearFormIntegrator (CoefficientFunction * acoef)
      : coef(acoef)
    { ; }

    virtual bool BoundaryForm () const { return 0; }
    virtual int DimFlux () const { return 1; }
    virtual int DimElement () const { return 3; }
    virtual int DimSpace () const { return 3; }

    virtual void
    AssembleElementMatrix (const FiniteElement & fel,
			   const ElementTransformation & eltrans, 
			   FlatMatrix<double> & elmat,
			   LocalHeap & locheap) const
    {
      int ndof = fel.GetNDof();

      // elmat.AssignMemory (ndof, ndof, locheap);
      elmat = 0;
      SpecificIntegrationPoint<3,3> sip (GetIntegrationRules().SelectIntegrationRule (fel.ElementType(), 0)[0], 
					 eltrans, locheap);
      double val = Evaluate (*coef, sip);
      elmat = 0;
      for (int i = 0; i < ndof; i++)
	elmat(i,i) = val;
    }

    virtual string Name () const { return "Regularization"; }
  };





  class BlockBilinearFormIntegrator : public BilinearFormIntegrator
  {
    BilinearFormIntegrator & bfi;
    int dim;
    int comp;
  public:
    BlockBilinearFormIntegrator (BilinearFormIntegrator & abfi, int adim, int acomp);
    BlockBilinearFormIntegrator (BilinearFormIntegrator & abfi, int adim);
    virtual ~BlockBilinearFormIntegrator ();

    virtual bool BoundaryForm () const
    { return bfi.BoundaryForm(); }

    virtual int DimFlux () const 
    { return (comp == -1) ? dim * bfi.DimFlux() : bfi.DimFlux(); }

    const BilinearFormIntegrator & Block () const { return bfi; }

    virtual void
    AssembleElementMatrix (const FiniteElement & bfel, 
			   const ElementTransformation & eltrans, 
			   FlatMatrix<double> & elmat,
			   LocalHeap & locheap) const;

    virtual void
    AssembleElementMatrix (const FiniteElement & bfel, 
			   const ElementTransformation & eltrans, 
			   FlatMatrix<Complex> & elmat,
			   LocalHeap & locheap) const;

    virtual void 
    ApplyElementMatrix (const FiniteElement & bfel, 
			const ElementTransformation & eltrans, 
			const FlatVector<double> & elx, 
			FlatVector<double> & ely,
			void * precomputed,
			LocalHeap & locheap) const;

    virtual void 
    ApplyElementMatrix (const FiniteElement & bfel, 
			const ElementTransformation & eltrans, 
			const FlatVector<Complex> & elx, 
			FlatVector<Complex> & ely,
			void * precomputed,
			LocalHeap & locheap) const;

    virtual void
    AssembleLinearizedElementMatrix (const FiniteElement & bfel,
				     const ElementTransformation & eltrans,
				     FlatVector<double> & elveclin,
				     FlatMatrix<double> & elmat,
				     LocalHeap & locheap) const;
    virtual void
    AssembleLinearizedElementMatrix (const FiniteElement & bfel, 
				     const ElementTransformation & eltrans, 
				     FlatVector<Complex> & elveclin,
				     FlatMatrix<Complex> & elmat,
				     LocalHeap & locheap) const;


    virtual void
    CalcFlux (const FiniteElement & fel,
	      const ElementTransformation & eltrans,
	      const IntegrationPoint & ip,
	      const FlatVector<double> & elx, 
	      FlatVector<double> & flux,
	      bool applyd,
	      LocalHeap & lh) const;

    virtual void
    CalcFlux (const FiniteElement & fel,
	      const ElementTransformation & eltrans,
	      const IntegrationPoint & ip,
	      const FlatVector<Complex> & elx, 
	      FlatVector<Complex> & flux,
	      bool applyd,
	      LocalHeap & lh) const;



    virtual void
    CalcFlux (const FiniteElement & fel,
	      const BaseSpecificIntegrationPoint & bsip,
	      const FlatVector<double> & elx, 
	      FlatVector<double> & flux,
	      bool applyd,
	      LocalHeap & lh) const;

    virtual void
    CalcFlux (const FiniteElement & fel,
	      const BaseSpecificIntegrationPoint & bsip,
	      const FlatVector<Complex> & elx, 
	      FlatVector<Complex> & flux,
	      bool applyd,
	      LocalHeap & lh) const;



    virtual void
    ApplyBTrans (const FiniteElement & bfel,
		 const BaseSpecificIntegrationPoint & bsip,
		 const FlatVector<double> & elx, 
		 FlatVector<double> & ely,
		 LocalHeap & lh) const;

    virtual void
    ApplyBTrans (const FiniteElement & bfel,
		 const BaseSpecificIntegrationPoint & bsip,
		 const FlatVector<Complex> & elx, 
		 FlatVector<Complex> & ely,
		 LocalHeap & lh) const;

    virtual double Energy (const FiniteElement & fel, 
			   const ElementTransformation & eltrans, 
			   const FlatVector<double> & elx, 
			   LocalHeap & locheap) const;

    virtual string Name () const;
  };




  class NormalBilinearFormIntegrator : public BilinearFormIntegrator
  {
    const BilinearFormIntegrator & bfi;
  public:
    NormalBilinearFormIntegrator (const BilinearFormIntegrator & abfi);
    virtual bool BoundaryForm () const
    { return bfi.BoundaryForm(); }

    virtual void
    AssembleElementMatrix (const FiniteElement & bfel, 
			   const ElementTransformation & eltrans, 
			   FlatMatrix<double> & elmat,
			   LocalHeap & locheap) const;
  };







  class ComplexBilinearFormIntegrator : public BilinearFormIntegrator
  {
    const BilinearFormIntegrator & bfi;
    Complex factor;
  public:
    ComplexBilinearFormIntegrator (const BilinearFormIntegrator & abfi,
				   Complex afactor);

    virtual bool BoundaryForm () const
    { return bfi.BoundaryForm(); }

    virtual int DimFlux () const
    { return bfi.DimFlux(); }
    virtual int DimElement () const
    { return bfi.DimElement(); }
    virtual int DimSpace () const
    { return bfi.DimSpace(); }

    virtual void GetFactor(Complex & fac) const {fac = factor;}
    virtual void GetFactor(double & fac) const {fac = factor.real();}
  
    virtual const BilinearFormIntegrator & GetBFI(void) const {return bfi;}


    virtual void
    AssembleElementMatrix (const FiniteElement & fel, 
			   const ElementTransformation & eltrans, 
			   FlatMatrix<double> & elmat,
			   LocalHeap & locheap) const;

    virtual void
    AssembleElementMatrix (const FiniteElement & fel, 
			   const ElementTransformation & eltrans, 
			   FlatMatrix<Complex> & elmat,
			   LocalHeap & locheap) const;

    virtual void
    AssembleElementMatrixIndependent (const FiniteElement & bfel_master,
				      const FiniteElement & bfel_master_element,				    
				      const FiniteElement & bfel_slave,
				      const ElementTransformation & eltrans_master, 
				      const ElementTransformation & eltrans_master_element, 
				      const ElementTransformation & eltrans_slave,
				      const IntegrationPoint & ip_master,
				      const IntegrationPoint & ip_master_element,
				      const IntegrationPoint & ip_slave,
				      FlatMatrix<double> & elmat,
				      LocalHeap & locheap) const;

    virtual void
    AssembleElementMatrixIndependent (const FiniteElement & bfel_master,
				      const FiniteElement & bfel_master_element,				    
				      const FiniteElement & bfel_slave,
				      const ElementTransformation & eltrans_master, 
				      const ElementTransformation & eltrans_master_element, 
				      const ElementTransformation & eltrans_slave,
				      const IntegrationPoint & ip_master,
				      const IntegrationPoint & ip_master_element,
				      const IntegrationPoint & ip_slave,
				      FlatMatrix<Complex> & elmat,
				      LocalHeap & locheap) const;

    virtual void
    AssembleElementMatrixIndependent (const FiniteElement & bfel_master,
				      const FiniteElement & bfel_slave,
				      const ElementTransformation & eltrans_master, 
				      const ElementTransformation & eltrans_slave,
				      const IntegrationPoint & ip_master,
				      const IntegrationPoint & ip_slave,
				      FlatMatrix<double> & elmat,
				      LocalHeap & locheap) const;

    virtual void
    AssembleElementMatrixIndependent (const FiniteElement & bfel_master,
				      const FiniteElement & bfel_slave,
				      const ElementTransformation & eltrans_master, 
				      const ElementTransformation & eltrans_slave,
				      const IntegrationPoint & ip_master,
				      const IntegrationPoint & ip_slave,
				      FlatMatrix<Complex> & elmat,
				      LocalHeap & locheap) const;

  

    virtual void 
    ApplyElementMatrix (const FiniteElement & fel, 
			const ElementTransformation & eltrans, 
			const FlatVector<Complex> & elx, 
			FlatVector<Complex> & ely,
			void * precomputed,
			LocalHeap & locheap) const;

    virtual void
    CalcFlux (const FiniteElement & fel,
	      const ElementTransformation & eltrans,
	      const IntegrationPoint & ip,
	      const FlatVector<Complex> & elx, 
	      FlatVector<Complex> & flux,
	      bool applyd,
	      LocalHeap & lh) const;


    virtual void
    CalcFlux (const FiniteElement & fel,
	      const BaseSpecificIntegrationPoint & bsip,
	      const FlatVector<Complex> & elx, 
	      FlatVector<Complex> & flux,
	      bool applyd,
	      LocalHeap & lh) const;

    virtual string Name () const;

  
    virtual const IntegrationRule & GetIntegrationRule (const FiniteElement & fel,
							const bool use_higher_integration_order = false) const;
  };






  class CompoundBilinearFormIntegrator : public BilinearFormIntegrator
  {
    const BilinearFormIntegrator & bfi;
    int comp;
  public:
    CompoundBilinearFormIntegrator (const BilinearFormIntegrator & abfi, int acomp);
  
    const BilinearFormIntegrator * GetBFI(void) const {return &bfi;}

    virtual bool BoundaryForm () const
    { return bfi.BoundaryForm(); }

    virtual int DimFlux () const 
    { return bfi.DimFlux(); }
    virtual int DimElement () const
    { return bfi.DimElement(); }
    virtual int DimSpace () const
    { return bfi.DimSpace(); }


    virtual void
    AssembleElementMatrix (const FiniteElement & bfel, 
			   const ElementTransformation & eltrans, 
			   FlatMatrix<double> & elmat,
			   LocalHeap & locheap) const;

    virtual void
    AssembleElementMatrix (const FiniteElement & bfel, 
			   const ElementTransformation & eltrans, 
			   FlatMatrix<Complex> & elmat,
			   LocalHeap & locheap) const;


    virtual void
    AssembleLinearizedElementMatrix (const FiniteElement & fel, 
				     const ElementTransformation & eltrans, 
				     FlatVector<double> & elveclin,
				     FlatMatrix<double> & elmat,
				     LocalHeap & locheap) const;

    virtual void
    AssembleLinearizedElementMatrix (const FiniteElement & fel, 
				     const ElementTransformation & eltrans, 
				     FlatVector<Complex> & elveclin,
				     FlatMatrix<Complex> & elmat,
				     LocalHeap & locheap) const;

    virtual void
    ApplyElementMatrix (const FiniteElement & bfel, 
			const ElementTransformation & eltrans, 
			const FlatVector<double> & elx,
			FlatVector<double> & ely,
			void * precomputed,
			LocalHeap & locheap) const;

    virtual void
    ApplyElementMatrix (const FiniteElement & bfel, 
			const ElementTransformation & eltrans, 
			const FlatVector<Complex> & elx,
			FlatVector<Complex> & ely,
			void * precomputed,
			LocalHeap & locheap) const;

    virtual void
    ApplyLinearizedElementMatrix (const FiniteElement & bfel, 
				  const ElementTransformation & eltrans, 
				  const FlatVector<double> & ellin,
				  const FlatVector<double> & elx,
				  FlatVector<double> & ely,
				  LocalHeap & locheap) const;

    virtual void
    ApplyLinearizedElementMatrix (const FiniteElement & bfel, 
				  const ElementTransformation & eltrans, 
				  const FlatVector<Complex> & ellin,
				  const FlatVector<Complex> & elx,
				  FlatVector<Complex> & ely,
				  LocalHeap & locheap) const;

    virtual void
    CalcFlux (const FiniteElement & bfel,
	      const ElementTransformation & eltrans,
	      const IntegrationPoint & ip,
	      const FlatVector<double> & elx, 
	      FlatVector<double> & flux,
	      bool applyd,
	      LocalHeap & lh) const;

    virtual void
    CalcFlux (const FiniteElement & bfel,
	      const ElementTransformation & eltrans,
	      const IntegrationPoint & ip,
	      const FlatVector<Complex> & elx, 
	      FlatVector<Complex> & flux,
	      bool applyd,
	      LocalHeap & lh) const;

    virtual void
    CalcFlux (const FiniteElement & fel,
	      const BaseSpecificIntegrationPoint & bsip,
	      const FlatVector<double> & elx, 
	      FlatVector<double> & flux,
	      bool applyd,
	      LocalHeap & lh) const;


    virtual void
    CalcFlux (const FiniteElement & fel,
	      const BaseSpecificIntegrationPoint & bsip,
	      const FlatVector<Complex> & elx, 
	      FlatVector<Complex> & flux,
	      bool applyd,
	      LocalHeap & lh) const;


    virtual string Name () const;
  };







#ifdef NOTAVAILABLE


  /*
    Mixed element matrix assembling.
    NOT FUNCTIONAL
    Assembling for mixed bilinear-forms of type $\int (B_2 v) : D (B_1 u) dx$.
  */

  template <int SPACEDIM = 2, class SCAL = double>
  class B1DB2Integrator : public BilinearFormIntegrator
  {
  public:
    ///
    B1DB2Integrator ();
    ///
    virtual ~B1DB2Integrator ();
    ///
    /*
      virtual BaseMatrix &
      AssembleMixedElementMatrix (const FiniteElement & fel1, 
      const FiniteElement & fel2, 
      const ElementTransformation & eltrans, 
      LocalHeap & locheap) const;
    */
    ///
    virtual int GetDimension1 () const = 0;
    ///
    virtual int GetDimension2 () const = 0;
    ///
    virtual int GetDimensionD1 () const = 0;
    ///
    virtual int GetDimensionD2 () const = 0;
    ///
    virtual int DiffOrder1 () const { return 0; }
    ///
    virtual int DiffOrder2 () const { return 0; }

    ///
    virtual void GenerateB1Matrix (const FiniteElement & fel,
				   const SpecificIntegrationPoint<> & sip,
				   FlatMatrix<> & bmat,
				   LocalHeap & locheap) const = 0;

    ///
    virtual void GenerateB2Matrix (const FiniteElement & fel,
				   const SpecificIntegrationPoint<> & sip,
				   FlatMatrix<> & bmat,
				   LocalHeap & locheap) const = 0;

    ///
    virtual void GenerateDMatrix (const FiniteElement & fel,
				  const SpecificIntegrationPoint<> & sip,
				  FlatMatrix<SCAL> & dmat,
				  LocalHeap & locheap) const = 0;

    ///
    virtual int Lumping () const
    { return 0; }
    ///
    virtual string Name () const { return string("B1DB2 integrator"); }
  };


#endif










  template <int DIM_SPACE>
  class DirichletPenaltyIntegrator : public BilinearFormIntegrator
  {
    CoefficientFunction * penalty;
  public:
    DirichletPenaltyIntegrator (CoefficientFunction * apenalty)
      : penalty(apenalty) { ; }

    static Integrator * Create (Array<CoefficientFunction*> & coeffs)
    {
      return new DirichletPenaltyIntegrator (coeffs[0]);
    }

    virtual bool BoundaryForm () const
    { return 1; }

    virtual string Name () const
    { return "DirichletPenalty"; }

    virtual void
    AssembleElementMatrix (const FiniteElement & fel, 
			   const ElementTransformation & eltrans, 
			   FlatMatrix<double> & elmat,
			   LocalHeap & locheap) const
    {
      int ndof = fel.GetNDof();

      // elmat.AssignMemory (ndof, ndof, locheap);
    
      const IntegrationRule & ir = GetIntegrationRules().SelectIntegrationRule (fel.ElementType(), 0);
      SpecificIntegrationPoint<DIM_SPACE-1,DIM_SPACE> sip (ir[0], eltrans, locheap);

      double val = Evaluate (*penalty, sip);

      elmat = 0;
      for (int i = 0; i < ndof; i++)
	elmat(i,i) = val;
    }


    virtual void 
    ApplyElementMatrix (const FiniteElement & fel, 
			const ElementTransformation & eltrans, 
			const FlatVector<Complex> & elx, 
			FlatVector<Complex> & ely,
			void * precomputed,
			LocalHeap & locheap) const
    {
      const IntegrationRule & ir = GetIntegrationRules().SelectIntegrationRule (fel.ElementType(), 0);
      SpecificIntegrationPoint<DIM_SPACE-1,DIM_SPACE> sip (ir[0], eltrans, locheap);
      double val = Evaluate (*penalty, sip);

      ely = val * elx;
    }
  };





















  /**
     Integrator for element vector.
  */
  class LinearFormIntegrator : public Integrator

  {
  public:
    ///
    LinearFormIntegrator () { ; }
    ///
    virtual ~LinearFormIntegrator () { ; }


    virtual void 
    AssembleElementVector (const FiniteElement & fel,
			   const ElementTransformation & eltrans, 
			   FlatVector<double> & elvec,
			   LocalHeap & locheap) const = 0;

    virtual void 
    AssembleElementVector (const FiniteElement & fel,
			   const ElementTransformation & eltrans, 
			   FlatVector<Complex> & elvec,
			   LocalHeap & locheap) const 
    {
      FlatVector<double> rvec(elvec.Size(), locheap);
      AssembleElementVector (fel, eltrans, rvec, locheap);
      // elvec.AssignMemory (rvec.Size(), locheap);
      elvec = rvec;
    }

    virtual void
    AssembleElementVectorIndependent (const FiniteElement & gfel,
				      const BaseSpecificIntegrationPoint & s_sip,
				      const BaseSpecificIntegrationPoint & g_sip,
				      FlatVector<double> & elvec,
				      LocalHeap & locheap,
				      const bool curveint = false) const
    {
      cerr << "AssembleElementVectorIndependent called for base-class!" << endl;
      exit(10);
    }
  
    virtual void
    AssembleElementVectorIndependent (const FiniteElement & gfel,
				      const BaseSpecificIntegrationPoint & s_sip,
				      const BaseSpecificIntegrationPoint & g_sip,
				      FlatVector<Complex> & elvec,
				      LocalHeap & locheap,
				      const bool curveint = false) const
    {
      FlatVector<double> rvec(elvec.Size(), locheap);
      AssembleElementVectorIndependent (gfel, s_sip, g_sip, rvec, locheap,curveint);
      // elvec.AssignMemory (rvec.Size(), locheap);
      elvec = rvec;
    }

  
  };























  class BlockLinearFormIntegrator : public LinearFormIntegrator
  {
    const LinearFormIntegrator & lfi;
    int dim;
    int comp;
  public:
    BlockLinearFormIntegrator (const LinearFormIntegrator & alfi, int adim, int acomp);

    virtual bool BoundaryForm () const
    { return lfi.BoundaryForm(); }


    virtual void 
    AssembleElementVector (const FiniteElement & bfel, 
			   const ElementTransformation & eltrans, 
			   FlatVector<double> & elvec,
			   LocalHeap & locheap) const;
  };





  class ComplexLinearFormIntegrator : public LinearFormIntegrator
  {
    const LinearFormIntegrator & lfi;
    Complex factor;
  public:
    ComplexLinearFormIntegrator (const LinearFormIntegrator & alfi, 
				 Complex afactor)
      : lfi(alfi), factor(afactor)
    { ; }

    virtual bool BoundaryForm () const
    { return lfi.BoundaryForm(); }

    virtual void
    AssembleElementVector (const FiniteElement & fel, 
			   const ElementTransformation & eltrans, 
			   FlatVector<double> & elvec,
			   LocalHeap & locheap) const
    {
      throw Exception ("ComplexLinearFormIntegrator: cannot assemble double vector");
    }

    virtual void
    AssembleElementVector (const FiniteElement & fel, 
			   const ElementTransformation & eltrans, 
			   FlatVector<Complex> & elvec,
			   LocalHeap & locheap) const
    {
      FlatVector<Complex> rvec(elvec.Size(), locheap);
      lfi.AssembleElementVector (fel, eltrans, rvec, locheap);
      // elvec.AssignMemory (rvec.Size(), locheap);
      elvec = factor * rvec;
    }  


    virtual void
    AssembleElementVectorIndependent (const FiniteElement & gfel, 
				      const BaseSpecificIntegrationPoint & s_sip,
				      const BaseSpecificIntegrationPoint & g_sip,
				      FlatVector<double> & elvec,
				      LocalHeap & locheap,
				      const bool curveint = false) const
    {
      throw Exception ("ComplexLinearFormIntegrator: cannot assemble double vector");
    }
  

    virtual void
    AssembleElementVectorIndependent (const FiniteElement & gfel, 
				      const BaseSpecificIntegrationPoint & s_sip,
				      const BaseSpecificIntegrationPoint & g_sip,
				      FlatVector<Complex> & elvec,
				      LocalHeap & locheap,
				      const bool curveint = false) const
    { 
      FlatVector<double> rvec;

      lfi.AssembleElementVectorIndependent (gfel, s_sip, g_sip,
					    rvec, locheap, curveint);
      elvec.AssignMemory (rvec.Size(), locheap);
      elvec = factor * rvec;
    }




    virtual string Name () const
    {
      return
	string ("ComplexIntegrator (") + lfi.Name() +
	string (")");
    }

  };




  class CompoundLinearFormIntegrator : public LinearFormIntegrator
  {
    const LinearFormIntegrator & lfi;
    int comp;
  public:
    CompoundLinearFormIntegrator (const LinearFormIntegrator & alfi, int acomp)
      : lfi(alfi), comp(acomp) { ; }

    virtual bool BoundaryForm () const
    { return lfi.BoundaryForm(); }


    virtual void 
    AssembleElementVector (const FiniteElement & bfel, 
			   const ElementTransformation & eltrans, 
			   FlatVector<double> & elvec,
			   LocalHeap & locheap) const
    {
      const CompoundFiniteElement & fel =
	dynamic_cast<const CompoundFiniteElement&> (bfel);

      FlatVector<double> vec1(fel[comp].GetNDof(), locheap);
      lfi.AssembleElementVector (fel[comp], eltrans, vec1, locheap);
    
      elvec.AssignMemory (fel.GetNDof(), locheap);
      elvec = 0;

      int base = 0;
      for (int i = 0; i < comp; i++)
	base += fel[i].GetNDof();

      for (int i = 0; i < vec1.Size(); i++)
	elvec(base+i) = vec1(i);
    }  


    virtual void 
    AssembleElementVector (const FiniteElement & bfel, 
			   const ElementTransformation & eltrans, 
			   FlatVector<Complex> & elvec,
			   LocalHeap & locheap) const
    {
      const CompoundFiniteElement & fel =
	dynamic_cast<const CompoundFiniteElement&> (bfel);

      FlatVector<Complex> vec1(fel[comp].GetNDof(), locheap);
      lfi.AssembleElementVector (fel[comp], eltrans, vec1, locheap);
    
      elvec.AssignMemory (fel.GetNDof(), locheap);
      elvec = 0;

      int base = 0;
      for (int i = 0; i < comp; i++)
	base += fel[i].GetNDof();

      for (int i = 0; i < vec1.Size(); i++)
	elvec(base+i) = vec1(i);
    }  

    virtual void
    AssembleElementVectorIndependent (const FiniteElement & gfel,
				      const BaseSpecificIntegrationPoint & s_sip,
				      const BaseSpecificIntegrationPoint & g_sip,
				      FlatVector<double> & elvec,
				      LocalHeap & locheap,
				      const bool curveint = false) const
    {
      const CompoundFiniteElement & fel =
	dynamic_cast<const CompoundFiniteElement&> (gfel);

      int i;
      FlatVector<double> vec1;
      lfi.AssembleElementVectorIndependent (fel[comp], s_sip, g_sip, vec1, locheap, curveint);
    
      elvec.AssignMemory (fel.GetNDof(), locheap);
      elvec = 0;

      int base = 0;
      for (i = 0; i < comp; i++)
	base += fel[i].GetNDof();

      for (i = 0; i < vec1.Size(); i++)
	elvec(base+i) = vec1(i);
    }
    virtual void
    AssembleElementVectorIndependent (const FiniteElement & gfel,
				      const BaseSpecificIntegrationPoint & s_sip,
				      const BaseSpecificIntegrationPoint & g_sip,
				      FlatVector<Complex> & elvec,
				      LocalHeap & locheap,
				      const bool curveint = false) const
    {
      const CompoundFiniteElement & fel =
	dynamic_cast<const CompoundFiniteElement&> (gfel);

      int i;
      FlatVector<Complex> vec1;
      lfi.AssembleElementVectorIndependent (fel[comp], s_sip, g_sip, vec1, locheap, curveint);
    
      elvec.AssignMemory (fel.GetNDof(), locheap);
      elvec = 0;

      int base = 0;
      for (i = 0; i < comp; i++)
	base += fel[i].GetNDof();

      for (i = 0; i < vec1.Size(); i++)
	elvec(base+i) = vec1(i);
    }

  


    virtual string Name () const
    {
      return
	string ("CompoundIntegrator (") + lfi.Name() +
	string (")");
    }
  };









  /// container for all integrators
  class Integrators
  {
  public:

    /// description of integrator
    class IntegratorInfo
    {
    public:
      string name;
      int spacedim;
      int numcoeffs;
      Integrator* (*creator)(Array<CoefficientFunction*> &);
    
      IntegratorInfo (const string & aname,
		      int aspacedim,
		      int anumcoffs,		      
		      Integrator* (*acreator)(Array<CoefficientFunction*>&));
    };
  

    Array<IntegratorInfo*> bfis;
    Array<IntegratorInfo*> lfis;
  
  public:
    ///
    Integrators();
    ///
    ~Integrators();  
    ///
    void AddBFIntegrator (const string & aname, int aspacedim, int anumcoeffs,
			  Integrator* (*acreator)(Array<CoefficientFunction*>&));
    ///
    void AddLFIntegrator (const string & aname, int aspacedim, int anumcoeffs,
			  Integrator* (*acreator)(Array<CoefficientFunction*>&));
  
    ///
    const Array<IntegratorInfo*> & GetBFIs() const { return bfis; }
    ///
    const IntegratorInfo * GetBFI(const string & name, int dim) const;
    ///
    BilinearFormIntegrator * CreateBFI(const string & name, int dim,
				       Array<CoefficientFunction*> & coeffs) const;

    ///
    const Array<IntegratorInfo*> & GetLFIs() const { return lfis; }
    ///
    const IntegratorInfo * GetLFI(const string & name, int dim) const;
    ///
    LinearFormIntegrator * CreateLFI(const string & name, int dim,
				     Array<CoefficientFunction*> & coeffs) const;

    ///
    void Print (ostream & ost) const;
  };

  /// 
  extern Integrators & GetIntegrators ();

}

#endif
