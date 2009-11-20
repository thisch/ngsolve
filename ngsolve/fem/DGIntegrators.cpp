/*********************************************************************/
/* File:   DGIntegrators.cpp                                         */
/* Author: Christoph Lehrenfeld                                      */
/* Date:   17. Nov. 2009                                             */
/*********************************************************************/

#include <fem.hpp>

namespace ngfem
{
  /** 
      DG for scalar Laplace
      //TODO: Convection-Integrators
  */
  
  using namespace ngfem;

  namespace DG_FORMULATIONS{
    enum DGTYPE{
      IP,
      NIPG,
      BO
    };
  }
  
  template <int D, DG_FORMULATIONS::DGTYPE dgtype>
  class DGInnerFacet_LaplaceIntegrator : public FacetBilinearFormIntegrator
  {
  protected:
    double alpha;   // interior penalyty
    CoefficientFunction *coef_lam;
  public:
    DGInnerFacet_LaplaceIntegrator (Array<CoefficientFunction*> & coeffs) 
      : FacetBilinearFormIntegrator(coeffs)
    { 
      coef_lam  = coeffs[0];
      if (dgtype!=DG_FORMULATIONS::BO)
	alpha = coeffs[1] -> EvaluateConst();
    }

    virtual ~DGInnerFacet_LaplaceIntegrator () { ; }
    
    virtual bool BoundaryForm () const 
    { return 0; }
    
    static Integrator * Create (Array<CoefficientFunction*> & coeffs)
    {
      return new DGInnerFacet_LaplaceIntegrator (coeffs);
    }
    
    virtual void AssembleElementMatrix (const FiniteElement & fel,
                                        const ElementTransformation & eltrans, 
                                        FlatMatrix<double> & elmat,
                                        LocalHeap & lh) const
    {
      throw Exception("DGIP - not implemented!");
    }
    
