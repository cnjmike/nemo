/*
 * MKORBIT:	generate an orbit from observational input variables
 *  
 *      18-apr-05   created        --   peter teuben
 *
 */

#include <stdinc.h>
#include <getparam.h>
#include <vectmath.h>
#include <orbit.h>

string defv[] = {
    "out=???\n		  output filename (an orbit)",
    "lon=\n               Longitude (in degrees)",
    "lat=\n               Latitude (in degrees)",
    "dist=\n              Distance (in kpc)",

    "pmlon=\n             Proper Motion in Longitude (mas/yr)",
    "pmlat=\n             Proper Motion in Latitude (mas/yr)",
    "vrad=\n              Radial velocity (km/s)",

    "R0=8.0\n             LSR distance (in kpc)",
    "V0=220.0\n           LSR velocities (in km/s)",
    "lsr=f\n              Use LSR correction of sun going (-9,12,7)",
    "solar=9,12,7\n       Solar Motion in a right handed UVW system in km/s",
    "coordsys=gal\n       lon/lat coordinate system: equ=RA/DEC gal=GLON/GLAT",

    "time=0.0\n           time",
    "potname=zero\n	  optional potential(5NEMO)",
    "potpars=\n	          .. with optional parameters",
    "potfile=\n		  .. and optional datafile name",
    "headline=\n          random verbiage",
    "VERSION=0.4\n        19-apr-05 PJT",
    NULL,
};

string usage = "Make a galactic orbit with from given initial conditions";

string cvsid="$Id$";



local string	infile,outfile;			/* file names */
local stream  instr,outstr;			/* file streams */

local orbitptr optr;
local a_potential p;

local double x,y,z,u,v,w;
local double etot, lz;
local double tnow, omega;
local double R0, V0;
local double lon, lat, pmlon, pmlat, dist, vrad;

local int  Dpos, Dvel;
local bool Qequ, Qlsr;

local double solar_uvw[3];  /* 9,12,7 */

local string coordsys;

local matrix T;

local matrix T0 = { { -0.0548755604, -0.8734370902,  -0.4838350155},
		    { +0.4941094279, -0.4448296300,  +0.7469822445},
		    { -0.8676661490, -0.1980763734,  +0.4559837762}};

local matrix T1 = { { -0.06699,  -0.87276,  -0.48354},
		    { +0.49273,  -0.45035,  +0.74458},
		    { -0.86760,  -0.18837,  +0.46020}};

local matrix T2;    /* see set_T2 */


local double k = 4.74047;     /*  conversion factor for 1 AU/yr in km/s */


void set_matrix()
{
  int i,j;
  /* pick which one you want: T0, T1, or even T2 */
  SETM(T,T0);

  dprintf(0,"T matrix:\n");
  for (i=0; i<3; i++) {
    for (j=0; j<3; j++)
      dprintf(0,"%g ",T[i][j]);
    dprintf(0,"\n");
  }
    
}

/*
 * ra_ngp  = 12h49m = 192.25
 * dec_ngp = 27.4
 * theta0  = 123.0
 */

void set_T2(double theta, double ra, double dec)
{
  matrix a,d,t,da;
  CLRM(a);
  CLRM(d);
  CLRM(t);

  a[0][0] = cos(ra);
  a[1][1] = -a[0][0];
  a[1][0] = a[0][1] = sin(ra);
  a[2][2] = 1.0;

  d[0][0] = -sin(dec);
  d[2][2] = -d[0][0];
  d[2][0] = d[0][2] = cos(dec);
  d[1][1] = -1.0;

  t[0][0] = cos(theta);
  t[1][1] = -a[0][0];
  t[1][0] = a[0][1] = sin(theta);
  t[2][2] = 1.0;

  MULM(da,d,a);
  MULM(T2,t,da);
}

void equ2gal(double ra, double dec, double *glon, double *glat, double *vel)
{
  matrix a,d, A, B;
  vector xx, yy;
  double cosa = cos(ra);
  double sina = sin(ra);
  double cosd = cos(dec);
  double sind = sin(dec);

  xx[0] = cosd*cosa;
  xx[1] = cosd*sina;
  xx[2] = sind;

  MULMV(yy,T,xx);
  *glon = atan2(yy[1],yy[0]);
  *glat = atan2(sqrt(sqr(yy[0]) + sqr(yy[1])), yy[2]);


  CLRM(a);
  CLRM(d);

  a[0][0] =  cosa;
  a[1][1] = -cosa;
  a[1][0] = a[0][1] = sina;
  a[2][2] = -1.0;

  d[0][0] =  cosd;
  d[2][2] = -cosd;
  d[2][0] = d[0][2] = -sind;
  d[1][1] = -1.0;

  MULM(A,a,d);    /* */
  MULM(B,T,A);    /* matrix for space velocity computations */


  MULMV(yy,B,vel);
  SETV(vel,yy);
}

