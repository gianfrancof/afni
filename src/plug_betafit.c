#include "afni.h"

#ifndef ALLOW_PLUGINS
#  error "Plugins not properly set up -- see machdep.h"
#endif

static char helpstring[] = "No help is available" ;

char * BFIT_main( PLUGIN_interface * ) ;

#define NYESNO 2
static char * YESNO_strings[NYESNO] = { "No" , "Yes" } ;

PLUGIN_interface * PLUGIN_init(int ncall)
{
   PLUGIN_interface * plint ;

   if( ncall > 0 ) return NULL ;  /* only one interface */

   /*-- set titles and call point --*/

   plint = PLUTO_new_interface( "Histogram: BF" ,
                                "Betafit Histogram" ,
                                helpstring ,
                                PLUGIN_CALL_VIA_MENU , BFIT_main  ) ;

   PLUTO_add_hint( plint , "Histogram: Betafit" ) ;

   PLUTO_set_sequence( plint , "A:afniinfo:dset" ) ;

   /*-- first line of input --*/

   PLUTO_add_option( plint , "Source" , "Source" , TRUE ) ;

   PLUTO_add_dataset(  plint ,
                       "Dataset" ,        /* label next to button   */
                       ANAT_ALL_MASK ,    /* take any anat datasets */
                       FUNC_ALL_MASK ,    /* only allow fim funcs   */
                       DIMEN_3D_MASK |    /* need 3D+time datasets  */
                       BRICK_ALLREAL_MASK /* need real-valued datasets */
                    ) ;
   PLUTO_add_number( plint , "Brick"  , 0,9999,0, 0,1 ) ;
   PLUTO_add_string( plint , "Square" , NYESNO , YESNO_strings , 1 ) ;

   /*-- second line of input --*/

   PLUTO_add_option( plint , "a Params" , "Params" , TRUE ) ;
   PLUTO_add_number( plint , "a bot" , 2,50,1 ,  5 , 1 ) ;
   PLUTO_add_number( plint , "a top" , 2,50,1 , 20 , 1 ) ;

   PLUTO_add_option( plint , "b Params" , "Params" , TRUE ) ;
   PLUTO_add_number( plint , "b bot" , 10,400,0 ,  10 , 1 ) ;
   PLUTO_add_number( plint , "b top" , 10,999,0 , 200 , 1 ) ;

   PLUTO_add_option( plint , "Misc" , "Params" , TRUE ) ;
   PLUTO_add_number( plint , "N ran" , 10,1000,-2 , 100 , 1 ) ;
   PLUTO_add_number( plint , "% cut" , 20,99,0 , 70,1 ) ;
   PLUTO_add_string( plint , "HSqrt"  , NYESNO , YESNO_strings , 0 ) ;

   /*-- (optional) line of input --*/

   PLUTO_add_option( plint , "Mask" , "Mask" , FALSE ) ;
   PLUTO_add_dataset( plint , "Dataset" ,
                                    ANAT_ALL_MASK , FUNC_ALL_MASK ,
                                    DIMEN_ALL_MASK | BRICK_ALLREAL_MASK ) ;
   PLUTO_add_number( plint , "Brick" , 0,9999,0 , 0,1 ) ;

   /*-- (optional) line of input --*/

   PLUTO_add_option( plint , "Range"  , "Range" , FALSE ) ;
   PLUTO_add_number( plint , "Bottom" , -99999,99999, 1, 0,1 ) ;
   PLUTO_add_number( plint , "Top"    , -99999,99999,-1, 0,1 ) ;

   /*-- (optional) line of input --*/

   PLUTO_add_option( plint , "Extra"  , "Extra" , FALSE ) ;
   PLUTO_add_number( plint , "a" , 2,50,1 ,  5 , 1 ) ;
   PLUTO_add_number( plint , "b" , 10,999,0 , 200 , 1 ) ;

   return plint ;
}

