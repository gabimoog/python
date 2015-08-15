#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "atomic.h"
#include "python.h"

int mdot_integral_ndom;
int sv_zero_r_ndom;   

/***********************************************************
                                       Space Telescope Science Institute

 Synopsis:
	get_sv_wind_params gets input data which is necessary for a Shlossman & Vitello 
	description of the wind
Arguments:		

Returns:
 
Description:	
	The parameters, one_dom->sv...,  obtained here are only used in the routines in stellar_winds.c
	which calculate the velocity and density of the wind during the initialization process.
	Other portions of the structure, geo defined here are more general purpose.		
Notes:


History:
 	98dec	ksl	Coded and debugged as part of major change in IO structure required when
 				adding a spherical wind
        080518  ksl     60a - geo should contain only cgs units
	11aug	ksl	70b - kluge to get better xscale with compton torus
**************************************************************/

int
get_sv_wind_params (ndom)
    int ndom;
{
  double windmin, windmax, theta_min, theta_max;

  Log ("Creating an SV wind model for a Cataclysmic Variable\n");

  geo.wind_mdot /= (MSOL / YR);	// Convert to MSOL/YR for easy of data entry
  rddoub ("wind.mdot(msol/yr)", &geo.wind_mdot);
  geo.wind_mdot *= MSOL / YR;


  zdom[ndom].sv_rmin = 2.8e9;
  zdom[ndom].sv_rmax = 8.4e9;
  zdom[ndom].sv_thetamin = 20. / RADIAN;
  zdom[ndom].sv_thetamax = 65. / RADIAN;
  zdom[ndom].sv_gamma = 1.;
  zdom[ndom].sv_v_zero = 6e5;		/* velocity at base of wind */
  zdom[ndom].sv_r_scale = 7e10;	/*Accleration length scale for wind */
  zdom[ndom].sv_alpha = 1.5;		/* Accleration scale exponent */
  zdom[ndom].sv_v_infinity = 3;	/* Final speed of wind in units of escape velocity */
  zdom[ndom].sv_lambda = 0.0;		/* Mass loss rate exponent */

  windmin = zdom[ndom].sv_rmin / geo.rstar;
  windmax = zdom[ndom].sv_rmax / geo.rstar;
  rddoub ("sv.diskmin(units_of_rstar)", &windmin);
  rddoub ("sv.diskmax(units_of_rstar)", &windmax);


  zdom[ndom].sv_rmin = windmin * geo.rstar;
  zdom[ndom].sv_rmax = windmax * geo.rstar;

  theta_min = zdom[ndom].sv_thetamin * RADIAN;
  theta_max = zdom[ndom].sv_thetamax * RADIAN;
  rddoub ("sv.thetamin(deg)", &theta_min);
  rddoub ("sv.thetamax(deg)", &theta_max);
  zdom[ndom].sv_thetamin = theta_min / RADIAN;
  zdom[ndom].sv_thetamax = theta_max / RADIAN;

  rddoub ("sv.mdot_r_exponent", &zdom[ndom].sv_lambda);	/* Mass loss rate exponent */
  rddoub ("sv.v_infinity(in_units_of_vescape", &zdom[ndom].sv_v_infinity);	/* Final speed of wind in units of escape velocity */

  rddoub ("sv.acceleration_length(cm)", &zdom[ndom].sv_r_scale);	/*Accleration length scale for wind */
  rddoub ("sv.acceleration_exponent", &zdom[ndom].sv_alpha);	/* Accleration scale exponent */
/* Assign the generic parameters for the wind the generic parameters of the wind */

  zdom[ndom].wind_rmin = geo.rstar;
  zdom[ndom].wind_rmax = geo.rmax;
  zdom[ndom].wind_rho_min = zdom[ndom].sv_rmin;
  zdom[ndom].wind_rho_max = zdom[ndom].sv_rmax;
  zdom[ndom].wind_thetamin = zdom[ndom].sv_thetamin;
  zdom[ndom].wind_thetamax = zdom[ndom].sv_thetamax;

 
  /* if modes.adjust_grid is 1 then we have already adjusted the grid manually */
  if (modes.adjust_grid == 0)
    {
      zdom[ndom].xlog_scale = zdom[ndom].sv_rmin;

      /* !! 70b - This change is to accomodate the torus, but it is not obvious this is the
       * best way to set the scales now. It might be better do do this in make_grid!!  */
      if (geo.compton_torus && geo.compton_torus_rmin < zdom[ndom].xlog_scale)
        {
          zdom[ndom].xlog_scale = zdom[ndom].compton_torus_rmin;
        }

     /*70d - ksl - This change made to give one a chance of being able to do an 
       agn and a CV with the sv model.  The underlying assumption is that the
       effective radius provides a good scale factor in the verticla direction.
       An alternative would be to use sv_rmin.
     */

     //OLD70d  geo.zlog_scale = 1e7;
     zdom[ndom].zlog_scale = geo.rstar;
    }

  

  /*Now calculate the normalization factor for the wind*/
  
  mdot_integral_ndom = ndom;

  geo.mdot_norm =
    qromb (sv_wind_mdot_integral, zdom[ndom].sv_rmin, zdom[ndom].sv_rmax, 1e-6);
  return (0);
}

