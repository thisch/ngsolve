#ifndef FILE_HDIVHOFE_IMPL
#define FILE_HDIVHOFE_IMPL

/*********************************************************************/
/* File:   hdivhofe_impl.hpp                                         */
/* Author: A. Becirovic, S. Zaglmayr, J. Schoeberl                   */
/* Date:   15. Feb. 2003                                             */
/*********************************************************************/



namespace ngfem
{
  
  /// compute shape
  template<typename Tx, typename TFA>  
  inline void HDivHighOrderFE_Shape<ET_TET> :: T_CalcShape (Tx hx[], TFA & shape) const
  {
    if (only_ho_div && order_inner[0]<=1) return;
    Tx x = hx[0], y = hx[1], z = hx[2];
    Tx lami[4] = { x, y, z, 1-x-y-z };

    ArrayMem<Tx,10> adpol1(order), adpol2(order), adpol3(order);
	
    int ii = 4; 
    if (!only_ho_div)
    {
      const FACE * faces = ElementTopology::GetFaces (ET_TET);
      for (int i = 0; i < 4; i++)
        {
        int p = order_face[i][0];

          int fav[3];
          for(int j = 0; j < 3; j++) fav[j]=faces[i][j];
          
          //Sort vertices  first edge op minimal vertex
          if(vnums[fav[0]] > vnums[fav[1]]) swap(fav[0],fav[1]);
          if(vnums[fav[1]] > vnums[fav[2]]) swap(fav[1],fav[2]);
          if(vnums[fav[0]] > vnums[fav[1]]) swap(fav[0],fav[1]);
          int fop = 6 - fav[0] - fav[1] - fav[2];
          
        // RT lowest order
          shape[i] = uDvDw_Cyclic<3> (lami[fav[0]], lami[fav[1]], lami[fav[2]]);

          Tx xi = lami[fav[1]]-lami[fav[0]];
          Tx eta = lami[fav[2]];
          Tx zeta = lami[fop];  
        
          T_FACESHAPES::CalcSplitted (p+2, xi, eta, zeta, adpol1, adpol2); 

          // Compability WITH TRIG!! 
          for (int k = 0; k < adpol1.Size(); k++)
            adpol1[k] *= 0.5; 
            
          // Curl (Type 2) 2*grad v x grad u
          for (int j = 0; j <= p-1; j++) 
            for (int k = 0; k <= p-1-j; k++)
              shape[ii++] = Du_Cross_Dv<3> (adpol2[k], adpol1[j]);

          // Curl (Type 3) //curl( * v) = nabla v x ned + curl(ned)*v
          for (int j = 0; j <= p-1; j++)
            shape[ii++] = curl_uDvw_minus_Duvw<3> (lami[fav[0]], lami[fav[1]], adpol2[j]);
        }
    }
    else
      ii = 0;
    // cell-based shapes 
    int p = order_inner[0];
    int pc = order_inner[0]; // should be order_inner_curl  
    int pp = max(p,pc); 
    if ( pp >= 2 )
      {
        T_INNERSHAPES::CalcSplitted(pp+2, lami[0]-lami[3], lami[1], lami[2], adpol1, adpol2, adpol3 );
      
        if (!only_ho_div){
          // Curl-Fields 
          for (int i = 0; i <= pc-2; i++)
            for (int j = 0; j <= pc-2-i; j++)
              for (int k = 0; k <= pc-2-i-j; k++)
                {
                  // grad v  x  grad (uw)
                  shape[ii++] = Du_Cross_Dv<3> (adpol2[j], adpol1[i]*adpol3[k]);
        
                  // grad w  x  grad (uv)
                  shape[ii++] = Du_Cross_Dv<3> (adpol3[k], adpol1[i]*adpol2[j]);
                }     


          // Type 1 : Curl(T3)
          // ned = lami[0] * nabla(lami[3]) - lami[3] * nabla(lami[0]) 
          for (int j= 0; j <= pc-2; j++)
            for (int k = 0; k <= pc-2-j; k++)
              shape[ii++] = curl_uDvw_minus_Duvw<3> (lami[0], lami[3], adpol2[j]*adpol3[k]);
        }

        if (!ho_div_free)
          { 
            // Type 2:  
            // (grad u  x  grad v) w 
            for (int i = 0; i <= p-2; i++)
              for (int j = 0; j <= p-2-i; j++)
                for (int k = 0; k <= p-2-i-j; k++)
                  shape[ii++] = wDu_Cross_Dv<3> (adpol1[i], adpol2[j], adpol3[k]);

            // (ned0 x grad v) w    
            for (int j = 0; j <= p-2; j++)
              for (int k= 0; k <= p-2-j; k++)
                shape[ii++] = wDu_Cross_Dv<3> (lami[0], adpol2[j], lami[3]*adpol3[k]);
            
            // Type 3: 
            // (ned0 x e_z) v = (N_y, -N_x,0)^T * v ) 
            for (int j=0; j<=p-2; j++) 
              shape[ii++] = wDu_Cross_Dv<3> (lami[0], z, lami[3]*adpol2[j]);
          }
      }
  }

  











