/*=============================================================================
//	
//  This software has been released under the terms of the GNU Public
//  license. See http://www.gnu.org/copyleft/gpl.html for details.
//
//  Copyright 2001 Anders Johansson ajh@atri.curtin.edu.au
//
//=============================================================================
*/

/* Design and implementation of different types of digital filters

*/
#include <math.h>
#include "dsp.h"

/******************************************************************************
*  FIR filter implementations
******************************************************************************/

/* C implementation of FIR filter y=w*x

   n number of filter taps, where mod(n,4)==0
   w filter taps
   x input signal must be a circular buffer which is indexed backwards 
*/
inline _ftype_t fir(register unsigned int n, _ftype_t* w, _ftype_t* x)
{
  register _ftype_t y; // Output
  y = 0.0; 
  do{
    n--;
    y+=w[n]*x[n];
  }while(n != 0);
  return y;
}

/* C implementation of parallel FIR filter y(k)=w(k) * x(k) (where * denotes convolution)

   n  number of filter taps, where mod(n,4)==0
   d  number of filters
   xi current index in xq
   w  filter taps k by n big
   x  input signal must be a circular buffers which are indexed backwards 
   y  output buffer
   s  output buffer stride
*/
inline _ftype_t* pfir(unsigned int n, unsigned int d, unsigned int xi, _ftype_t** w, _ftype_t** x, _ftype_t* y, unsigned int s)
{
  register _ftype_t* xt = *x + xi;
  register _ftype_t* wt = *w;
  register int    nt = 2*n;
  while(d-- > 0){
    *y = fir(n,wt,xt);
    wt+=n;
    xt+=nt;
    y+=s;
  }
  return y;
}

/* Add new data to circular queue designed to be used with a parallel
   FIR filter, with d filters. xq is the circular queue, in pointing
   at the new samples, xi current index in xq and n the length of the
   filter. xq must be n*2 by k big, s is the index for in.
*/
inline int updatepq(unsigned int n, unsigned int d, unsigned int xi, _ftype_t** xq, _ftype_t* in, unsigned int s)  
{
  register _ftype_t* txq = *xq + xi;
  register int nt = n*2;
  
  while(d-- >0){
    *txq= *(txq+n) = *in;
    txq+=nt;
    in+=s;
  }
  return (++xi)&(n-1);
}

/******************************************************************************
*  FIR filter design
******************************************************************************/