    virtual void AssembleFacetMatrix (const FiniteElement & volumefel1, int LocalFacetNr1,
			 const ElementTransformation & eltrans1, FlatArray<int> & ElVertices1,
			 const FiniteElement & volumefel2, int LocalFacetNr2,
			 const ElementTransformation & eltrans2, FlatArray<int> & ElVertices2,
                         FlatMatrix<double> & elmat,
                         LocalHeap & lh) const
    {
      static int timer = NgProfiler::CreateTimer ("DGFacet laplace boundary");
      NgProfiler::RegionTimer reg (timer);
      const ScalarFiniteElement<D> * fel1_l2 = 
        dynamic_cast<const ScalarFiniteElement<D>*> (&volumefel1);
      ELEMENT_TYPE eltype1 = volumefel1.ElementType();
      int nd1 = fel1_l2->GetNDof();

      const ScalarFiniteElement<D> * fel2_l2 = NULL;
      ELEMENT_TYPE eltype2 = eltype1;
      int nd2 = 0;

      double maxorder = fel1_l2->Order();

      if (LocalFacetNr2!=-1){ //two elements neighbouring the facet? or boundary? 
        fel2_l2 = dynamic_cast<const ScalarFiniteElement<D>*> (&volumefel2);
	eltype2 = volumefel2.ElementType();
        nd2 = fel2_l2->GetNDof();
	maxorder = max(fel1_l2->Order(),fel2_l2->Order());
      }
      
      elmat = 0.0;

      FlatVector<> mat1_shape(nd1, lh);
      FlatVector<> mat1_dudn(nd1, lh);
      FlatVector<> mat2_shape(nd2, lh);
      FlatVector<> mat2_dudn(nd2, lh);
      
      FlatMatrixFixHeight<2> bmat(nd1+nd2, lh);
      FlatMatrixFixHeight<2> dbmat(nd1+nd2, lh);
      Mat<2> dmat;

      FlatMatrixFixWidth<D> dshape(nd1, lh);
      FlatMatrixFixWidth<D> fac_dshape(nd1, lh);
      Facet2ElementTrafo transform1(eltype1,ElVertices1); 
      const NORMAL * normals1 = ElementTopology::GetNormals(eltype1);
      Facet2ElementTrafo transform2(eltype2,ElVertices2); 
      const NORMAL * normals2 = ElementTopology::GetNormals(eltype2);

      HeapReset hr(lh);
      ELEMENT_TYPE etfacet = ElementTopology::GetFacetType (eltype1, LocalFacetNr1);

      Vec<D> normal_ref1, normal_ref2;
      for (int i=0; i<D; i++){
	normal_ref1(i) = normals1[LocalFacetNr1][i];
	normal_ref2(i) = (LocalFacetNr2!=-1) ? normals2[LocalFacetNr2][i] : 0;
      }
      const IntegrationRule & ir_facet =
	SelectIntegrationRule (etfacet, 2*maxorder);
      if (maxorder==0) maxorder=1;
   
      bmat = 0.0;
      for (int l = 0; l < ir_facet.GetNIP(); l++)
	{
	  IntegrationPoint ip1 = transform1(LocalFacetNr1, ir_facet[l]);
	  
	  SpecificIntegrationPoint<D,D> sip1 (ip1, eltrans1, lh);
	  double lam = coef_lam->Evaluate(sip1);

	  Mat<D> jac1 = sip1.GetJacobian();
	  Mat<D> inv_jac1 = sip1.GetJacobianInverse();
	  double det1 = sip1.GetJacobiDet();

	  Vec<D> normal1 = det1 * Trans (inv_jac1) * normal_ref1;       
	  double len1 = L2Norm (normal1);
	  normal1 /= len1;

	  fel1_l2->CalcShape(sip1.IP(), mat1_shape);
	  Vec<D> invjac_normal1 = inv_jac1 * normal1;
	  mat1_dudn = fel1_l2->GetDShape (sip1.IP(), lh) * invjac_normal1;
	  
	  if (LocalFacetNr2!=-1){
	    //TODO orientierung muss irgendwie noch uebergeben werden!
	    IntegrationPoint ip2 = (LocalFacetNr2!=-1) ? transform2(LocalFacetNr2, ir_facet[l]) : ip1;
	    SpecificIntegrationPoint<D,D> sip2 (ip2, eltrans2, lh);
	    double lam2 = coef_lam->Evaluate(sip2);
	    if(abs(lam-lam2)>1e-6){
	      std::cout << "lambda :\t" << lam << "\t=?=\t" << lam2 << std::endl;
	      cout << "POINTS: " << sip1.GetPoint() << "\t" << sip2.GetPoint() << endl;
	    }

	    Mat<D> jac2 = sip2.GetJacobian();
	    Mat<D> inv_jac2 = sip2.GetJacobianInverse();
	    double det2 = sip2.GetJacobiDet();
	    
	    Vec<D> normal2 = det2 * Trans (inv_jac2) * normal_ref2;       
	    double len2 = L2Norm (normal2); 
	    if(abs(len1-len2)>1e-6)
	      std::cout << "len :\t" << len1 << "\t=?=\t" << len2 << std::endl;
	    normal2 /= len2;
	    Vec<D> invjac_normal2;;
	    fel2_l2->CalcShape(sip2.IP(), mat2_shape);
	    invjac_normal2 = inv_jac2 * normal2;
	    mat2_dudn = fel2_l2->GetDShape (sip2.IP(), lh) * invjac_normal2;
	    
	    bmat.Row(0).Range (0   , nd1)   = 0.5 * mat1_dudn;	    
	    bmat.Row(0).Range (nd1   , nd1+nd2)   = -0.5 * mat2_dudn;
	    bmat.Row(1).Range (0   , nd1)   = mat1_shape;
	    bmat.Row(1).Range (nd1   , nd1+nd2)   = -mat2_shape;


// 	      NgProfiler::AddFlops (timer2, XYZ);
	  }else{
	    bmat.Row(0).Range (0   , nd1)   = mat1_dudn;
	    bmat.Row(1).Range (0   , nd1)   = mat1_shape;
// 	      NgProfiler::AddFlops (timer2, XYZ);
	  }
	  dmat(0,0) = 0;
	  dmat(1,0) = -1;
	  switch (dgtype){
	    case DG_FORMULATIONS::BO:
	      dmat(0,1) = 1;
	      dmat(1,1) = 0;
	      break;
	    case DG_FORMULATIONS::NIPG:
	      dmat(0,1) = 1; 
	      dmat(1,1) = alpha * sqr (maxorder) * (len1/det1);
	      break;
	    case DG_FORMULATIONS::IP:
	    default:
	      dmat(0,1) = -1; 
	      dmat(1,1) = alpha * sqr (maxorder) * (len1/det1);
	      break;	      
	  }
	  dmat *= lam * len1 * ir_facet[l].Weight();
	  dbmat = dmat * bmat;
	  elmat += Trans (bmat) * dbmat;
	}
	if (LocalFacetNr2==-1) elmat=0.0;
      }
  };

  
  template <int D, DG_FORMULATIONS::DGTYPE dgtype>
  class DGBoundaryFacet_LaplaceIntegrator : public FacetBilinearFormIntegrator
  {
  protected:
    double alpha;   // interior penalyty
    CoefficientFunction *coef_lam;
    CoefficientFunction *coef_rob;
  public:
    DGBoundaryFacet_LaplaceIntegrator (Array<CoefficientFunction*> & coeffs) 
      : FacetBilinearFormIntegrator(coeffs)
    { 
      coef_lam  = coeffs[0];
      coef_rob  = coeffs[1];
      if (dgtype!=DG_FORMULATIONS::BO)
	alpha = coeffs[2] -> EvaluateConst();
    }
    