/***********************************************************
                                       Space Telescope Science Institute

 Synopsis:
	double sv_velocity(x,v) calulates the v of a Schlossman Vitello wind from a position
	x
Arguments:		
	double x[]		the postion for which one desires the velocity
Returns:
	double v[]		the calculated velocity
	
	The amplitude of the velocity is returned 
	
Description:	
		
Notes:

History:
 	98dec	ksl	Coded as part of effort to isolate SV portions of the code.
	01mar	ksl	Moved mdot_norm calculation from sv_rho to get_sv_params() 
			since it doesn't need to be recalulated every time
	04aug	ksl	52 -- Modified to allow for s thick disk, and to return
			the velocity in cartesian rather than cylindrical coords.
			These are the same if x is in xz plane.
	04dec	ksl	54a  -- Minor mod to avoid -O3 warning
	05jul	ksl	56d  -- Corrected error in the direction calculated for
			ptest
 
**************************************************************/

double
sv_velocity (x, v, ndom)
     double x[], v[];
     int ndom;
{
  double r, rzero, theta, speed;
  double ldist, zzz, v_escape, vl;
  struct photon ptest;
  double xtest[3];
  double s;
  double ds_to_disk ();
  DomainPtr one_dom;

  zzz = v_escape = -99.;

  one_dom = &zdom[ndom];

  rzero = sv_find_wind_rzero (ndom, x);
  theta = sv_theta_wind (ndom, rzero);

  r = sqrt (x[0] * x[0] + x[1] * x[1]);
  ldist = sqrt ((r - rzero) * (r - rzero) + x[2] * x[2]);

  /* Calculate the poloidal distance for a vertically extended disk ksl 111124 */
  if (geo.disk_type == 2)
    {
      xtest[0] = r;		// Define xtest in the +z plane
      xtest[1] = 0;
      xtest[2] = fabs (x[2]);
      ptest.x[0] = rzero;	// Define ptest to be the footpoint extended to xy plane
      ptest.x[1] = 0.0;
      ptest.x[2] = EPSILON;
      ptest.lmn[0] = sin (theta);	// 56d -- ptest direction is along stream line
      ptest.lmn[1] = 0.0;
      ptest.lmn[2] = cos (theta);
      s = ds_to_disk (&ptest, 1);
      move_phot (&ptest, s);	// Now move the test photon to  disk surface
      vsub (ptest.x, xtest, xtest);	// Poloidal distance is just the distance beteen these two points.
      ldist = length (x);
    }


  vl = one_dom->sv_v_zero;
  if (ldist > 0)
    {
      zzz = pow (ldist / one_dom->sv_r_scale, one_dom->sv_alpha);

      if (rzero < geo.rstar)
	v_escape = sqrt (2. * G * geo.mstar / geo.rstar);
      else
	v_escape = sqrt (2. * G * geo.mstar / rzero);

      vl =
	one_dom->sv_v_zero + (one_dom->sv_v_infinity * v_escape -
			 one_dom->sv_v_zero) * zzz / (1. + zzz);
    }

  v[0] = vl * sin (theta);

  if (r > 0)
    v[1] = sqrt (G * geo.mstar * rzero) / r;
  else
    v[1] = 0;

  v[2] = vl * cos (theta);

  if (x[2] < 0)			//line added SS Nov 04 - is this correct?
    v[2] *= (-1);
  /* 04aug -- ksl --52 At this point we have calculated the velocity in the xz plane, which
   * is identical to the statement that we have calculated it in
   * cylindrical coordinates.  The next bit projects back to xyz 
   * coordinates if x was not originally in the xz plane.
   */
  if (x[1] != 0.0)
    {
      project_from_cyl_xyz (x, v, xtest);
      stuff_v (xtest, v);
    }


  speed = (sqrt (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]));
  if (sane_check (speed))
    {
      Error ("sv_velocity: x %f %f %f v %f %f %f\n", x[0], x[1], x[2], v[0],
	     v[1], v[2]);
      Error
	("sv_velocity: rzero %f theta %f ldist %f zzz %f v_escape %f vl %f\n",
	 rzero, theta, ldist, zzz, v_escape, vl);
    }


  return (speed);

}