/* Design FIR filter using the Window method

   n     filter length must be odd for HP and BS filters
   w     buffer for the filter taps (must be n long)
   fc    cutoff frequencies (1 for LP and HP, 2 for BP and BS) 
         0 < fc < 1 where 1 <=> Fs/2
   flags window and filter type as defined in filter.h
         variables are ored together: i.e. LP|HAMMING will give a 
	 low pass filter designed using a hamming window  
   opt   beta constant used only when designing using kaiser windows
   
   returns 0 if OK, -1 if fail
*/
int design_fir(unsigned int n, _ftype_t* w, _ftype_t* fc, unsigned int flags, _ftype_t opt)
{
  unsigned int	o   = n & 1;          	// Indicator for odd filter length
  unsigned int	end = ((n + 1) >> 1) - o;       // Loop end
  unsigned int	i;			// Loop index

  _ftype_t k1 = 2 * M_PI;		// 2*pi*fc1
  _ftype_t k2 = 0.5 * (_ftype_t)(1 - o);// Constant used if the filter has even length
  _ftype_t k3;				// 2*pi*fc2 Constant used in BP and BS design
  _ftype_t g  = 0.0;     		// Gain
  _ftype_t t1,t2,t3;     		// Temporary variables
  _ftype_t fc1,fc2;			// Cutoff frequencies

  // Sanity check
  if(!w || (n == 0)) return -1;

  // Get window coefficients
  switch(flags & WINDOW_MASK){
  case(BOXCAR):
    boxcar(n,w); break;
  case(TRIANG):
    triang(n,w); break;
  case(HAMMING):
    hamming(n,w); break;
  case(HANNING):
    hanning(n,w); break;
  case(BLACKMAN):
    blackman(n,w); break;
  case(FLATTOP):
    flattop(n,w); break;
  case(KAISER):
    kaiser(n,w,opt); break;
  default:
    return -1;	
  }

  if(flags & (LP | HP)){ 
    fc1=*fc;
    // Cutoff frequency must be < 0.5 where 0.5 <=> Fs/2
    fc1 = ((fc1 <= 1.0) && (fc1 > 0.0)) ? fc1/2 : 0.25;
    k1 *= fc1;

    if(flags & LP){ // Low pass filter

      // If the filter length is odd, there is one point which is exactly
      // in the middle. The value at this point is 2*fCutoff*sin(x)/x, 
      // where x is zero. To make sure nothing strange happens, we set this
      // value separately.
      if (o){
	w[end] = fc1 * w[end] * 2.0;
	g=w[end];
      }

      // Create filter
      for (i=0 ; i<end ; i++){
	t1 = (_ftype_t)(i+1) - k2;
	w[end-i-1] = w[n-end+i] = w[end-i-1] * sin(k1 * t1)/(M_PI * t1); // Sinc
	g += 2*w[end-i-1]; // Total gain in filter
      }
    }
    else{ // High pass filter
      if (!o) // High pass filters must have odd length
	return -1;
      w[end] = 1.0 - (fc1 * w[end] * 2.0);
      g= w[end];

      // Create filter
      for (i=0 ; i<end ; i++){
	t1 = (_ftype_t)(i+1);
	w[end-i-1] = w[n-end+i] = -1 * w[end-i-1] * sin(k1 * t1)/(M_PI * t1); // Sinc
	g += ((i&1) ? (2*w[end-i-1]) : (-2*w[end-i-1])); // Total gain in filter
      }
    }
  }

  if(flags & (BP | BS)){
    fc1=fc[0];
    fc2=fc[1];
    // Cutoff frequencies must be < 1.0 where 1.0 <=> Fs/2
    fc1 = ((fc1 <= 1.0) && (fc1 > 0.0)) ? fc1/2 : 0.25;
    fc2 = ((fc2 <= 1.0) && (fc2 > 0.0)) ? fc2/2 : 0.25;
    k3  = k1 * fc2; // 2*pi*fc2
    k1 *= fc1;      // 2*pi*fc1

    if(flags & BP){ // Band pass
      // Calculate center tap
      if (o){
	g=w[end]*(fc1+fc2);
	w[end] = (fc2 - fc1) * w[end] * 2.0;
      }

      // Create filter
      for (i=0 ; i<end ; i++){
	t1 = (_ftype_t)(i+1) - k2;
	t2 = sin(k3 * t1)/(M_PI * t1); // Sinc fc2
	t3 = sin(k1 * t1)/(M_PI * t1); // Sinc fc1
	g += w[end-i-1] * (t3 + t2);   // Total gain in filter
	w[end-i-1] = w[n-end+i] = w[end-i-1] * (t2 - t3); 
      }
    }      
    else{ // Band stop
      if (!o) // Band stop filters must have odd length
	return -1;
      w[end] = 1.0 - (fc2 - fc1) * w[end] * 2.0;
      g= w[end];

      // Create filter
      for (i=0 ; i<end ; i++){
	t1 = (_ftype_t)(i+1);
	t2 = sin(k1 * t1)/(M_PI * t1); // Sinc fc1
	t3 = sin(k3 * t1)/(M_PI * t1); // Sinc fc2
	w[end-i-1] = w[n-end+i] = w[end-i-1] * (t2 - t3); 
	g += 2*w[end-i-1]; // Total gain in filter
      }
    }
  }

  // Normalize gain
  g=1/g;
  for (i=0; i<n; i++) 
    w[i] *= g;
  
  return 0;
}

