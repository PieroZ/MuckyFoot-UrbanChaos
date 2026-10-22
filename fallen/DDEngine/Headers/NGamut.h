//
// Gamut calculations of polygons.
//

#ifndef _NGAMUT_
#define _NGAMUT_


//
// The size of the gamut mapwho.
//

// Arrays are sized for the biggest map (256x256), the gamut itself is kept
// within the map that's loaded now.
//

extern SLONG PAP_size_hi;

#define NGAMUT_SIZE_MAX		256
#define NGAMUT_SIZE_LO_MAX	64
#define NGAMUT_SIZE			(PAP_size_hi)
#define NGAMUT_SIZE_LO		(PAP_size_hi >> 2)
#define NGAMUT_LO_RATIO		4		// NGAMUT_SIZE / NGAMUT_SIZE_LO

//
// The gamut for squares.
//

typedef struct
{
	SLONG xmin;
	SLONG xmax;

} NGAMUT_Gamut;

extern NGAMUT_Gamut NGAMUT_gamut[NGAMUT_SIZE_MAX];
extern SLONG        NGAMUT_xmin;
extern SLONG        NGAMUT_zmin;
extern SLONG        NGAMUT_zmax;

//
// Function to work out a gamut.
//
// Initialises the gamut.
// Pushes out the gamut along the line. The line can go off the gamut mapwho, but
// the gamut always stays within the gamut mapwho.
//

void NGAMUT_init    (void);
void NGAMUT_add_line(
		float px1,
		float pz1,
		float px2,
		float pz2);

//
// Works out a square gamut of the given radius centered at (x,z)
//

void NGAMUT_view_square(float mid_x, float mid_z, float radius);


//
// Once you've worked out the gamut for squares, this function works
// out the gamut for points.
//

extern NGAMUT_Gamut NGAMUT_point_gamut[NGAMUT_SIZE_MAX];
extern SLONG        NGAMUT_point_zmin;
extern SLONG        NGAMUT_point_zmax;

void NGAMUT_calculate_point_gamut(void);

//
// This is the square gamut expanded by one in each direction.
//

extern NGAMUT_Gamut NGAMUT_out_gamut[NGAMUT_SIZE_MAX];
extern SLONG        NGAMUT_out_zmin;
extern SLONG        NGAMUT_out_zmax;

void NGAMUT_calculate_out_gamut(void);

//
// This is a lower-res gamut. It is guaranteed to encompass all the
// squares in the point gamut.  It is calculated from the out gamut, so
// make sure you have calculated that one already.
//

extern NGAMUT_Gamut NGAMUT_lo_gamut[NGAMUT_SIZE_LO_MAX];
extern SLONG        NGAMUT_lo_zmin;
extern SLONG        NGAMUT_lo_zmax;

void NGAMUT_calculate_lo_gamut(void);


#endif
