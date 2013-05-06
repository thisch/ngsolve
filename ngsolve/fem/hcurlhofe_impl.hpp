#ifndef FILE_HCURLHOFE_IMPL
#define FILE_HCURLHOFE_IMPL

/*********************************************************************/
/* File:   hcurlhofe.hpp                                             */
/* Author: Sabine Zaglmayr, Joachim Schoeber                         */
/* Date:   20. Maerz 2003                                            */
/*                                                                   */
/* AutoCurl - revision: J. Schoeberl, March 2009                     */
/*********************************************************************/
   
namespace ngfem
{

  // declaration of the shapes ...

  
  template <ELEMENT_TYPE ET> 
  class HCurlHighOrderFE_Shape :  public HCurlHighOrderFE<ET>
  {
  public:
    template<typename Tx, typename TFA>  
    void T_CalcShape (Tx hx[2], TFA & shape) const; 
  };




  
  //------------------------------------------------------------------------
  // HCurlHighOrderSegm
  //------------------------------------------------------------------------

  
  template<> template<typename Tx, typename TFA>  
  void HCurlHighOrderFE_Shape<ET_SEGM> :: T_CalcShape (Tx hx[1], TFA & shape) const
  {
    Tx x = hx[0];
    Tx lam[2] = { x, 1-x };

    ArrayMem<AutoDiff<1>,10> adpol1(order);
	
    INT<2> e = GetEdgeSort (0, vnums);	  
    
    //Nedelec low order edge shape function 
    shape[0] = uDv_minus_vDu<1> (lam[e[0]], lam[e[1]]);

    int p = order_cell[0]; 
    //HO-Edge shapes (Gradient Fields)   
    if(p > 0 && usegrad_cell)
      { 
        LegendrePolynomial::
          EvalScaledMult (p-1, 
                          lam[e[1]]-lam[e[0]], lam[e[0]]+lam[e[1]], 
                          lam[e[0]]*lam[e[1]], adpol1);
        
        for(int j = 0; j < p; j++) 	      
          shape[j+1] = Du<1> (adpol1[j]);
      }
  }
  


  
  //------------------------------------------------------------------------
  // HCurlHighOrderTrig
  //------------------------------------------------------------------------
  
  template<> template<typename Tx, typename TFA>  
  void HCurlHighOrderFE_Shape<ET_TRIG> :: T_CalcShape (Tx hx[2], TFA & shape) const
  {
    Tx x = hx[0], y = hx[1];
    Tx lam[3] = { x, y, 1-x-y };

    ArrayMem<AutoDiff<2>,10> adpol1(order),adpol2(order);	
	
    int ii = 3; 
    for (int i = 0; i < 3; i++)
      {
        INT<2> e = GetEdgeSort (i, vnums);	  

	//Nedelec low order edge shape function 
        shape[i] = uDv_minus_vDu<2> (lam[e[0]], lam[e[1]]);

	int p = order_edge[i]; 
	//HO-Edge shapes (Gradient Fields)   
	if(p > 0 && usegrad_edge[i]) 
	  { 
	    LegendrePolynomial::
	      EvalScaledMult (order_edge[i]-1, 
			      lam[e[1]]-lam[e[0]], lam[e[0]]+lam[e[1]], 
			      lam[e[0]]*lam[e[1]], adpol1);
	    
	    for(int j = 0; j < p; j++) 	      
              shape[ii++] = Du<2> (adpol1[j]);
	  }
      }   
     
    //Inner shapes (Face) 
    int p = order_face[0][0];      
    if(p > 1) 
      {
	INT<4> fav = GetFaceSort (0, vnums);

	AutoDiff<2> xi  = lam[fav[2]]-lam[fav[1]];
	AutoDiff<2> eta = lam[fav[0]]; 

        TrigShapesInnerLegendre::CalcSplitted(p+1, xi, eta, adpol1,adpol2);
	
	// gradients:
	if(usegrad_face[0])
	  for (int j = 0; j < p-1; j++)
	    for (int k = 0; k < p-1-j; k++, ii++)
              shape[ii] = Du<2> (adpol1[j] * adpol2[k]);

	// other combination
	for (int j = 0; j < p-1; j++)
	  for (int k = 0; k < p-1-j; k++, ii++)
            shape[ii] = uDv_minus_vDu<2> (adpol2[k], adpol1[j]);
	
	// rec_pol * Nedelec0 
	for (int j = 0; j < p-1; j++, ii++)
          shape[ii] = wuDv_minus_wvDu<2> (lam[fav[1]], lam[fav[2]], adpol2[j]);
      }
  }


  //------------------------------------------------------------------------
  // HCurlHighOrderQuad
  //------------------------------------------------------------------------
  