/* Design polyphase FIR filter from prototype filter

   n     length of prototype filter
   k     number of polyphase components
   w     prototype filter taps
   pw    Parallel FIR filter 
   g     Filter gain
   flags FWD forward indexing
         REW reverse indexing
	 ODD multiply every 2nd filter tap by -1 => HP filter

   returns 0 if OK, -1 if fail
*/
int design_pfir(unsigned int n, unsigned int k, _ftype_t* w, _ftype_t** pw, _ftype_t g, unsigned int flags)
{
  int l = (int)n/k;	// Length of individual FIR filters
  int i;     	// Counters
  int j;
  _ftype_t t;	// g * w[i]
  
  // Sanity check
  if(l<1 || k<1 || !w || !pw)
    return -1;

  // Do the stuff
  if(flags&REW){
    for(j=l-1;j>-1;j--){//Columns
      for(i=0;i<(int)k;i++){//Rows
	t=g *  *w++;
	pw[i][j]=t * ((flags & ODD) ? ((j & 1) ? -1 : 1) : 1);
      }
    }
  }
  else{
    for(j=0;j<l;j++){//Columns
      for(i=0;i<(int)k;i++){//Rows
	t=g *  *w++;
	pw[i][j]=t * ((flags & ODD) ? ((j & 1) ? 1 : -1) : 1);
      }
    }
  }
  return -1;
}

/******************************************************************************
*  IIR filter design
******************************************************************************/

/* Helper functions for the bilinear transform */

/* Pre-warp the coefficients of a numerator or denominator.
   Note that a0 is assumed to be 1, so there is no wrapping
   of it.  
*/
void prewarp(_ftype_t* a, _ftype_t fc, _ftype_t fs)
{
  _ftype_t wp;
  wp = 2.0 * fs * tan(M_PI * fc / fs);
  a[2] = a[2]/(wp * wp);
  a[1] = a[1]/wp;
}

/* Transform the numerator and denominator coefficients of s-domain
   biquad section into corresponding z-domain coefficients.
   
   The transfer function for z-domain is:

          1 + alpha1 * z^(-1) + alpha2 * z^(-2)
   H(z) = -------------------------------------
          1 + beta1 * z^(-1) + beta2 * z^(-2)

   Store the 4 IIR coefficients in array pointed by coef in following
   order:
   beta1, beta2    (denominator)
   alpha1, alpha2  (numerator)
   
   Arguments:
   a       - s-domain numerator coefficients
   b       - s-domain denominator coefficients
   k 	   - filter gain factor. Initially set to 1 and modified by each
             biquad section in such a way, as to make it the
             coefficient by which to multiply the overall filter gain
             in order to achieve a desired overall filter gain,
             specified in initial value of k.  
   fs 	   - sampling rate (Hz)
   coef    - array of z-domain coefficients to be filled in.
 
   Return: On return, set coef z-domain coefficients and k to the gain
   required to maintain overall gain = 1.0;
*/
void bilinear(_ftype_t* a, _ftype_t* b, _ftype_t* k, _ftype_t fs, _ftype_t *coef)
{
  _ftype_t ad, bd;

  /* alpha (Numerator in s-domain) */
  ad = 4. * a[2] * fs * fs + 2. * a[1] * fs + a[0];
  /* beta (Denominator in s-domain) */
  bd = 4. * b[2] * fs * fs + 2. * b[1] * fs + b[0];

  /* Update gain constant for this section */
  *k *= ad/bd;

  /* Denominator */
  *coef++ = (2. * b[0] - 8. * b[2] * fs * fs)/bd; /* beta1 */
  *coef++ = (4. * b[2] * fs * fs - 2. * b[1] * fs + b[0])/bd; /* beta2 */

  /* Numerator */
  *coef++ = (2. * a[0] - 8. * a[2] * fs * fs)/ad; /* alpha1 */
  *coef   = (4. * a[2] * fs * fs - 2. * a[1] * fs + a[0])/ad;   /* alpha2 */
}