void gal2uvw(double glon, double glat, double *vel)
{
  double cosa = cos(lat);
  double sina = sin(lat);
  double coso = cos(lon);
  double sino = sin(lon);
  double u1,v1,w1;
  matrix A;
  vector xx;

  u1 = vel[0]*cosa*coso  - vel[1]*cosa  - vel[2]*sina*coso; 
  v1 = vel[0]*cosa*sino  + vel[1]*sina  + vel[2]*sina*sino;
  w1 = vel[0]*sina                      + vel[2]*cosa; 

  vel[0] = u1;
  vel[1] = v1;
  vel[2] = w1;
}


void setparams()
{
  potproc_double pot;
  double pos[3],acc[3],vel[3],epot;
  int ndim=3;
  int signcount;
  int lzsign;

  set_matrix();
  
  p.name = getparam("potname");
  p.pars = getparam("potpars");
  p.file = getparam("potfile");

  lon = getdparam("lon") * DD2R;         /* can be ra/dec or glon/glat */
  lat = getdparam("lat") * DD2R;         /* but now in radians */

  R0 = getdparam("R0");                  /* in kpc */
  V0 = getdparam("V0");                  /* in km/s */

  dist = getdparam("dist");              /* in kpc */

  vrad = getdparam("vrad");              /* in km/s */
  pmlon = getdparam("pmlon") * k * dist; /* now in km/s; can be in ra/dec or glon/glat */
  pmlat = getdparam("pmlat") * k * dist;

  vel[0] = vrad;
  vel[1] = pmlon;
  vel[2] = pmlat;

  Qlsr = getbparam("lsr");
  if (nemoinpd(getparam("solar"),solar_uvw,3) != 3)
    error("solar=%s must contain 3 numbers: UVW for solar motion",getparam("solar"));

  coordsys = getparam("coordsys");
  if (streq(coordsys,"gal"))
    Qequ = FALSE;
  else if (streq(coordsys,"equ"))
    Qequ = TRUE;
  else
    error("%s: coordsys must be equ(atorial) or gal(actic)",coordsys);

  if (Qequ) {
    double ra = lon;
    double dec = lat;
    dprintf(0,"EQU-VEL: %g %g %g\n",vel[0],vel[1],vel[2]);
    equ2gal(ra,dec,&lon,&lat,vel);
    dprintf(0,"GAL-VEL: %g %g %g\n",vel[0],vel[1],vel[2]);
    dprintf(0,"EQU2GAL: %g %g -> %g %g\n",ra/DD2R, dec/DD2R, lon/DD2R, lat/DD2R);
  } else {
    dprintf(0,"GAL-VEL: %g %g %g\n",vel[0],vel[1],vel[2]);
    gal2uvw(lon,lat,vel);
    dprintf(0,"GAL-UVW: %g %g %g\n",vel[0],vel[1],vel[2]);
    dprintf(0,"    GAL: %g %g\n", lon/DD2R, lat/DD2R);
  }

  tnow = getdparam("time");
  outfile = getparam("out");
  
  pot = get_potential_double(p.name, p.pars, p.file);
  if (pot==NULL) 
    error("potential %s cannot be loaded",p.name);
  omega = get_pattern();
  dprintf(0,"Using pattern speed = %g\n",omega);

  /* sun is at (-R0,0,0) with LSR moving at (0,V0,0) */
  x = -R0 + dist * cos(lat) * cos(lon);
  y =       dist * cos(lat) * sin(lon);
  z =       dist * sin(lat);
  u = vel[0];
  v = vel[1];
  w = vel[2];

  if (Qlsr) {
    u += solar_uvw[0];
    v += solar_uvw[1];
    w += solar_uvw[2];
  }
  pos[0] = x;
  pos[1] = y;
  pos[2] = z;
  (*pot)(&ndim,pos,acc,&epot,&tnow);
  epot -= 0.5*omega*omega*(x*x+y*y);
  etot = epot + 0.5*(u*u+v*v+w*w) - 0.5*omega*omega*(x*x+y*y);
  lz = x*v-y*u;

  if (hasvalue("headline")) set_headline(getparam("headline"));
}

void nemo_main ()
{
  warning("new program, hasn't been tested out well");
  setparams();

  optr = NULL;				/* make an orbit */
  allocate_orbit (&optr, 3, 1);
  Masso(optr)  = 1.0;                     /* and set Mass */
  Torb(optr,0) = tnow;
  Xorb(optr,0) = x;			/* .. positions */
  Yorb(optr,0) = y;
  Zorb(optr,0) = z;
  Uorb(optr,0) = u;			/* .. velocities */
  Vorb(optr,0) = v;
  Worb(optr,0) = w;
  I1(optr) = etot;			/*  energy (zero if not used) */
  I2(optr) = lz;                          /* angular momentum */
  
  dprintf(0,"pos: %f %f %f  \nvel: %f %f %f  \netot: %f\nlz=%f\n",
	  x,y,z,u,v,w,etot,lz);
  dprintf(0,"benchmark uvw=  u = -154  v = -493  w = 97 km/s\n");
  
  outstr = stropen (outfile,"w");		/* write to file */
  put_history(outstr);
  PotName(optr) = p.name;
  PotPars(optr) = p.pars;
  PotFile(optr) = p.file;
  write_orbit(outstr,optr);
  strclose(outstr);
}