  template<> template<typename Tx, typename TFA>  
  void  HCurlHighOrderFE_Shape<ET_QUAD> :: T_CalcShape (Tx hx[2], TFA & shape) const
  {
    Tx x = hx[0], y = hx[1];

    AutoDiff<2> lami[4] = {(1-x)*(1-y),x*(1-y),x*y,(1-x)*y};  
    AutoDiff<2> sigma[4] = {(1-x)+(1-y),x+(1-y),x+y,(1-x)+y};  

    int ii = 4;
    ArrayMem<AutoDiff<2>, 10> pol_xi(order+2), pol_eta(order+2);

    for (int i = 0; i < 4; i++)
      {
	int p = order_edge[i]; 
        INT<2> e = GetEdgeSort (i, vnums);	  
	
	AutoDiff<2> xi  = sigma[e[1]]-sigma[e[0]];
	AutoDiff<2> lam_e = lami[e[0]]+lami[e[1]];  
        Tx bub = 0.25 * lam_e * (1 - xi*xi);

	// Nedelec0-shapes
	shape[i] = uDv<2> (0.5 * lam_e, xi); 

	// High Order edges ... Gradient fields 
	if(usegrad_edge[i])
	  {
	    LegendrePolynomial::
	      EvalMult (order_edge[i]-1, 
			xi, bub, pol_xi);

	    for (int j = 0; j < p; j++)
              shape[ii++] = Du<2> (pol_xi[j]);
	  }
      }
     
    INT<2> p = order_face[0]; // (order_cell[0],order_cell[1]);
    int fmax = 0; 
    for (int j = 1; j < 4; j++)
      if (vnums[j] > vnums[fmax])
	fmax = j;
    
    int f1 = (fmax+3)%4; 
    int f2 = (fmax+1)%4; 
    if(vnums[f2] > vnums[f1]) swap(f1,f2);  // fmax > f2 > f1; 

    AutoDiff<2> xi = sigma[fmax]-sigma[f1];  // in [-1,1]
    AutoDiff<2> eta = sigma[fmax]-sigma[f2]; // in [-1,1]
    
    T_ORTHOPOL::Calc(p[0]+1, xi,pol_xi);
    T_ORTHOPOL::Calc(p[1]+1,eta,pol_eta);
    
    //Gradient fields 
    if(usegrad_face[0])
      for (int k = 0; k < p[0]; k++)
	for (int j= 0; j < p[1]; j++)
          shape[ii++] = Du<2> (pol_xi[k]*pol_eta[j]);

    //Rotation of Gradient fields 
    for (int k = 0; k < p[0]; k++)
      for (int j= 0; j < p[1]; j++)
        shape[ii++] = uDv_minus_vDu<2> (pol_eta[j], pol_xi[k]);

    //Missing ones 
    for(int j = 0; j< p[0]; j++)
      shape[ii++] = uDv<2> (0.5*pol_xi[j], eta);

    for(int j = 0; j < p[1]; j++)
      shape[ii++] = uDv<2> (0.5*pol_eta[j], xi); 
  }


  
  //------------------------------------------------------------------------
  //        Tetrahedron
  //------------------------------------------------------------------------
 