/*----------------------------------------------------------------------
   Inputs: (a,b,xc) for incomplete beta
   Outputs:
   Let Ipq = Int( x**(a-1)*(1-x)**(b-1)*ln(x)**p*ln(1-x)**q, x=0..xc ).
   Then
     bi7[0] = I00     = normalization factor
     bi7[1] = I10/I00 = <ln(x)>
     bi7[2] = I01/I00 = <ln(1-x)>
     bi7[3] = d(bi7[1])/da = (I20*I00-I10**2)/I00**2
     bi7[4] = d(bi7[1])/db = (I11*I00-I10*I01)/I00**2
     bi7[5] = d(bi7[2])/da = (I11*I00-I10*I01)/I00**2
     bi7[6] = d(bi7[2])/db = (I02*I00-I01**2)/I00**2
   The integrals are calculated by transforming to y=a*ln(xc/a), and
   then using Gauss-Laguerre quadrature:

   Int( x**(a-1)*(1-x)**(b-1) * f(x) , x=0..xc )

   transforms to

   xc**a
   ----- * Int( exp(-y)*(1-xc*exp(-y/a))**(b-1)*f(xc*exp(-y/a)), y=0..infty )
     a

   The return value of this function is -1 if an error occurred, and
   is 0 if all is good.
-------------------------------------------------------------------------*/

static int bi7func( double a , double b , double xc , double * bi7 )
{
#define NL 20  /* must be between 2 and 20 - see cs_laguerre.c */

   static double *yy=NULL , *ww=NULL ;
   double xx , s00,s10,s01,s20,s11,s02 , ff , l0,l1 ;
   register int ii ;

   if( a  <= 0.0 || b  <= 0.0 ||
       xc <= 0.0 || xc >= 1.0 || bi7 == NULL ) return -1 ;

   if( yy == NULL ) get_laguerre_table( NL , &yy , &ww ) ;

   s00=s10=s01=s20=s11=s02 = 0.0 ;
   for( ii=NL-1 ; ii >= 0 ; ii-- ){
      xx = xc*exp(-yy[ii]/a) ;            /* x transformed from y */
      l0 = log(xx) ; l1 = log(1.0-xx) ;   /* logarithms for Ipq sums */
      ff = pow(1.0-xx,b-1.0) ;            /* (1-x)**(b-1) */
      s00 += ww[ii] * ff ;                /* spq = Ipq sum */
      s10 += ww[ii] * ff * l0 ;
      s20 += ww[ii] * ff * l0 * l0 ;
      s01 += ww[ii] * ff * l1 ;
      s02 += ww[ii] * ff * l1 * l1 ;
      s11 += ww[ii] * ff * l0 * l1 ;
   }

   if( s00 <= 0.0 ) return -1 ;

   bi7[0] = s00 * pow(xc,a) / a ;           /* normalizer */
   bi7[1] = s10/s00 ;                       /* R0 */
   bi7[2] = s01/s00 ;                       /* R1 */
   bi7[3] = (s20*s00-s10*s10)/(s00*s00) ;   /* dR0/da */
   bi7[4] = (s11*s00-s10*s01)/(s00*s00) ;   /* dR0/db */
   bi7[5] = bi7[4] ;                        /* dR1/da */
   bi7[6] = (s02*s00-s01*s01)/(s00*s00) ;   /* dR1/db */

   return 0 ;
}

/*-----------------------------------------------------------------------*/

#define LL   0.2
#define UL   10000.0

static double AL   = 0.21 ;
static double AU   = 9.9 ;
static double BL   = 5.9 ;
static double BU   = 999.9 ;
static int    NRAN = 6666 ;

static void betarange( double al,double au , double bl , double bu , int nran )
{
   if( al > 0.0 ) AL = al ;
   if( au > AL  ) AU = au ;
   if( bl > 0.0 ) BL = bl ;
   if( bu > BL  ) BU = bu ;
   if( nran > 1 ) NRAN = nran ;
}