    virtual bool BoundaryForm () const 
    { return 1; }
    
    virtual ~DGBoundaryFacet_LaplaceIntegrator () { ; }

    static Integrator * Create (Array<CoefficientFunction*> & coeffs)
    {
      return new DGBoundaryFacet_LaplaceIntegrator (coeffs);
    }
    
    virtual void AssembleElementMatrix (const FiniteElement & fel,
                                        const ElementTransformation & eltrans, 
                                        FlatMatrix<double> & elmat,
                                        LocalHeap & lh) const
    {
      throw Exception("DGIP - not implemented!");
    }
    
    virtual void AssembleFacetMatrix (const FiniteElement & volumefel, int LocalFacetNr,
			 const ElementTransformation & eltrans, FlatArray<int> & ElVertices,
 			 const ElementTransformation & seltrans,
                         FlatMatrix<double> & elmat,
                         LocalHeap & lh) const
    {
      static int timer = NgProfiler::CreateTimer ("DGFacet laplace boundary");
      NgProfiler::RegionTimer reg (timer);
      const ScalarFiniteElement<D> * fel1_l2 = 
        dynamic_cast<const ScalarFiniteElement<D>*> (&volumefel);
      ELEMENT_TYPE eltype1 = volumefel.ElementType();
      int nd1 = fel1_l2->GetNDof();

      const ScalarFiniteElement<D> * fel2_l2 = NULL;
      ELEMENT_TYPE eltype2 = eltype1;
      int nd2 = 0;

      double maxorder = fel1_l2->Order();

      
      elmat = 0.0;

      FlatVector<> mat1_shape(nd1, lh);
      FlatVector<> mat1_dudn(nd1, lh);
      
      FlatMatrixFixHeight<2> bmat(nd1+nd2, lh);
      FlatMatrixFixHeight<2> dbmat(nd1+nd2, lh);
      Mat<2> dmat;

      FlatMatrixFixWidth<D> dshape(nd1, lh);
      FlatMatrixFixWidth<D> fac_dshape(nd1, lh);
      Facet2ElementTrafo transform1(eltype1,ElVertices); 
      const NORMAL * normals1 = ElementTopology::GetNormals(eltype1);
      
      HeapReset hr(lh);
      ELEMENT_TYPE etfacet = ElementTopology::GetFacetType (eltype1, LocalFacetNr);

      Vec<D> normal_ref1, normal_ref2;
      for (int i=0; i<D; i++){
	normal_ref1(i) = normals1[LocalFacetNr][i];
      }
      const IntegrationRule & ir_facet =
	SelectIntegrationRule (etfacet, 2*maxorder);
      if (maxorder==0) maxorder=1;

      bmat = 0.0;
      for (int l = 0; l < ir_facet.GetNIP(); l++)
	{
	  IntegrationPoint ip1 = transform1(LocalFacetNr, ir_facet[l]);
	  
	  SpecificIntegrationPoint<D,D> sip1 (ip1, eltrans, lh);
	  double lam = coef_lam->Evaluate(sip1);

	  SpecificIntegrationPoint<D-1,D> sips (ip1, seltrans, lh);
	  double rob = coef_rob->Evaluate(sips);	  
	  
	  Mat<D> jac1 = sip1.GetJacobian();
	  Mat<D> inv_jac1 = sip1.GetJacobianInverse();
	  double det1 = sip1.GetJacobiDet();

	  Vec<D> normal1 = det1 * Trans (inv_jac1) * normal_ref1;       
	  double len1 = L2Norm (normal1);
	  normal1 /= len1;

	  fel1_l2->CalcShape(sip1.IP(), mat1_shape);
	  Vec<D> invjac_normal1 = inv_jac1 * normal1;
	  mat1_dudn = fel1_l2->GetDShape (sip1.IP(), lh) * invjac_normal1;
	  
	  bmat.Row(0).Range (0   , nd1)   = mat1_dudn;
	  bmat.Row(1).Range (0   , nd1)   = mat1_shape;
	  
	  dmat(0,0) = 0;
	  dmat(1,0) = -1;
	  
	  switch (dgtype){
	    case DG_FORMULATIONS::BO:
	      dmat(0,1) = 1;
	      dmat(1,1) = 0;
	      break;
	    case DG_FORMULATIONS::NIPG:
	      dmat(0,1) = 1; 
	      dmat(1,1) = alpha * sqr (maxorder) * (len1/det1);
	      break;
	    case DG_FORMULATIONS::IP:
	    default:
	      dmat(0,1) = -1; 
	      dmat(1,1) = alpha * sqr (maxorder) * (len1/det1);
	      break;	      
	  }
	  dmat *= rob * lam * len1 * ir_facet[l].Weight();
	  dbmat = dmat * bmat;
	  elmat += Trans (bmat) * dbmat;
	}
      }
  };
  
  
  template <int D, DG_FORMULATIONS::DGTYPE dgtype>
  class DGFacet_DirichletBoundaryIntegrator : public FacetLinearFormIntegrator
  {
  protected:
    double alpha;   // interior penalyty
    CoefficientFunction *coef_lam;
    CoefficientFunction *coef_rob;
  public:
    DGFacet_DirichletBoundaryIntegrator (Array<CoefficientFunction*> & coeffs) 
      : FacetLinearFormIntegrator(coeffs)
    { 
      coef_lam  = coeffs[0];
      coef_rob  = coeffs[1];
      alpha = coeffs[2] -> EvaluateConst();
    }
    