  template<> template<typename Tx, typename TFA>  
  void HCurlHighOrderFE_Shape<ET_TET> :: T_CalcShape (Tx hx[3], TFA & shape) const
  {
    Tx x = hx[0], y = hx[1], z = hx[2];
    Tx lam[4] = { x, y, z, 1-x-y-z };

    ArrayMem<AutoDiff<3>,10> adpol1(order+2),adpol2(order+2),adpol3(order+2); 
    int ii = 6; 

    for (int i = 0; i < N_EDGE; i++)
      { 
	int p = order_edge[i]; 

        INT<2> e = GetEdgeSort (i, vnums);	  
	
	//Nedelec low order edge shape function 
        shape[i] = uDv_minus_vDu<3> (lam[e[0]], lam[e[1]]);

	//HO-Edge shape functions (Gradient Fields) 	
	if (p > 0 && usegrad_edge[i]) 
	  {     
	    LegendrePolynomial::
	      EvalScaledMult (order_edge[i]-1, 
			      lam[e[1]]-lam[e[0]], lam[e[0]]+lam[e[1]], 
			      lam[e[0]]*lam[e[1]], adpol1);
	    
	    for(int j = 0; j < p; j++) 	      
              shape[ii++] = Du<3> (adpol1[j]);
	  }
      }

    // face shape functions
    for(int i = 0; i < N_FACE; i++) 
      if (order_face[i][0] >= 2)
        {
          INT<4> fav = GetFaceSort (i, vnums);
          
          int vop = 6 - fav[0] - fav[1] - fav[2];  	
          int p = order_face[i][0];
          
          AutoDiff<3> xi = lam[fav[2]]-lam[fav[1]];
          AutoDiff<3> eta = lam[fav[0]]; // lo 
          AutoDiff<3> zeta = lam[vop];   // lz 
          
          TetShapesFaceLegendre::CalcSplitted (p+1, xi, eta, zeta, adpol1, adpol2); 
          
          // gradients 
          if (usegrad_face[i])
            for (int j = 0; j <= p-2; j++)
              for (int k = 0; k <= p-2-j; k++, ii++)
                shape[ii] = Du<3> (adpol1[j] * adpol2[k]);
          
          // other combination
          for (int j = 0; j <= p-2; j++)
            for (int k = 0; k <= p-2-j; k++, ii++)
              shape[ii] = uDv_minus_vDu<3> (adpol2[k], adpol1[j]);
          
          // type 3
          for (int j = 0; j <= p-2; j++, ii++)
            shape[ii] = wuDv_minus_wvDu<3> (lam[fav[1]], lam[fav[2]], adpol2[j]);
        }

    
    int p = order_cell[0]; 
    
    TetShapesInnerLegendre::CalcSplitted(p+1, x-(1-x-y-z), y, z,adpol1, adpol2, adpol3 );
    
    //gradient fields 
    if(usegrad_cell)
      for (int i = 0; i <= p-3; i++)
	for (int j = 0; j <= p-3-i; j++)
	  for (int k = 0; k <= p-3-i-j; k++)
            shape[ii++] = Du<3> (adpol1[i] * adpol2[j] * adpol3[k]);

    // other combinations
    for (int i = 0; i <= p-3; i++)
      for (int j = 0; j <= p-3-i; j++)
	for (int k = 0; k <= p-3-i-j; k++)
          { // not Sabine's original ...
            shape[ii++] = uDv_minus_vDu<3> (adpol1[i], adpol2[j] * adpol3[k]);
            shape[ii++] = uDv_minus_vDu<3> (adpol1[i] * adpol3[k], adpol2[j]);
          }
       
    for (int j= 0; j <= p-3; j++)
      for (int k = 0; k <= p-3-j; k++)
        shape[ii++] = wuDv_minus_wvDu<3> (lam[0], lam[3], adpol2[j] * adpol3[k]);
  }


		        
  //------------------------------------------------------------------------
  //                   Prism
  //------------------------------------------------------------------------