/***********************************************************
                                       Space Telescope Science Institute

 Synopsis:
	double sv_rho(x) calculates the density of an sv_wind at a position x
Arguments:		
	double x[]	the position where for the which one desires the denisty
Returns:
	The density at x is returned in gram/cm**3
	
Description:	
		
Notes:
	52 -- If the disk is thick, our intention is that the stream lines
	trace back to the disk, and have the same properties at that radius
	as they would have had except for a vertical offset.

History:
 	98dec	ksl	Coded as part of effort to isolate SV portions of the code.
 
**************************************************************/

double
sv_rho (ndom, x)
     double x[];
     int ndom;
{
  double r, rzero, theta;
  double ldist;
  double dmdot_da;
  double dtheta_drzero, dr_drzero;

  double v[3], rho;
  struct photon ptest;
  double xtest[3];
  double s;
  DomainPtr one_dom;
  // double ds_to_disk ();

  one_dom = &zdom[ndom];
  sv_velocity (x, v, ndom);

  rzero = sv_find_wind_rzero (ndom, x);
  theta = sv_theta_wind (ndom, rzero);

  r = sqrt (x[0] * x[0] + x[1] * x[1]);
  ldist = sqrt ((r - rzero) * (r - rzero) + x[2] * x[2]);

  if (geo.disk_type == 2)	/* These are corrections for a vertically extended disk */
    {
      xtest[0] = r;		// Define xtest in the +z plane
      xtest[1] = 0;
      xtest[2] = fabs (x[2]);
      ptest.x[0] = rzero;
      ptest.x[1] = 0.0;
      ptest.x[2] = EPSILON;
      ptest.lmn[0] = cos (theta);
      ptest.lmn[1] = 0.0;
      ptest.lmn[2] = sin (theta);
      s = ds_to_disk (&ptest, 1);
      move_phot (&ptest, s);	// Now test photon is at disk surface
      vsub (ptest.x, xtest, xtest);
      ldist = length (xtest);
      rzero = length (ptest.x);
    }


/* Reduced by a factor of 2 to account for radiation on both sides of the disk */
  dmdot_da =
    geo.wind_mdot * pow (rzero,
			 one_dom->sv_lambda) * cos (theta) / geo.mdot_norm / 2.;

/* Although the current definition of sv_theta_wind is continuous, the derivative is not continuous accross the
   outer boundary of the wind and thus dtheta_drzero would appear to change discontinuously.   This created
   large apparent density jumps at the outside edge of the wind. We can't allow that and so we force the derivative to equal
   that at the edge of the wind if you try to calculate the density rho.  ksl 97apr23 */

  if (rzero > one_dom->sv_rmax)
    rzero = one_dom->sv_rmax;
  dtheta_drzero =
    (sv_theta_wind (ndom, rzero) - sv_theta_wind (ndom, (1. - EPSILON) * rzero)) 
     / (EPSILON * rzero);

  dr_drzero = 1. + ldist * dtheta_drzero / cos (theta);
/* Note VS93 eqn 8 is dr/drzero but equation  7 is drzero/dr   ksl 97 apr 19 */
  rho = rzero * dmdot_da / (dr_drzero * r * v[2]);

  return (rho);
}


/***********************************************************
                                       Space Telescope Science Institute

 Synopsis:
	sv_find_wind_rzero(p) locates the radial position in the disk from which the 
	stream line arises.  
 
Arguments:		
	double p[]	    A 3 vector position, and not a photon structure
Returns:
	sv_find_wind_rzero returns the radius of the footpoint of the stream line
 
Description:	
	In our models, which follows Shlossman and Vitello the wind is expected to arise in a cone.  
	However, we have also defined r_o in a sensible way for any position regardless of whether it
	is in the cone or not.   The routine should be accurate even if p[2] is negative, i.e if we
	are trying to calculate r in the -z hemisphere.	

	More specifically, if the point is a distance drho outside of the windcone then rzero
	will be rmax+drho. Alternatively if the point is inside the wind cone then, rzero
	will rmin*rho/rho_min where rho_min is the minimum value of rho to be in the windcone
	at that height.
		
Notes:

History:
 	97jan      ksl	Coding on python began.
	15aug	ksl	Adapted for domains
 
**************************************************************/


