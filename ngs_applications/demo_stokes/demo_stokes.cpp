/*

Discretization of Stokes equation

-nu Laplace u + grad p = f
div u = 0

 */


#include <solve.hpp>

using namespace ngsolve;


/*
  Build one finite element space for the three fields  u_x, u_y, p.
  Taylor-Hood element: P2 for the velocities u, and P1 for the pressure
*/

class FESpaceStokes : public CompoundFESpace
{
public:
  FESpaceStokes (const MeshAccess & ama, 		   
		 const ARRAY<const FESpace*> & aspaces, 
		 const Flags & flags)
    : CompoundFESpace (ama, aspaces, flags)
  { ; }
  
  virtual ~FESpaceStokes () { ; }
  
  static FESpace * Create (const MeshAccess & ma, const Flags & flags)
  {
    cout << "Create Taylor-Hood FE Space" << endl;

    // space has 3 components
    ARRAY<const FESpace*> spaces(3);

    int order = int (flags.GetNumFlag ("order", 2));
    // if (order < 2)
    // throw Exception ("Taylor-Hood elements need order 2 or higher");

    Flags uflags;
    uflags.SetFlag ("order", order);
    uflags.SetFlag ("orderinner", order+1);
    spaces[0] = new H1HighOrderFESpace (ma, uflags);
    spaces[1] = new H1HighOrderFESpace (ma, uflags);
    
    // uflags.SetFlag ("order", order);
    // spaces[2] = new H1HighOrderFESpace (ma, uflags);

    uflags.SetFlag ("order", order-1);
    spaces[2] = new L2HighOrderFESpace (ma, uflags);
    
    FESpaceStokes * fes = new FESpaceStokes (ma, spaces, flags);
    return fes;
  }
  
  virtual string GetClassName () const { return "Demo-StokesFESpace"; }
};



/*
  The weak form is written as

  \int (Bv)^T D Bu dx, 

  where B is a differential operator, and D is a matrix
*/

class DiffOpStokes : public DiffOp<DiffOpStokes>
{
  // 5 components:
  // du1/dx, du1/dy, du2/dx, du2/dy, p

public:
  enum { DIM = 1 };          // just one copy of the spaces
  enum { DIM_SPACE = 2 };    // 2D space
  enum { DIM_ELEMENT = 2 };  // 2D elements (in contrast to 1D boundary elements)
  enum { DIM_DMAT = 5 };     // D-matrix is 5x5
  enum { DIFFORDER = 0 };    // minimal differential order (to determine integration order)
  

  /*
    Computes the 5 x N matrix consisting of B_j(phi_i).
    The phi_i, with j = 0,...N-1 are the shape functions,
    and j=0...4 are the 5 components of the diff-op:

 
    The arguments are generic.
    bfel  must be a proper finite element, i.e., a compound element consiting of 3 scalar parts.
    sip  is the specific integration point, containing the Jacobi matrix of the mapping
    mat  is a matrix. Can be a general dense matrix, or a matrix of fixed height 5 (thus generic)
  */

  template <typename FEL, typename SIP, typename MAT>
  static void GenerateMatrix (const FEL & bfel, const SIP & sip,
			      MAT & mat, LocalHeap & lh)
  {
    // must get the right elements, otherwise an exception is thrown.

    const CompoundFiniteElement & cfel = 
      dynamic_cast<const CompoundFiniteElement&> (bfel);

    // a scalar H1 element
    const NodalFiniteElement<2> & fel_u = 
      dynamic_cast<const NodalFiniteElement<2>&> (cfel[0]);
    const NodalFiniteElement<2> & fel_p = 
      dynamic_cast<const NodalFiniteElement<2>&> (cfel[2]);
    
    int nd_u = fel_u.GetNDof();
    int nd_p = fel_p.GetNDof();

    // transformation of derivatives from reference element to general element:
    FlatMatrix<> gradu(2, nd_u, lh);
    gradu = Trans (sip.GetJacobianInverse ()) * 
      Trans (fel_u.GetDShape(sip.IP(), lh));
    
    // the shape functions of the pressure
    FlatVector<> vecp(nd_p, lh);
    vecp = fel_p.GetShape (sip.IP(), lh);

    // the first nd_u shape functions belong to u_x, the next nd_u belong to u_y:
    mat = 0;
    for (int j = 0; j < nd_u; j++)
      {
	mat(0,j) = gradu(0,j);
	mat(1,j) = gradu(1,j);
	mat(2,nd_u+j) = gradu(0,j);
	mat(3,nd_u+j) = gradu(1,j);
      }

    // ... and finally nd_p shape functions for the pressure:
    for (int j = 0; j < nd_p; j++)
      mat(4, 2*nd_u+j) = vecp(j);
  }
};