  template<> template<typename Tx, typename TFA>  
  void  HCurlHighOrderFE_Shape<ET_PRISM> :: T_CalcShape (Tx hx[3], TFA & shape) const
  {
    typedef TrigShapesInnerLegendre T_TRIGFACESHAPES;

    Tx x = hx[0], y = hx[1], z = hx[2];

    AutoDiff<3> lam[6] = { x, y, 1-x-y, x, y, 1-x-y };
    AutoDiff<3> muz[6]  = { 1-z, 1-z, 1-z, z, z, z };

    ArrayMem<AutoDiff<3>,20> adpolxy1(order+3),adpolxy2(order+3); 
    ArrayMem<AutoDiff<3>,20> adpolz(order+3);   

    int ii = 9;
    
    // horizontal edge shapes
    for (int i = 0; i < 6; i++)
      {
	int p = order_edge[i]; 
        INT<2> e = GetEdgeSort (i, vnums);	  
	
	//Nedelec0
        shape[i] = wuDv_minus_wvDu<3> (lam[e[0]], lam[e[1]], muz[e[1]]);
   
	//high order \nabla (P_edge(x,y) * muz)
	if(usegrad_edge[i])
	  {
	    /*
              T_ORTHOPOL::CalcTrigExt(p+1, lam[e[1]]-lam[e[0]],
              1-lam[e[0]]-lam[e[1]],adpolxy1);
              for(int j = 0; j <= p-1; j++)
              shape[ii++] = Du<3> (adpolxy1[j] * muz[e[1]]);
	    */
	    LegendrePolynomial::
	      EvalScaledMult (order_edge[i]-1, 
                              lam[e[1]]-lam[e[0]], lam[e[1]]+lam[e[0]], 
                              lam[e[0]]*lam[e[1]]*muz[e[1]], adpolxy1);
	    for(int j = 0; j <= p-1; j++)
              shape[ii++] = Du<3> (adpolxy1[j]);
	  }
      }

    //Vertical Edge Shapes
    for (int i = 6; i < 9; i++)
      {
	int p = order_edge[i]; 
        INT<2> e = GetEdgeSort (i, vnums);	  

        shape[i] = wuDv_minus_wvDu<3> (muz[e[0]], muz[e[1]], lam[e[1]]);
	
	//high order edges:  \nabla (T_ORTHOPOL^{p+1}(2z-1) * lam(x,y))
	if(usegrad_edge[i])
	  {
	    // T_ORTHOPOL::Calc (p+1, muz[e[1]]-muz[e[0]], adpolz);
	    // for (int j = 0; j < p; j++)
	    //   shape[ii++] = Du<3> (adpolz[j] * lam[e[1]]);

	    LegendrePolynomial::
	      EvalMult (order_edge[i]-1, 
			muz[e[1]]-muz[e[0]], 
			muz[e[0]]*muz[e[1]]*lam[e[1]], adpolz);
	    
	    for (int j = 0; j < p; j++)
              shape[ii++] = Du<3> (adpolz[j]);
	  }
      }


    const FACE * faces = ElementTopology::GetFaces (ET_PRISM); 

    // trig face shapes
    for (int i = 0; i < 2; i++)
      {
	int p = order_face[i][0];
	if (p < 2) continue;

	INT<4> fav = GetFaceSort (i, vnums);

	AutoDiff<3> xi = lam[fav[2]]-lam[fav[1]];
	AutoDiff<3> eta = lam[fav[0]]; // 1-lam[f2]-lam[f1];
	
	T_TRIGFACESHAPES::CalcSplitted(p+1,xi,eta,adpolxy1,adpolxy2); 

	if(usegrad_face[i])
	  // gradient-fields =>  \nabla( adpolxy1*adpolxy2*muz )
	  for (int j = 0; j <= p-2; j++)
	    for (int k = 0; k <= p-2-j; k++)
              shape[ii++] = Du<3> (adpolxy1[j]*adpolxy2[k] * muz[fav[2]]);
	
	// rotations of grad-fields => grad(uj)*vk*w -  uj*grad(vk)*w 
	for (int j = 0; j <= p-2; j++)
	  for (int k = 0; k <= p-2-j; k++)
            shape[ii++] = wuDv_minus_wvDu<3> (adpolxy2[k], adpolxy1[j], muz[fav[2]]);

	//  Ned0*adpolxy2[j]*muz 
	for (int j = 0; j <= p-2; j++,ii++)
          shape[ii] = wuDv_minus_wvDu<3> (lam[fav[1]], lam[fav[2]], adpolxy2[j]*muz[fav[2]]);
      }
    

    // quad faces
    for (int i = 2; i < 5; i++)
      {
	INT<2> p = order_face[i];
	 
	int fmax = 0;
	for (int j = 1; j < 4; j++)
	  if (vnums[faces[i][j]] > vnums[faces[i][fmax]]) fmax = j;

	int fz = 3-fmax; 
	int ftrig = fmax^1; 
	AutoDiff<3> xi = lam[faces[i][fmax]]-lam[faces[i][ftrig]]; 
	AutoDiff<3> eta = 1-lam[faces[i][fmax]]-lam[faces[i][ftrig]]; 
	AutoDiff<3> zeta = muz[faces[i][fmax]]-muz[faces[i][fz]]; 
	
	int pp = int(max2(p[0],p[1]))+1;
	T_ORTHOPOL::CalcTrigExt(pp,xi,eta,adpolxy1); 
	T_ORTHOPOL::Calc(pp,zeta,adpolz); 

	if(usegrad_face[i])
	  {
	    // Gradientfields nabla(polxy*polz) 
	    if (vnums[faces[i][ftrig]] > vnums[faces[i][fz]]) 
	      for (int k = 0; k <= p[0]-1; k++)
		for (int j = 0; j <= p[1]-1; j++)
                  shape[ii++] = Du<3> (adpolxy1[k] * adpolz[j]);
	    else
	      for (int j = 0; j <= p[0]-1; j++)
		for (int k = 0; k <= p[1]-1; k++)
                  shape[ii++] = Du<3> (adpolxy1[k] * adpolz[j]);
	  }
	  
	// Rotations of GradFields => nabla(polxy)*polz - polxy*nabla(polz)
	if (vnums[faces[i][ftrig]] > vnums[faces[i][fz]]) 
	  for (int k = 0; k <= p[0]-1; k++)
	    for (int j = 0; j <= p[1]-1; j++)
              shape[ii++] = uDv_minus_vDu<3> (adpolz[j], adpolxy1[k]);
	else
	  for (int j = 0; j <= p[0]-1; j++)
	    for (int k = 0; k <= p[1]-1; k++)
              shape[ii++] = uDv_minus_vDu<3> (adpolxy1[k], adpolz[j]);
	
	// Type 3 
	// (ned0_trig)*polz, (ned0_quad)* polxy 

	if(vnums[faces[i][ftrig]] > vnums[faces[i][fz]]) // p = (p_trig,p_z) 
	  {
	    for(int j=0;j<=p[0]-1;j++) 
              shape[ii++] = wuDv_minus_wvDu<3> (muz[faces[i][fz]], muz[faces[i][fmax]], adpolxy1[j]);
	    for(int j=0;j<=p[1]-1;j++) 
              shape[ii++] = wuDv_minus_wvDu<3> (lam[faces[i][ftrig]], lam[faces[i][fmax]], adpolz[j]);
	  }
	else 
	  {
	    for(int j=0;j<=p[0]-1;j++) 
              shape[ii++] = wuDv_minus_wvDu<3> (lam[faces[i][ftrig]], lam[faces[i][fmax]], adpolz[j]);
	    for(int j=0;j<=p[1]-1;j++) 
              shape[ii++] = wuDv_minus_wvDu<3> (muz[faces[i][fz]], muz[faces[i][fmax]], adpolxy1[j]);
	  }
      }
    
    if(order_cell[0] > 1 && order_cell[2] > 0) 
      {
	T_TRIGFACESHAPES::CalcSplitted(order_cell[0]+1,x-y,1-x-y,adpolxy1,adpolxy2);
	T_ORTHOPOL::Calc(order_cell[2]+1,2*z-1,adpolz); 
	
	// gradientfields
	if(usegrad_cell)
	  for(int i=0;i<=order_cell[0]-2;i++)
	    for(int j=0;j<=order_cell[0]-2-i;j++)
	      for(int k=0;k<=order_cell[2]-1;k++)
                shape[ii++] = Du<3> (adpolxy1[i]*adpolxy2[j]*adpolz[k]);

	// Rotations of gradientfields
	for(int i=0;i<=order_cell[0]-2;i++)
	  for(int j=0;j<=order_cell[0]-2-i;j++)
	    for(int k=0;k<=order_cell[2]-1;k++)
	      {
                shape[ii++] = wuDv_minus_wvDu<3> (adpolxy1[i],adpolxy2[j],adpolz[k]);
                shape[ii++] = uDv_minus_vDu<3> (adpolxy1[i],adpolxy2[j]*adpolz[k]);
	      }

	// Type 3 
	// ned0(trig) * polxy2[j]*polz 
	// z.DValue(0) * polxy1[i] * polxy2[j] 
	// double ned_trig[2] = {y.Value(),-x.Value()};  
	for(int j=0;j<=order_cell[0]-2;j++) 
	  for (int k=0;k<=order_cell[2]-1;k++) 
            shape[ii++] = wuDv_minus_wvDu<3> (x,y, adpolxy2[j]*adpolz[k]);

    	for(int i = 0; i <= order_cell[0]-2; i++) 
	  for(int j = 0; j <= order_cell[0]-2-i; j++) 
            shape[ii++] = wuDv_minus_wvDu<3> (z,1-z, adpolxy1[i]*adpolxy2[j]);
      }
  }