  template<typename Tx, typename TFA>  
  void HDivHighOrderFE_Shape<ET_PRISM> :: T_CalcShape (Tx hx[], TFA & shape) const
  {
    Tx x = hx[0], y = hx[1], z = hx[2];

    AutoDiff<3> lami[6] = { x, y, 1-x-y, x, y, 1-x-y };
    AutoDiff<3> muz[6]  = { 1-z, 1-z, 1-z, z, z, z };
       
    const FACE * faces = ElementTopology::GetFaces (ET_PRISM); 

    ArrayMem<AutoDiff<3>,20> adpolxy1(order+4),adpolxy2(order+4); 
    ArrayMem<AutoDiff<3>,20> adpolz(order+4);   


    ArrayMem<Tx,10> adpol1(order), adpol2(order), adpol3(order);
    
    // trig faces

    int ii = 5;
    for (int i = 0; i < 2; i++)
      {
	int p = order_face[i][0];
	int fav[3] = {faces[i][0], faces[i][1], faces[i][2]};

	if(vnums[fav[0]] > vnums[fav[1]]) swap(fav[0],fav[1]); 
	if(vnums[fav[1]] > vnums[fav[2]]) swap(fav[1],fav[2]);
	if(vnums[fav[0]] > vnums[fav[1]]) swap(fav[0],fav[1]); 	  	
	
	shape[i] = wDu_Cross_Dv<3> (lami[fav[0]], lami[fav[1]], muz[fav[0]]);

	AutoDiff<3> xi = lami[fav[1]]-lami[fav[0]];
	AutoDiff<3> eta = lami[fav[2]];

	T_TRIGFACESHAPES::CalcSplitted(p+2,xi,eta,adpol1,adpol2); 
	for (int k = 0; k < adpol1.Size(); k++)
          adpol1[k] *= 0.5; 
	
        // Curl (Type 2) 2*grad v x grad u
        for (int j = 0; j <= p-1; j++) 
          for (int k = 0; k <= p-1-j; k++)
	    shape[ii++] = Du_Cross_Dv<3> (adpol2[k]*muz[fav[2]], adpol1[j]);

        // Curl (Type 3) //curl( * v) = nabla v x ned + curl(ned)*v
        for (int j = 0; j <= p-1; j++)
	  shape[ii++] = curl_uDvw_minus_Duvw<3> (lami[fav[0]], lami[fav[1]], adpol2[j]*muz[fav[2]]);
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
	int f = faces[i][fmax];
	int f1 = faces[i][ftrig];
	int f2 = faces[i][fz];

	AutoDiff<3> xi = lami[faces[i][fmax]]-lami[faces[i][ftrig]]; 
	AutoDiff<3> eta = 1-lami[faces[i][fmax]]-lami[faces[i][ftrig]]; 
	AutoDiff<3> zeta = muz[faces[i][fmax]]-muz[faces[i][fz]]; 
	
	int pp = int(max2(p[0],p[1]))+1;
	T_ORTHOPOL::CalcTrigExt(pp,xi,eta,adpolxy1); 
	T_ORTHOPOL::Calc(pp,zeta,adpolz); 

	double fac = (vnums[faces[i][fz]] > vnums[faces[i][ftrig]]) ? 1 : -1;

	shape[i] = uDvDw_minus_DuvDw<3> (lami[faces[i][fmax]],
					 lami[faces[i][ftrig]], -0.5*fac*zeta);
					    

	if (vnums[f1] > vnums[f2])
	  {
	    for (int k = 0; k <= p[0]-1; k++)
	      for (int j = 0; j <= p[1]-1; j++, ii++)
		shape[ii] = Du_Cross_Dv<3> (adpolxy1[k], -2*adpolz[j]);
	  }
	else
	  {
	    for (int j = 0; j <= p[1]-1; j++)
	      for (int k = 0; k <= p[0]-1; k++, ii++)
		shape[ii] = Du_Cross_Dv<3> (adpolxy1[k],  2*adpolz[j]);
	  }
	  

        if (vnums[f1] > vnums[f2])
          {
            for (int j= 0; j <= p[0]-1; j++, ii++)
	      shape[ii] = Du_Cross_Dv<3> (adpolxy1[j], zeta);
            for(int j=0; j<= p[1]-1;j++,ii++)
	      shape[ii] = curl_uDvw_minus_Duvw<3> (lami[f1], lami[f], 2*adpolz[j]);
          }  
        else
          {
            for(int j = 0; j <= p[0]-1; j++,ii++)
	      shape[ii] = curl_uDvw_minus_Duvw<3> (lami[f1], lami[f], 2*adpolz[j]);
            for (int j= 0; j <= p[1]-1; j++, ii++)
	      shape[ii] = Du_Cross_Dv<3> (adpolxy1[j], zeta);
          }   
      }    

    int p = order_inner[0];
    int pz = order_inner[2];
    if(p >= 1 && pz >= 1)
      {
	T_TRIGFACESHAPES::CalcSplitted(p+2,x-y,1-x-y,adpolxy1,adpolxy2);
	T_ORTHOPOL::Calc(pz+2,2*z-1,adpolz); 
	

	for(int i=0;i<=p-1;i++)
	  for(int j=0;j<=p-1-i;j++)
	    for(int k=0;k<=pz-1;k++)
	      shape[ii++] = Du_Cross_Dv<3> (adpolxy1[i],adpolxy2[j]*adpolz[k]);

	for(int i=0;i<=p-1;i++)
	  for(int j=0;j<=p-1-i;j++)
	    for(int k=0;k<=pz-1;k++)
	      shape[ii++] = curl_uDvw_minus_Duvw<3> (adpolxy1[i],adpolxy2[j],adpolz[k]);

	for(int j=0;j<=p-1;j++) 
	  for (int k=0;k<=pz-1;k++) 
            shape[ii++] = curl_uDvw_minus_Duvw<3> (x,y, adpolxy2[j]*adpolz[k]);

    	for(int i = 0; i <= p-1; i++) 
	  for(int j = 0; j <= p-1-i; j++) 
            shape[ii++] = curl_uDvw_minus_Duvw<3> (z,1-z, adpolxy1[i]*adpolxy2[j]);

        if (!ho_div_free)
          {  // not yet verified
            ScaledLegendrePolynomial (p, x-y, x+y, adpolxy1);
            LegendrePolynomial (p, 1-2*x, adpolxy2);
            LegendrePolynomial (pz, 1-2*z, adpolz);

            /*
            for (int i = 0; i <= p; i++)
              for (int j = 0; j <= p-i; j++)
                for (int k = 0; k <= pz; k++)
                  if (i+j+k > 0)
                    shape[ii++] = wDu_Cross_Dv<3> ((x-y)*adpolxy1[i], x*adpolxy2[j], z*(1-z)*adpolz[k]);
            */

            for (int i = 0; i <= p; i++)
              for (int j = 0; j <= p-i; j++)
                for (int k = 0; k < pz; k++)
                  shape[ii++] = wDu_Cross_Dv<3> ((x-y)*adpolxy1[i], x*adpolxy2[j], z*(1-z)*adpolz[k]);

            for (int i = 0; i < p; i++)
              for (int j = 0; j < p-i; j++)
                shape[ii++] = wDu_Cross_Dv<3> (z, x*y*adpolxy1[i], (1-x-y)*adpolxy2[j]);

            for (int i = 0; i < p; i++)
              shape[ii++] = wDu_Cross_Dv<3> (z, x, y*(1-x-y)*adpolxy1[i]);


            /*
            for (int i = 0; i <= p-1; i++)
              for (int k = 0; k <= pz; k++)
                shape[ii++] = wDu_Cross_Dv<3> (x*y*adpolxy1[i], z*adpolz[k],  1-x-y);
            */
          }
      }

    if (ii != ndof) cout << "hdiv-prism: dofs missing, ndof = " << ndof << ", ii = " << ii << endl;
  }






}



#endif