    virtual bool BoundaryForm () const 
    { return 1; }
    
    virtual ~DGFacet_DirichletBoundaryIntegrator () { ; }

    static Integrator * Create (Array<CoefficientFunction*> & coeffs)
    {
      return new DGFacet_DirichletBoundaryIntegrator (coeffs);
    }
    
    virtual void AssembleElementVector (const FiniteElement & fel,
                                        const ElementTransformation & eltrans, 
                                        FlatVector<double> & elvec,
                                        LocalHeap & lh) const
    {
      throw Exception("DGIP - not implemented!");
    }
    
    virtual void AssembleFacetVector (const FiniteElement & volumefel, int LocalFacetNr,
			 const ElementTransformation & eltrans, FlatArray<int> & ElVertices,
			 const ElementTransformation & seltrans,
                         FlatVector<double> & elvec, LocalHeap & lh) const
    {
      static int timer = NgProfiler::CreateTimer ("DGIPFacet laplace boundary");

      NgProfiler::RegionTimer reg (timer);
      const ScalarFiniteElement<D> * fel1_l2 = 
        dynamic_cast<const ScalarFiniteElement<D>*> (&volumefel);
      ELEMENT_TYPE eltype1 = volumefel.ElementType();
      int nd1 = fel1_l2->GetNDof();
      elvec = 0.0;

      FlatVector<> mat1_shape(nd1, lh);
      FlatVector<> mat1_dudn(nd1, lh);
      
      Facet2ElementTrafo transform1(eltype1,ElVertices); 

      const NORMAL * normals1 = ElementTopology::GetNormals(eltype1);

      HeapReset hr(lh);
      ELEMENT_TYPE etfacet = ElementTopology::GetFacetType (eltype1, LocalFacetNr);

      Vec<D> normal_ref1;
      Vec<D> dvec(2);
      dvec=0.0;
      for (int i=0; i<D; i++){
	normal_ref1(i) = normals1[LocalFacetNr][i];
      }
      double maxorder = fel1_l2->Order();
      const IntegrationRule & ir_facet =
	SelectIntegrationRule (etfacet, 2*maxorder);
      if (maxorder==0) maxorder=1;

      for (int l = 0; l < ir_facet.GetNIP(); l++)
	{
	  IntegrationPoint ip1 = transform1(LocalFacetNr, ir_facet[l]);
	  
	  SpecificIntegrationPoint<D,D> sip1 (ip1, eltrans, lh);
	  double lam = coef_lam->Evaluate(sip1);

	  SpecificIntegrationPoint<D-1,D> sips (ip1, seltrans, lh);
	  double rob = coef_rob->Evaluate(sips);
	  
	  Mat<D> jac1 = sip1.GetJacobian();
	  Mat<D> inv_jac1 = sip1.GetJacobianInverse();
	  double det1 = sip1.GetJacobiDet();

	  Vec<D> normal1 = det1 * Trans (inv_jac1) * normal_ref1;       
	  double len1 = L2Norm (normal1);
	  normal1 /= len1;

	  fel1_l2->CalcShape(sip1.IP(), mat1_shape);

	  Vec<D> invjac_normal1 = inv_jac1 * normal1;

	  mat1_dudn = fel1_l2->GetDShape (sip1.IP(), lh) * invjac_normal1;
	  
	  
	  switch (dgtype){
	    case DG_FORMULATIONS::BO:
	      dvec(0) = 1; 
	      dvec(1) = 0;
	      break;
	    case DG_FORMULATIONS::NIPG:
	      dvec(0) = 1; 
	      dvec(1) = alpha * sqr (maxorder) * (len1/det1);
	      break;
	    case DG_FORMULATIONS::IP:
	    default:
	      dvec(0) = -1;
	      dvec(1) = alpha* sqr (maxorder) * (len1/det1);
	      break;	      
	  }	  
	  
	  double fac = len1*ir_facet[l].Weight()*lam*rob;
	  elvec += (fac * dvec(0))*mat1_dudn;
	  elvec += (fac * dvec(1) ) * mat1_shape;
	}
      }
  };

 
    template <int D>
  class DGFacet_NeumannBoundaryIntegrator : public FacetLinearFormIntegrator
  {
  protected:
    CoefficientFunction *coef_lam;
    CoefficientFunction *coef_rob;
  public:
    DGFacet_NeumannBoundaryIntegrator (Array<CoefficientFunction*> & coeffs) 
      : FacetLinearFormIntegrator(coeffs)
    { 
      coef_lam  = coeffs[0];
      coef_rob  = coeffs[1];
    }
    