  //------------------------------------------------------------------------
  // HCurlHighOrderHex
  //------------------------------------------------------------------------


  template<> template<typename Tx, typename TFA>  
  void HCurlHighOrderFE_Shape<ET_HEX> :: T_CalcShape (Tx hx[3], TFA & shape) const
  {
    Tx x = hx[0], y = hx[1], z = hx[2];

    AutoDiff<3> lami[8]={(1-x)*(1-y)*(1-z),x*(1-y)*(1-z),x*y*(1-z),(1-x)*y*(1-z),
			 (1-x)*(1-y)*z,x*(1-y)*z,x*y*z,(1-x)*y*z}; 
    AutoDiff<3> sigma[8]={(1-x)+(1-y)+(1-z),x+(1-y)+(1-z),x+y+(1-z),(1-x)+y+(1-z),
			  (1-x)+(1-y)+z,x+(1-y)+z,x+y+z,(1-x)+y+z}; 
     
    int ii = 12; 
    ArrayMem<AutoDiff<3>, 20> pol_xi(order+2),pol_eta(order+2),pol_zeta(order+2);
   
    // edges
    for (int i = 0; i < 12; i++)
      {
	int p = order_edge[i]; 
        INT<2> e = GetEdgeSort (i, vnums);	  
	
	AutoDiff<3> xi  = sigma[e[1]]-sigma[e[0]];
	AutoDiff<3> lam_e = lami[e[0]]+lami[e[1]];  
        Tx bub = 0.25 * lam_e * (1 - xi*xi);

	// Nedelec0-shapes
	shape[i] = uDv<3> (0.5 * lam_e, xi); 

	// High Order edges ... Gradient fields 
	if(usegrad_edge[i])
	  {
	    LegendrePolynomial::
	      EvalMult (order_edge[i]-1, 
			xi, bub, pol_xi);

	    for (int j = 0; j < p; j++)
              shape[ii++] = Du<3> (pol_xi[j]);
	  }
      }
    
    //Faces 
    const FACE * faces = ElementTopology::GetFaces (ET_HEX);
    for (int i = 0; i<6; i++)
      {
	INT<2> p = order_face[i];
	
	AutoDiff<3> lam_f = 0;
	for (int j = 0; j < 4; j++)
	  lam_f += lami[faces[i][j]];
	
	int qmax = 0;
	for (int j = 1; j < 4; j++)
	  if (vnums[faces[i][j]] > vnums[faces[i][qmax]])
	    qmax = j;
	
	int q1 = (qmax+3)%4; 
	int q2 = (qmax+1)%4; 

	if(vnums[faces[i][q2]] > vnums[faces[i][q1]])
	  swap(q1,q2);  // fmax > f1 > f2

	int fmax = faces[i][qmax]; 
	int f1 = faces[i][q1]; 
	int f2 = faces[i][q2]; 
	      
	AutoDiff<3> xi = sigma[fmax]-sigma[f1]; 
	AutoDiff<3> eta = sigma[fmax]-sigma[f2]; 
    
	T_ORTHOPOL::Calc(p[0]+1, xi,pol_xi);
	T_ORTHOPOL::Calc(p[1]+1,eta,pol_eta);
	
	//Gradient fields 
	if(usegrad_face[i])
	  for (int k = 0; k < p[0]; k++)
	    for (int j= 0; j < p[1]; j++)
              shape[ii++] = Du<3> (lam_f * pol_xi[k] * pol_eta[j]);
	
	//Rotation of Gradient fields 
	for (int k = 0; k < p[0]; k++)
	  for (int j= 0; j < p[1]; j++)
            shape[ii++] = uDv_minus_vDu<3> (pol_eta[j], lam_f * pol_xi[k]);

	// Missing ones 
	for(int j = 0; j < p[0];j++) 
          shape[ii++] = wuDv_minus_wvDu<3> (0.5, eta, pol_xi[j]*lam_f); 

	for(int j = 0; j < p[1];j++) 
          shape[ii++] = wuDv_minus_wvDu<3> (0.5, xi, pol_eta[j]*lam_f); 
      }
    
    // Element-based shapes
    T_ORTHOPOL::Calc(order_cell[0]+1,2*x-1,pol_xi);
    T_ORTHOPOL::Calc(order_cell[1]+1,2*y-1,pol_eta);
    T_ORTHOPOL::Calc(order_cell[2]+1,2*z-1,pol_zeta); 
    
    //Gradient fields
    if(usegrad_cell)
      for (int i=0; i<order_cell[0]; i++)
	for(int j=0; j<order_cell[1]; j++) 
	  for(int k=0; k<order_cell[2]; k++)
            shape[ii++] = Du<3> (pol_xi[i] * pol_eta[j] * pol_zeta[k]);

    //Rotations of gradient fields
    for (int i=0; i<order_cell[0]; i++)
      for(int j=0; j<order_cell[1]; j++) 
	for(int k=0; k<order_cell[2]; k++)
	  {
            shape[ii++] = uDv_minus_vDu<3> (pol_xi[i] * pol_eta[j], pol_zeta[k]);
            shape[ii++] = uDv_minus_vDu<3> (pol_xi[i], pol_eta[j] * pol_zeta[k]);
	  } 
    
    for(int i = 0; i < order_cell[0]; i++) 
      for(int j = 0; j < order_cell[1]; j++)
        shape[ii++] = wuDv_minus_wvDu<3> (z,1-z,pol_xi[i] * pol_eta[j]);

    for(int i = 0; i < order_cell[0]; i++) 
      for(int k = 0; k < order_cell[2]; k++)
        shape[ii++] = wuDv_minus_wvDu<3> (y,1-y,pol_xi[i] * pol_zeta[k]);

    for(int j = 0; j < order_cell[1]; j++)
      for(int k = 0; k < order_cell[2]; k++)
        shape[ii++] = wuDv_minus_wvDu<3> (x,1-x,pol_eta[j] * pol_zeta[k]);
  }