static int betasolve( double e0, double e1, double xc, double * ap, double * bp )
{
   double bi7[7] , aa,bb , da,db , m11,m12,m21,m22 , r1,r2 , dd,ee ;
   int nite=0 , ii,jj ;

   if( ap == NULL || bp == NULL ||
       xc <= 0.0  || xc >= 1.0  || e0 >= 0.0 || e1 >= 0.0 ) return -1 ;

   dd = 1.e+20 ; aa = bb = 0.0 ;
   for( jj=0 ; jj < NRAN ; jj++ ){
      da = AL +(AU-AL) * drand48() ;
      db = BL +(BU-BL) * drand48() ;
      ii = bi7func( da , db , xc , bi7 ) ; if( ii ) continue ;
      r1 = bi7[1] - e0 ; r2 = bi7[2] - e1 ;
      ee = fabs(r1/e0) + fabs(r2/e1) ;
      if( ee < dd ){ aa=da ; bb=db ; dd=ee ; }
   }
   if( aa == 0.0 || bb == 0.0 ) return -1 ;
#if 0
   fprintf(stderr,"%2d: aa=%15.10g  bb=%15.10g  ee=%g\n",nite,aa,bb,ee) ;
#endif

   do{
      ii = bi7func( aa , bb , xc , bi7 ) ;
      if( ii ) return -1 ;
      r1  = bi7[1] - e0 ;
      r2  = bi7[2] - e1 ; ee = fabs(r1/e0) + fabs(r2/e1) ;
      m11 = bi7[3] ; m12 = bi7[4] ; m21 = bi7[5] ; m22 = bi7[6] ;
      dd  = m11*m22 - m12*m21 ;
      if( dd == 0.0 ) return -1 ;
      da = ( m22*r1 - m12*r2 ) / dd ;
      db = (-m21*r1 + m11*r2 ) / dd ;
      nite++ ;
      aa -= da ; bb -=db ;
      if( aa < LL ) aa = LL ; else if( aa > UL ) aa = UL ;
      if( bb < LL ) bb = LL ; else if( bb > UL ) bb = UL ;
#if 0
      fprintf(stderr,"%2d: aa=%15.10g  bb=%15.10g  ee=%g\n",nite,aa,bb,ee) ;
#endif

      if( aa == LL || bb == LL || aa == UL || bb == UL ) return -1 ;
   } while( fabs(da)+fabs(db) > 0.02 ) ;

   *ap = aa ; *bp = bb ; return 0 ;
}

/*--------------------------------------------------------------------*/