    virtual bool BoundaryForm () const 
    { return 1; }
    
    virtual ~DGFacet_NeumannBoundaryIntegrator () { ; }

    static Integrator * Create (Array<CoefficientFunction*> & coeffs)
    {
      return new DGFacet_NeumannBoundaryIntegrator (coeffs);
    }
    
    virtual void AssembleElementVector (const FiniteElement & fel,
                                        const ElementTransformation & eltrans, 
                                        FlatVector<double> & elvec,
                                        LocalHeap & lh) const
    {
      throw Exception("DGIP - not implemented!");
    }
    
    virtual void AssembleFacetVector (const FiniteElement & volumefel, int LocalFacetNr,
			 const ElementTransformation & eltrans, FlatArray<int> & ElVertices,
			 const ElementTransformation & seltrans,
                         FlatVector<double> & elvec, LocalHeap & lh) const
    {
      static int timer = NgProfiler::CreateTimer ("DGIPFacet laplace boundary");

      NgProfiler::RegionTimer reg (timer);
      const ScalarFiniteElement<D> * fel1_l2 = 
        dynamic_cast<const ScalarFiniteElement<D>*> (&volumefel);
      ELEMENT_TYPE eltype1 = volumefel.ElementType();
      int nd1 = fel1_l2->GetNDof();
      elvec = 0.0;

      FlatVector<> mat1_shape(nd1, lh);
      FlatVector<> mat1_dudn(nd1, lh);
      
      Facet2ElementTrafo transform1(eltype1,ElVertices); 

      const NORMAL * normals1 = ElementTopology::GetNormals(eltype1);

      HeapReset hr(lh);
      ELEMENT_TYPE etfacet = ElementTopology::GetFacetType (eltype1, LocalFacetNr);

      Vec<D> normal_ref1;
      for (int i=0; i<D; i++){
	normal_ref1(i) = normals1[LocalFacetNr][i];
      }
      double maxorder = fel1_l2->Order();
      const IntegrationRule & ir_facet =
	SelectIntegrationRule (etfacet, 2*maxorder);
      if (maxorder==0) maxorder=1;

      for (int l = 0; l < ir_facet.GetNIP(); l++)
	{
	  IntegrationPoint ip1 = transform1(LocalFacetNr, ir_facet[l]);
	  
	  SpecificIntegrationPoint<D,D> sip1 (ip1, eltrans, lh);
	  double lam = coef_lam->Evaluate(sip1);

	  SpecificIntegrationPoint<D-1,D> sips (ip1, seltrans, lh);
	  double rob = coef_rob->Evaluate(sips);
	  
	  Mat<D> jac1 = sip1.GetJacobian();
	  Mat<D> inv_jac1 = sip1.GetJacobianInverse();
	  double det1 = sip1.GetJacobiDet();

	  Vec<D> normal1 = det1 * Trans (inv_jac1) * normal_ref1;       
	  double len1 = L2Norm (normal1);
	  normal1 /= len1;

	  fel1_l2->CalcShape(sip1.IP(), mat1_shape);

	  Vec<D> invjac_normal1 = inv_jac1 * normal1;

	  double fac = len1*ir_facet[l].Weight()*lam*rob;
	  elvec += fac *  mat1_shape;
	}
      }
  };

   
  
  
  class Init
  { 
  public: 
    Init ();
  };
  