  //------------------------------------------------------------------------
  //            Pyramid
  //------------------------------------------------------------------------


  template<> template<typename Tx, typename TFA>  
  void  HCurlHighOrderFE_Shape<ET_PYRAMID> :: T_CalcShape (Tx hx[3], TFA & shape) const
  {
    typedef TrigShapesInnerLegendre T_TRIGFACESHAPES;  

    Tx x = hx[0], y = hx[1], z = hx[2];

    if(z.Value()==1.) z.Value() -=1.e-8; 


    AutoDiff<3> xt = x/(1-z); 
    AutoDiff<3> yt = y/(1-z); 
    AutoDiff<3> sigma[5] = {(1-xt)+(1-yt)+(1-z),xt+(1-yt)+(1-z), xt + yt + (1-z), 
			    (1-xt)+yt+(1-z),z}; 

    AutoDiff<3> lami[5] = {(1-xt)*(1-yt)*(1-z),xt*(1-yt)*(1-z), xt * yt * (1-z), 
			   (1-xt)*yt*(1-z),z}; 

    AutoDiff<3> lambda[5] = {(1-xt)*(1-yt),xt*(1-yt), xt * yt, 
                             (1-xt)*yt,z}; 
        
       
    const EDGE * edges = ElementTopology::GetEdges (ET_PYRAMID);
    
    ArrayMem<AutoDiff<3>, 20> pol_xi(order+2), pol_eta(order+2), pol_zeta(order+2),pol2_zeta(order+2); 
    
    int ii =8; 
 
    // horizontal edges incl. Nedelec 0
    for (int i = 0; i < 4; i++)
      {
	int es = edges[i][0], ee = edges[i][1]; 
	if (vnums[es] > vnums[ee]) swap (es, ee);
	
	AutoDiff<3> xi  = sigma[ee] - sigma[es];   
	// AutoDiff<3> lam_e = lami[ee] + lami[es];
	AutoDiff<3> lam_t = lambda[ee] + lambda[es]; 
	
        shape[i] = uDv<3> (0.5 * (1-z)*(1-z)*lam_t, xi);

	if(usegrad_edge[i])
	  {
	    // T_ORTHOPOL::CalcTrigExt (order_edge[i]+1, xi*(1-z), z, pol_xi);

	    LegendrePolynomial::
	      EvalScaledMult (order_edge[i]-1, 
			      xi*(1-z), 1-z, 
			      (1-xi*xi)*(1-z)*(1-z), pol_xi);

	    for(int j = 0; j < order_edge[i]; j++)
              shape[ii++] = Du<3> (pol_xi[j]*lam_t);
	  }
      }
    
    // vertical edges incl. Nedelec 0  
    for(int i = 4; i < 8; i++)
      {
	int es = edges[i][0], ee = edges[i][1]; 
	
	if (vnums[es] > vnums[ee]) swap (es, ee);

        shape[i] = uDv_minus_vDu<3> (lami[es], lami[ee]);

	if (usegrad_edge[i])
	  {
	    /*
              T_ORTHOPOL::CalcTrigExt (order_edge[i]+1, lami[ee]-lami[es],  
              1-lami[es]-lami[ee], pol_xi);
	    */
	    LegendrePolynomial::
	      EvalScaledMult (order_edge[i]-1, lami[ee]-lami[es], lami[es]+lami[ee], 
			      lami[ee]*lami[es],
			      pol_xi);
	    for(int j = 0; j < order_edge[i]; j++)
	      shape[ii++] = Du<3> (pol_xi[j]);
	  }
      }

    const FACE * faces = ElementTopology::GetFaces (ET_PYRAMID); 

    // trig face dofs
    for (int i = 0; i < 4; i++)
      if (order_face[i][0] >= 2)
	{
	  int p = order_face[i][0];
	  AutoDiff<3> lam_face = lambda[faces[i][0]] + lambda[faces[i][1]];  
	  AutoDiff<3> bary[3] = 
	    {(sigma[faces[i][0]]-(1-z)-lam_face)*(1-z), 
	     (sigma[faces[i][1]]-(1-z)-lam_face)*(1-z), z}; 
			     
	  int fav[3] = {0, 1, 2};
	  if(vnums[faces[i][fav[0]]] > vnums[faces[i][fav[1]]]) swap(fav[0],fav[1]); 
	  if(vnums[faces[i][fav[1]]] > vnums[faces[i][fav[2]]]) swap(fav[1],fav[2]);
	  if(vnums[faces[i][fav[0]]] > vnums[faces[i][fav[1]]]) swap(fav[0],fav[1]); 	
	 
	  T_TRIGFACESHAPES::CalcSplitted(p+1, bary[fav[2]]-bary[fav[1]], 
					 bary[fav[0]],pol_xi,pol_eta);
	  
	  for(int j=0;j<=p-2;j++) pol_eta[j] *= lam_face;  
	  
	  // phi = pol_xi * pol_eta * lam_face; 
	  // Type 1: Gradient Functions 
	  if(usegrad_face[i])
	    for(int j=0;j<= p-2; j++)
	      for(int k=0;k<=p-2-j; k++)
                shape[ii++] = Du<3> (pol_xi[j] * pol_eta[k]);

	  // Type 2:  
	  for(int j=0;j<= p-2; j++)
	    for(int k=0;k<=p-2-j; k++)
              shape[ii++] = uDv_minus_vDu<3> (pol_eta[k], pol_xi[j]);

	  // Type 3: Nedelec-based ones (Ned_0*v_j)
	  for(int j=0;j<=p-2;j++)
            shape[ii++] = wuDv_minus_wvDu<3> (bary[fav[1]], bary[fav[2]], pol_eta[j]);
	}


    // quad face 
    if (order_face[4][0] >= 1)
      {
	int px = order_face[4][0];
	int py = order_face[4][0]; // SZ-Attentione 
	int p = max2(px, py);
	int pp = p+1;

	AutoDiff<3> fac = 1.0;
	for (int k = 1; k <= p; k++)
	  fac *= (1-z);
	
	int fmax = 0;
	for (int l=1; l<4; l++) 
	  if (vnums[l] > vnums[fmax]) fmax = l;  

	int f1 = (fmax+3)%4;
	int f2 = (fmax+1)%4; 
	if(vnums[f1]>vnums[f2]) swap(f1,f2);  // fmax > f2 > f1 
        
 	AutoDiff<3> xi  = sigma[fmax] - sigma[f2]; 
	AutoDiff<3> eta = sigma[fmax] - sigma[f1];
	
	pol_eta[pp-1] = 0; //!
	pol_xi[pp-1] = 0;  //!

	T_ORTHOPOL::Calc (pp+2, xi, pol_xi);	
	T_ORTHOPOL::Calc (pp+2, eta, pol_eta);
	
	for(int k=0;k<pp;k++) pol_eta[k] = fac*pol_eta[k]; 

	// Type 1: Gradient-fields 
	if (usegrad_face[4])
	  for (int k = 0; k <= px-1; k++) 
	    for (int j = 0; j <= py-1; j++, ii++) 
              shape[ii] = Du<3> (pol_xi[k] * pol_eta[j]);

	// Type 2: 
	for (int k = 0; k < px; k++) 
	  for (int j = 0; j < py; j++, ii++) 
            shape[ii] = uDv_minus_vDu<3> (pol_eta[j], pol_xi[k]);

	// Type 3:
	for (int k=0; k< px; k++,ii++)
          shape[ii] = uDv<3> (0.5*pol_xi[k]*fac, eta);

	for (int k=0; k< py; k++,ii++)
          shape[ii] = uDv<3> (0.5*pol_eta[k]*fac, xi);  // shouldn't there be a  *fac  ????
      }


    if (order_cell[0] >= 2)
      {
	int pp = order_cell[0];
	// According H^1 terms: 
	// u_i = L_i+2(2xt-1)
	// v_j = L_j+2(2yt-1) 
	// w_k = z * (1-z)^(k+2)  with 0 <= i,j <= k, 0<= k <= p-2  
	
	T_ORTHOPOL::Calc (pp+3, 2*xt-1, pol_xi);
	T_ORTHOPOL::Calc (pp+3, 2*yt-1, pol_eta);		
		
	pol_zeta[0] = z*(1-z)*(1-z);
	for (int k=1;k<=pp-2;k++) 
	  pol_zeta[k] = (1-z)*pol_zeta[k-1];

	// This one includes constant and linear polynomial
	AutoDiff<3> zz = 2*z-1; 
	IntegratedLegendrePolynomial (pp, zz, pol2_zeta); 
	

	// --> Attention: Use Integrated Legendre also for H^1
	if(usegrad_cell)
	  {
	    for(int k=0;k<= pp-2;k++)
	      for(int i=0;i<=k;i++)
		for(int j=0;j<=k;j++,ii++)
                  shape[ii] = Du<3> (pol_xi[i]*pol_eta[j]*pol_zeta[k]);
	  }
	
	// Type 2a: l.i. combinations of grad-terms   
	// shape = u_i \nabla(v_j) w_k 
	// shape = u_i v_j \nabla(w_k) 
	for(int k=0;k<= pp-2;k++)
	  for(int i=0;i<=k;i++)
	    for(int j=0;j<=k;j++,ii++)
              shape[ii] = uDv<3> (pol_xi[i]*pol_zeta[k], pol_eta[j]);

	// Type 2b: shape = v_j w_k \nabla (xt) 
	//          shape = u_i w_k \nabla (yt)
	for(int k = 0;k<= pp-2;k++)
	  for(int j=0;j<=k;j++) 
            shape[ii++] = uDv<3> (pol_eta[j]*pol_zeta[k], xt);
	
	for(int  k = 0;k<= pp-2;k++)
	  for (int i=0;i<=k;i++)
            shape[ii++] = uDv<3> (pol_xi[i]*pol_zeta[k], yt);
	
	// 3rd component spans xi^i eta^j zeta^(k-1), i,j <= k
	// pol_zeta starts linear in zeta 
	// pol_xi and pol_eta quadratic in xi resp. eta 
	pol_zeta[0] = (1-z);
	for (int k=1;k<=pp;k++) 
	  pol_zeta[k] = (1-z)*pol_zeta[k-1];
     
	for(int k=0;k<= pp-1;k++)
	  for(int i=0;i<=k;i++)
	    for(int j=0;j<=k;j++,ii++)
              shape[ii] = uDv<3> (pol_eta[j] * pol_xi[i] * pol_zeta[k], z);
      }
  }
  

}


#endif