/*
  The 5x5 coefficient matrix:

  nu 0  0  0  1
  0  nu 0  0  0
  0  0  nu 0  0
  0  0  0  nu 1
  1  0  0  1  0

*/

class StokesDMat : public DMatOp<StokesDMat>
{
  CoefficientFunction * nu;
public:

  enum { DIM_DMAT = 5 };

  StokesDMat (CoefficientFunction * anu) : nu(anu) { ; }
  
  template <typename FEL, typename SIP, typename MAT>
  void GenerateMatrix (const FEL & fel, const SIP & sip,
		       MAT & mat, LocalHeap & lh) const
  {
    mat = 0;

    // (nu  grud u, grad v)
    double val = nu -> Evaluate (sip);
    for (int i = 0; i < 4; i++)
      mat(i, i) = val;

    // (du1/dx+du2/dy, p)
    mat(4,0) = mat(0,4) = 1;
    mat(4,3) = mat(3,4) = 1;
  }  
};


/*
  Combine differential operator and D-matrix to an integrator
*/
class StokesIntegrator 
  : public T_BDBIntegrator<DiffOpStokes, StokesDMat, FiniteElement>
{
public:
  StokesIntegrator (CoefficientFunction * coeff)
    :  T_BDBIntegrator<DiffOpStokes, StokesDMat, FiniteElement>
  (StokesDMat (coeff))
  { ; }
  
  static Integrator * Create (ARRAY<CoefficientFunction*> & coeffs)
  {
    return new StokesIntegrator (coeffs[0]);
  }
  
  virtual string Name () const { return "Stokes"; }
};






/*
  Evaluate (u_x, u_y) 
 */
class DiffOpIdU : public DiffOp<DiffOpIdU>
{
  // 2 components:
  // u1 u2

public:
  enum { DIM = 1 };
  enum { DIM_SPACE = 2 };
  enum { DIM_ELEMENT = 2 };
  enum { DIM_DMAT = 2 };
  enum { DIFFORDER = 0 };
  
  template <typename FEL, typename SIP, typename MAT>
  static void GenerateMatrix (const FEL & bfel, const SIP & sip,
			      MAT & mat, LocalHeap & lh)
  {
    const CompoundFiniteElement & cfel = 
      dynamic_cast<const CompoundFiniteElement&> (bfel);
    const NodalFiniteElement<2> & fel_u = 
      dynamic_cast<const NodalFiniteElement<2>&> (cfel[0]);
    
    int nd_u = fel_u.GetNDof();

    FlatVector<> vecu(nd_u, lh);
    vecu = fel_u.GetShape (sip.IP(), lh);

    mat = 0;
    for (int j = 0; j < nd_u; j++)
      {
	mat(0,j) = vecu(j);
	mat(1,nd_u+j) = vecu(j);
      }
  }
};


class StokesUIntegrator 
  : public T_BDBIntegrator<DiffOpIdU, DiagDMat<2>, FiniteElement>
{
public:
  ///
  StokesUIntegrator ()
    :  T_BDBIntegrator<DiffOpIdU, DiagDMat<2>, FiniteElement>
  (DiagDMat<2> (new ConstantCoefficientFunction(1)))
  { ; }
  
  static Integrator * Create (ARRAY<CoefficientFunction*> & coeffs)
  {
    return new StokesUIntegrator ();
  }
  
  ///
  virtual string Name () const { return "Stokes IdU"; }
};




namespace 
#ifdef MACOS
demo_stokes_cpp
#endif 
{
  class Init
  { 
  public: 
    Init ();
  };
  
  Init::Init()
  {
    GetFESpaceClasses().AddFESpace ("stokes", FESpaceStokes::Create);

    GetIntegrators().AddBFIntegrator ("stokes", 2, 1,
				      StokesIntegrator::Create);
    GetIntegrators().AddBFIntegrator ("stokesu", 2, 0,
				      StokesUIntegrator::Create);
  }

#ifdef MACOS
  static
#endif  
  Init init;
}