  Init::Init()
  {
    cout << "DG-IP module (2D trigs only) is loaded" << endl;
    // (symmetric) Interior Penalty method: consistent and stable (and adjoint consistent)
    GetIntegrators().AddBFIntegrator ("DGIP_innfac_laplace", 2, 2,
				      DGInnerFacet_LaplaceIntegrator<2,DG_FORMULATIONS::IP>::Create);
    GetIntegrators().AddBFIntegrator ("DGIP_innfac_laplace", 3, 2,
				      DGInnerFacet_LaplaceIntegrator<3,DG_FORMULATIONS::IP>::Create);
    GetIntegrators().AddBFIntegrator ("DGIP_bndfac_laplace", 2, 3,
				      DGBoundaryFacet_LaplaceIntegrator<2,DG_FORMULATIONS::IP>::Create);
    GetIntegrators().AddBFIntegrator ("DGIP_bndfac_laplace", 3, 3,
				      DGBoundaryFacet_LaplaceIntegrator<3,DG_FORMULATIONS::IP>::Create);
    GetIntegrators().AddLFIntegrator ("DGIP_bndfac_dir", 2, 3,
				      DGFacet_DirichletBoundaryIntegrator<2,DG_FORMULATIONS::IP>::Create);
    GetIntegrators().AddLFIntegrator ("DGIP_bndfac_dir", 3, 3,
				      DGFacet_DirichletBoundaryIntegrator<3,DG_FORMULATIONS::IP>::Create);
    GetIntegrators().AddLFIntegrator ("DGIP_bndfac_neumann", 2, 2,
				      DGFacet_NeumannBoundaryIntegrator<2>::Create);
    GetIntegrators().AddLFIntegrator ("DGIP_bndfac_neumann", 3, 2,
				      DGFacet_NeumannBoundaryIntegrator<3>::Create);
				      
/*    // nonsymmteric Interior Penalty method: consistent and stable

    GetIntegrators().AddBFIntegrator ("DGNIPG_innerfac_laplace", 2, 2,
				      DGInnerFacet_LaplaceIntegrator<2,DG_FORMULATIONS::NIPG>::Create);
    GetIntegrators().AddBFIntegrator ("DGNIPG_innerfac_laplace", 3, 2,
				      DGInnerFacet_LaplaceIntegrator<3,DG_FORMULATIONS::NIPG>::Create);
    GetIntegrators().AddBFIntegrator ("DGNIPG_boundfac_laplace", 2, 3,
				      DGBoundaryFacet_LaplaceIntegrator<2,DG_FORMULATIONS::NIPG>::Create);
    GetIntegrators().AddBFIntegrator ("DGNIPG_boundfac_laplace", 3, 3,
				      DGBoundaryFacet_LaplaceIntegrator<3,DG_FORMULATIONS::NIPG>::Create);
    GetIntegrators().AddLFIntegrator ("DGNIPG_boundfac_dir", 2, 3,
				      DGFacet_DirichletBoundaryIntegrator<2,DG_FORMULATIONS::NIPG>::Create);
    GetIntegrators().AddLFIntegrator ("DGNIPG_boundfac_dir", 3, 3,
				      DGFacet_DirichletBoundaryIntegrator<3,DG_FORMULATIONS::NIPG>::Create);
    GetIntegrators().AddLFIntegrator ("DGNIPG_boundfac_neumann", 2, 2,
				      DGFacet_NeumannBoundaryIntegrator<2>::Create);
    GetIntegrators().AddLFIntegrator ("DGNIPG_boundfac_neumann", 3, 2,
				      DGFacet_NeumannBoundaryIntegrator<3>::Create);
    // Baumann-Oden method: (only) consistent

    GetIntegrators().AddBFIntegrator ("DGBO_innerfac_laplace", 2, 2,
				      DGInnerFacet_LaplaceIntegrator<2,DG_FORMULATIONS::BO>::Create);
    GetIntegrators().AddBFIntegrator ("DGBO_innerfac_laplace", 3, 2,
				      DGInnerFacet_LaplaceIntegrator<3,DG_FORMULATIONS::BO>::Create);
    GetIntegrators().AddBFIntegrator ("DGBO_boundfac_laplace", 2, 3,
				      DGBoundaryFacet_LaplaceIntegrator<2,DG_FORMULATIONS::BO>::Create);
    GetIntegrators().AddBFIntegrator ("DGBO_boundfac_laplace", 3, 3,
				      DGBoundaryFacet_LaplaceIntegrator<3,DG_FORMULATIONS::BO>::Create);
    GetIntegrators().AddLFIntegrator ("DGBO_boundfac_dir", 2, 3,
				      DGFacet_DirichletBoundaryIntegrator<2,DG_FORMULATIONS::BO>::Create);
    GetIntegrators().AddLFIntegrator ("DGBO_boundfac_dir", 3, 3,
				      DGFacet_DirichletBoundaryIntegrator<3,DG_FORMULATIONS::BO>::Create);
    GetIntegrators().AddLFIntegrator ("DGBO_boundfac_neumann", 2, 2,
				      DGFacet_NeumannBoundaryIntegrator<2>::Create);
    GetIntegrators().AddLFIntegrator ("DGBO_boundfac_neumann", 3, 2,
				      DGFacet_NeumannBoundaryIntegrator<3>::Create);*/
					 
					 
  }//end of init-constructor
  
  Init init;
}; //end of namespace ngfem