char * BFIT_main( PLUGIN_interface * plint )
{
   MCW_idcode * idc ;
   THD_3dim_dataset * input_dset , * mask_dset = NULL ;
   int nvals,ival , nran,nvox , nbin , miv , sqr,sqt ;
   float abot,atop,bbot,btop,pcut , eps,eps1 ;
   float *bval , *cval ;
   double e0,e1 , aa,bb,xc  ;

   int mcount,mgood , ii , jj , ibot,itop ;
   float mask_bot=666.0 , mask_top=-666.0 , hbot,htop,dbin ;
   byte * mmm ;
   char buf[THD_MAX_NAME+128] , tbuf[THD_MAX_NAME+128] , * tag ;
   int   * hbin , * jbin,*kbin=NULL , *jist[2] ;
   MRI_IMAGE * flim ;

   double aext=-1.0,bext=-1.0 ;

   /*--------------------------------------------------------------------*/
   /*----- Check inputs from AFNI to see if they are reasonable-ish -----*/

   if( plint == NULL )
      return "************************\n"
             "BFIT_main:  NULL input\n"
             "************************"  ;

   /*-- read 1st line --*/

   PLUTO_next_option(plint) ;
   idc        = PLUTO_get_idcode(plint) ;
   input_dset = PLUTO_find_dset(idc) ;
   if( input_dset == NULL )
      return "****************************\n"
             "BFIT_main: bad input dataset\n"
             "****************************"  ;

   nvox  = DSET_NVOX(input_dset) ;
   nvals = DSET_NVALS(input_dset) ;
   ival  = (int) PLUTO_get_number(plint) ;
   if( ival < 0 || ival >= nvals )
      return "**************************\n"
             "BFIT_main: bad Brick index\n"
             "**************************" ;

   DSET_load(input_dset) ;
   if( DSET_ARRAY(input_dset,0) == NULL )
      return "*****************************\n"
             "BFIT_main: can't load dataset\n"
             "*****************************"  ;

   tag = PLUTO_get_string(plint) ;
   sqr = PLUTO_string_index(tag,NYESNO,YESNO_strings) ;

   /*-- read 2nd line --*/

   PLUTO_next_option(plint) ;
   abot = PLUTO_get_number(plint) ;
   atop = PLUTO_get_number(plint) ;
   if( atop <= abot )
      return "*** atop <= abot! ***" ;

   PLUTO_next_option(plint) ;
   bbot = PLUTO_get_number(plint) ;
   btop = PLUTO_get_number(plint) ;
   if( atop <= abot )
      return "*** btop <= bbot! ***" ;

   PLUTO_next_option(plint) ;
   nran = (int) PLUTO_get_number(plint) ;
   pcut = PLUTO_get_number(plint) ;

   tag = PLUTO_get_string(plint) ;
   sqt = PLUTO_string_index(tag,NYESNO,YESNO_strings) ;

   /*-- read optional lines --*/

   while( (tag=PLUTO_get_optiontag(plint)) != NULL ){

      /*-- Mask itself --*/

      if( strcmp(tag,"Mask") == 0 ){

         idc       = PLUTO_get_idcode(plint) ;
         mask_dset = PLUTO_find_dset(idc) ;

         if( mask_dset == NULL ){
            return "******************************\n"
                   "BFIT_main:  bad mask dataset\n"
                   "******************************"  ;
         }

         if( DSET_NVOX(mask_dset) != nvox ){
           return "************************************************************\n"
                  "BFIT_main: mask input dataset doesn't match source dataset\n"
                  "************************************************************" ;
         }

         miv = (int) PLUTO_get_number(plint) ;
         if( miv >= DSET_NVALS(mask_dset) || miv < 0 ){
            return "****************************************************\n"
                   "BFIT_main: mask dataset sub-brick index is illegal\n"
                   "****************************************************"  ;
         }

         DSET_load(mask_dset) ;
         if( DSET_ARRAY(mask_dset,miv) == NULL ){
            return "*************************************\n"
                   "BFIT_main:  can't load mask dataset\n"
                   "*************************************"  ;
         }
         continue ;
      }

      /*-- Mask range of values --*/

      if( strcmp(tag,"Range") == 0 ){
         if( mask_dset == NULL ){
            return "******************************************\n"
                   "BFIT_main:  Can't use Range without Mask\n"
                   "******************************************"  ;
         }

         mask_bot = PLUTO_get_number(plint) ;
         mask_top = PLUTO_get_number(plint) ;
         continue ;
      }

      /*-- Extra plot --*/

      if( strcmp(tag,"Extra") == 0 ){
         aext = PLUTO_get_number(plint) ;
         bext = PLUTO_get_number(plint) ;
         continue ;
      }
   }

   /*------------------------------------------------------*/
   /*---------- At this point, the inputs are OK ----------*/

   /*-- build a byte mask array --*/

   if( mask_dset == NULL ){
      mmm = (byte *) malloc( sizeof(byte) * nvox ) ;
      if( mmm == NULL )
         return " \n*** Can't malloc workspace! ***\n" ;
      memset( mmm , 1, nvox ) ; mcount = nvox ;
   } else {

      mmm = THD_makemask( mask_dset , miv , mask_bot , mask_top ) ;
      if( mmm == NULL )
         return " \n*** Can't make mask for some reason! ***\n" ;
      mcount = THD_countmask( nvox , mmm ) ;

      if( !EQUIV_DSETS(mask_dset,input_dset) ) DSET_unload(mask_dset) ;
      if( mcount < 999 ){
         free(mmm) ;
         return " \n*** Less than 999 voxels survive the mask! ***\n" ;
      }
   }

   /*-- load values into bval --*/

   bval = (float *) malloc( sizeof(float) * mcount ) ;

   switch( DSET_BRICK_TYPE(input_dset,ival) ){

         case MRI_short:{
            short * bar = (short *) DSET_ARRAY(input_dset,ival) ;
            float mfac = DSET_BRICK_FACTOR(input_dset,ival) ;
            if( mfac == 0.0 ) mfac = 1.0 ;
            for( ii=jj=0 ; ii < nvox ; ii++ )
               if( mmm[ii] ) bval[jj++] = mfac*bar[ii] ;
         }
         break ;

         case MRI_byte:{
            byte * bar = (byte *) DSET_ARRAY(input_dset,ival) ;
            float mfac = DSET_BRICK_FACTOR(input_dset,ival) ;
            if( mfac == 0.0 ) mfac = 1.0 ;
            for( ii=jj=0 ; ii < nvox ; ii++ )
               if( mmm[ii] ) bval[jj++] = mfac*bar[ii] ;
         }
         break ;

         case MRI_float:{
            float * bar = (float *) DSET_ARRAY(input_dset,ival) ;
            float mfac = DSET_BRICK_FACTOR(input_dset,ival) ;
            if( mfac == 0.0 ) mfac = 1.0 ;
               for( ii=jj=0 ; ii < nvox ; ii++ )
                  if( mmm[ii] ) bval[jj++] = mfac*bar[ii] ;
         }
         break ;
   }
   free(mmm) ;

   if( sqr ){
      cval = (float *) malloc( sizeof(float) * mcount ) ;
      for( ii=0 ; ii < mcount ; ii++ ){
         cval[ii] = bval[ii] ;
         bval[ii] = bval[ii]*bval[ii] ;
      }
      qsort_floatfloat( mcount , bval , cval ) ;
   } else {
      cval = NULL ;
      qsort_float( mcount , bval ) ;
   }

   if( bval[mcount-1] > 1.0 ){
      free(bval) ; if(cval!=NULL) free(cval) ;
      return "*** beta values > 1.0 exist! ***" ;
   }
   if( bval[0] < 0.0 ){
      free(bval) ; if(cval!=NULL) free(cval) ;
      return "*** beta values < 0.0 exist! ***" ;
   }

   for( ibot=0; ibot<mcount && bval[ibot]<=0.0; ibot++ ) ; /* find 1st bval > 0 */

   itop  = (int)( ibot + 0.01*pcut*(mcount-ibot) + 0.5 ) ;
   mgood = itop - ibot ;
   if( mgood < 999 ){
      free(bval) ; if(cval!=NULL) free(cval) ;
      return "*** not enough positive values! ***" ;
   }

   xc = bval[itop-1] ;

#if 0
   fprintf(stderr,"+++ mcount=%d ibot=%d itop=%d xc=%g\n",mcount,ibot,itop,xc) ;
#endif

   e0 = e1 = 0.0 ;
   for( ii=ibot ; ii < itop ; ii++ ){
     e0 += log(bval[ii]) ; e1 += log(1.0-bval[ii]) ;
   }
   e0 /= mgood ; e1 /= mgood ;

   betarange( abot , atop , bbot , btop ,  nran ) ;
   betasolve( e0,e1,xc , &aa,&bb );

   /*+++ At this point, could do some bootstrap to
         estimate how good the estimates aa and bb are
         --- this is work for when I return from NIH trip +++*/

   eps1 = mgood / ( (mcount-ibot)*(1.0-beta_t2p(xc,aa,bb)) ) ;
   eps  = 1.0-eps1 ;
#if 0
   fprintf(stderr,"+++ eps1=%g\n",eps1) ;
#endif
   if( eps1 > 1.0 ) eps1 = 1.0 ;
   eps1 = (mcount-ibot) * eps1 ;

   /*-- do histogram --*/

   if( !sqr ){
      hbot = 0.0 ; htop = 1.0 ; nbin = 200 ;
      if( bval[mcount-1] < 1.0 ) htop = bval[mcount-1] ;
      dbin = (htop-hbot)/nbin ;

      hbin = (int *) calloc((nbin+1),sizeof(int)) ;
      jbin = (int *) calloc((nbin+1),sizeof(int)) ;

      for( ii=0 ; ii < nbin ; ii++ ){  /* beta fit */
         jbin[ii] = (int)( eps1 * ( beta_t2p(hbot+ii*dbin,aa,bb)
                                   -beta_t2p(hbot+ii*dbin+dbin,aa,bb) ) ) ;
      }

      jist[0] = jbin ;

      flim = mri_new_vol_empty( mcount-ibot,1,1 , MRI_float ) ;
      mri_fix_data_pointer( bval+ibot , flim ) ;
      mri_histogram( flim , hbot,htop , TRUE , nbin,hbin ) ;

      if( aext > 0.0 ){
         kbin = (int *) calloc((nbin+1),sizeof(int)) ;
         jist[1] = kbin ;
         for( ii=0 ; ii < nbin ; ii++ ){  /* beta fit */
            kbin[ii] = (int)( eps1 * ( beta_t2p(hbot+ii*dbin,aext,bext)
                                      -beta_t2p(hbot+ii*dbin+dbin,aext,bext) ) ) ;
         }
      }
   } else {
      double hb,ht ;
      htop = 1.0 ; nbin = 200 ;
      if( bval[mcount-1] < 1.0 ) htop = sqrt(bval[mcount-1]) ;
      hbot = -htop ;
      dbin = (htop-hbot)/nbin ;

      hbin = (int *) calloc((nbin+1),sizeof(int)) ;
      jbin = (int *) calloc((nbin+1),sizeof(int)) ;

      for( ii=0 ; ii < nbin ; ii++ ){  /* beta fit */
         hb = hbot+ii*dbin ; ht = hb+dbin ;
         hb = hb*hb ; ht = ht*ht ;
         if( hb > ht ){ double qq=hb ; hb=ht ; ht=qq ; }
         jbin[ii] = (int)( 0.5*eps1 * ( beta_t2p(hb,aa,bb)
                                       -beta_t2p(ht,aa,bb) ) ) ;
      }

      jist[0] = jbin ;

      flim = mri_new_vol_empty( mcount-ibot,1,1 , MRI_float ) ;
      mri_fix_data_pointer( cval+ibot , flim ) ;
      mri_histogram( flim , hbot,htop , TRUE , nbin,hbin ) ;

      if( aext > 0.0 ){
         kbin = (int *) calloc((nbin+1),sizeof(int)) ;
         jist[1] = kbin ;
         for( ii=0 ; ii < nbin ; ii++ ){  /* beta fit */
            hb = hbot+ii*dbin ; ht = hb+dbin ;
            hb = hb*hb ; ht = ht*ht ;
            if( hb > ht ){ double qq=hb ; hb=ht ; ht=qq ; }
            kbin[ii] = (int)( 0.5*eps1 * ( beta_t2p(hb,aext,bext)
                                          -beta_t2p(ht,aext,bext) ) ) ;
         }
      }
   }

   sprintf(buf,"%s[%d] a=%.2f b=%.2f \\epsilon=%.2f %%=%.0f",
           DSET_FILECODE(input_dset),ival,aa,bb,eps,pcut ) ;

   if( sqt ){
      for( ii=0 ; ii < nbin ; ii++ ){
         hbin[ii] = (int) sqrt( (double)(100*hbin[ii]+0.5) ) ;
         jbin[ii] = (int) sqrt( (double)(100*jbin[ii]+0.5) ) ;
         if( kbin!=NULL )
            kbin[ii] = (int) sqrt( (double)(100*kbin[ii]+0.5) ) ;
      }
   }

   sprintf(tbuf,"Betafit: cutoff=%.2f", (sqr)?sqrt(xc):xc ) ;

   PLUTO_histoplot( nbin,hbot,htop,hbin ,
                    tbuf,NULL,buf , (kbin==NULL)?1:2 , jist ) ;

   mri_clear_data_pointer(flim) ; mri_free(flim) ;
   free(bval) ; free(hbin) ; free(jbin) ;
   if( cval != NULL ) free(cval);
   if( kbin != NULL ) free(kbin);
   return NULL ;
}