double
sv_find_wind_rzero (ndom, p)
     int ndom;
     double p[];		/* Note that p is a 3 vector and not a photon structure */
{
  double x, z;
  double zbrent ();
  double rho_min, rho_max, rho;
  DomainPtr one_dom;

  one_dom = &zdom[ndom];
  /* thetamin and theta max are defined w.r.t  z axis */

  z = fabs (p[2]);		/* This is necessary to get correct answer above
				   and below plane */

  if (z == 0)
    {
      x = (sqrt (p[0] * p[0] + p[1] * p[1]));	// If p is in the xy plane, there is no need to think further
      return (x);
    }


  sv_zero_init (p);		/* This initializes the routine sv_zero_r.  It is not
				   actually needed unless zbrent is called, but it
				   does allow you to check your answer otherwize
				 */
  /* The next lines provide a graceful answer in the case where the
   * position is actually outside the wind so that rzero returned is
   * continuous
   */

  rho_min = one_dom->sv_rmin + z * tan (one_dom->sv_thetamin);
  rho_max = one_dom->sv_rmax + z * tan (one_dom->sv_thetamax);
  rho = sqrt (p[0] * p[0] + p[1] * p[1]);

  if (rho <= rho_min)
    {
      x = one_dom->sv_rmin * rho / rho_min;
      return (x);
    }
  if (rho >= rho_max)
    {
      x = one_dom->sv_rmax + rho - rho_max;
      return (x);
    }

  /* 100 here means that zbrent will end if one has a guess of rzero which is
     correct ot 100 cm */

  /* change the global variable sv_zero_r_ndom before we call zbrent */
  sv_zero_r_ndom = ndom;
  x = zbrent (sv_zero_r, one_dom->sv_rmin, one_dom->sv_rmax, 100.);
  return (x);

}

/***********************************************************
                                       Space Telescope Science Institute

 Synopsis:
	The next two subroutines are used to guess the position in the disk
	from which a streamline arises.  sv_zero_r is the routine called by
	the numerical recipes routine zbrent to walk down on the actual value
	of r in the disk.  sv_zero_init is required to set the portions of the routine
	which do not change in calls from zbrent.  sv_zero_r returns the difference
	between rho actual (sqrt x*x + y *y ) and rho_guessed
 
Arguments:		

Returns:
 
Description:	
	
		
Notes:

History:
 	97jan      ksl	Coding on python began.
 
**************************************************************/

double zero_p[3];

int
sv_zero_init (p)
     double p[];
{
  stuff_v (p, zero_p);
  zero_p[2] = fabs (zero_p[2]);	/* Required to get correct 
				   answer below (in -z ) the disk */
  return (0);
}

/* This routine is used to test whether a guess of r_zero is correct.  If
   you have the answer exactly then sv_zero_r will return 0 */


double
sv_zero_r (r)
     double r;
{
  double theta;
  double rho, rho_guess;

  // sv_zero_r_ndom should be set to the domain number before calling sv_zero_r

  theta = sv_theta_wind (sv_zero_r_ndom, r);

  rho = sqrt (zero_p[0] * zero_p[0] + zero_p[1] * zero_p[1]);
  rho_guess = r + tan (theta) * zero_p[2];
  return (rho_guess - rho);


}

/***********************************************************
                                       Space Telescope Science Institute

 Synopsis:
	double sv_theta_wind(r) finds the angle at which the wind emerges from at a specific
	radius r on surface of the disk
 
Arguments:
	double r	a radial distance on the surface of the disk		

Returns:
	As long as r is between geo.sv_rmin and geo.sv_rmax, sv_theta_wind returns the
	angle given by the SV prescription.
	
	Inside geo.sv_rmin, it returns a value which smoothly goes from thetamin to 0
	as the radius goes to 0
	
	Outside geo.sv_rmax, it returns geo.sv_thetamax
 
Description:	
		
Notes:

History:
 	97jan      ksl	Coding on python began.
 
**************************************************************/

double
sv_theta_wind (ndom, r)
     int ndom;
     double r;
{
  double theta;
  DomainPtr one_dom;

  one_dom = &zdom[ndom];
  
  if (r <= geo.sv_rmin)
    return (atan (tan (one_dom->sv_thetamin * r / one_dom->sv_rmin)));
  if (r >= one_dom->sv_rmax)
    return (one_dom->sv_thetamax);
  theta = one_dom->sv_thetamin +
    (one_dom->sv_thetamax -
     one_dom->sv_thetamin) * pow ((r - one_dom->sv_rmin) / (one_dom->sv_rmax - one_dom->sv_rmin),
			     one_dom->sv_gamma);
  return (theta);

}

/***********************************************************
                                       Space Telescope Science Institute

 Synopsis:
	double sv_wind_mdot_integral(r) is the integrand of SV model for mdot as ation
	of radius
Arguments:		
	double r;
 
Returns:
 
Description:	
	
		
Notes:
	This routine should be further down in the file but at one point this created
	an error for reasons I do not understand (97sept13).  I moved it to the bottom
	of the file on 12/14/97.

History:
 	97jan      ksl	Coding on python began.
 
**************************************************************/


double
sv_wind_mdot_integral (r)
     double r;
{
  double x;
  DomainPtr one_dom;

  one_dom = &zdom[mdot_integral_ndom];
  x = 2 * PI * pow (r, one_dom->sv_lambda + 1.) * cos (sv_theta_wind (mdot_integral_ndom, r));
  return (x);

}