/* IIR filter design using bilinear transform and prewarp. Transforms
   2nd order s domain analog filter into a digital IIR biquad link. To
   create a filter fill in a, b, Q and fs and make space for coef and k.
   

   Example Butterworth design: 

   Below are Butterworth polynomials, arranged as a series of 2nd
   order sections:

   Note: n is filter order.
   
   n  Polynomials
   -------------------------------------------------------------------
   2  s^2 + 1.4142s + 1
   4  (s^2 + 0.765367s + 1) * (s^2 + 1.847759s + 1)
   6  (s^2 + 0.5176387s + 1) * (s^2 + 1.414214 + 1) * (s^2 + 1.931852s + 1)
   
   For n=4 we have following equation for the filter transfer function:
                       1                              1
   T(s) = --------------------------- * ----------------------------
          s^2 + (1/Q) * 0.765367s + 1   s^2 + (1/Q) * 1.847759s + 1
   
   The filter consists of two 2nd order sections since highest s power
   is 2.  Now we can take the coefficients, or the numbers by which s
   is multiplied and plug them into a standard formula to be used by
   bilinear transform.

   Our standard form for each 2nd order section is:

          a2 * s^2 + a1 * s + a0
   H(s) = ----------------------
          b2 * s^2 + b1 * s + b0

   Note that Butterworth numerator is 1 for all filter sections, which
   means s^2 = 0 and s^1 = 0

   Lets convert standard Butterworth polynomials into this form:

             0 + 0 + 1                  0 + 0 + 1
   --------------------------- * --------------------------
   1 + ((1/Q) * 0.765367) + 1   1 + ((1/Q) * 1.847759) + 1

   Section 1:
   a2 = 0; a1 = 0; a0 = 1;
   b2 = 1; b1 = 0.765367; b0 = 1;

   Section 2:
   a2 = 0; a1 = 0; a0 = 1;
   b2 = 1; b1 = 1.847759; b0 = 1;

   Q is filter quality factor or resonance, in the range of 1 to
   1000. The overall filter Q is a product of all 2nd order stages.
   For example, the 6th order filter (3 stages, or biquads) with
   individual Q of 2 will have filter Q = 2 * 2 * 2 = 8.


   Arguments:
   a       - s-domain numerator coefficients, a[1] is always assumed to be 1.0
   b       - s-domain denominator coefficients
   Q	   - Q value for the filter
   k 	   - filter gain factor. Initially set to 1 and modified by each
             biquad section in such a way, as to make it the
             coefficient by which to multiply the overall filter gain
             in order to achieve a desired overall filter gain,
             specified in initial value of k.  
   fs 	   - sampling rate (Hz)
   coef    - array of z-domain coefficients to be filled in.

   Note: Upon return from each call, the k argument will be set to a
   value, by which to multiply our actual signal in order for the gain
   to be one. On second call to szxform() we provide k that was
   changed by the previous section. During actual audio filtering
   k can be used for gain compensation.

   return -1 if fail 0 if success.
*/
int szxform(_ftype_t* a, _ftype_t* b, _ftype_t Q, _ftype_t fc, _ftype_t fs, _ftype_t *k, _ftype_t *coef)
{
  _ftype_t at[3];
  _ftype_t bt[3];

  if(!a || !b || !k || !coef || (Q>1000.0 || Q< 1.0)) 
    return -1;

  memcpy(at,a,3*sizeof(_ftype_t));
  memcpy(bt,b,3*sizeof(_ftype_t));

  bt[1]/=Q;

  /* Calculate a and b and overwrite the original values */
  prewarp(at, fc, fs);
  prewarp(bt, fc, fs);
  /* Execute bilinear transform */
  bilinear(at, bt, k, fs, coef);

  return 0;
}

