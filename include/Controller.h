#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <time.h>
#include <stdlib.h>
#include <wiringPiSPI.h>
#include <wiringPi.h>
#include <fstream>			// Debug

#define MAX_CH 16			// No. of DAC channels

//------------------------------------------------------------
// This should be exported to Computation.h (when integrating with ConsTru)

typedef float*** fcube;
typedef float**  fmat;
typedef float*   fvec;
fmat  CreateMatrix (const int r, const int c);
fcube CreateCube   (const int s, const int r, const int c);

//-------------------------------------------------------------

static unsigned g_slice;
static time_t   trigTime;
static void     UpdateSlicenum (void);

//-------------------------------------------------------------

class Controller
{
public:
	Controller (fmat shimvals = NULL, int nchannels = 0, int nslices = 0, float extTR = 0);
	~Controller();

	void SetFilter         (fcube filter, int ord);

	int  FieldCamera        (float secDur = 5.0, int useFilter = 0);
	int  DynamicShim        (int useFilter = 0);
private:

	fvec Preemphasis       (fvec vals);

	// Hardware functions
	int  ApplyShimCurrents (fvec vals);
	int  ResetShimCurrents (int useFilter);

private:
	fmat  shimvals;
	int   nchannels, nslices;
	float extTR;
	int   ch_ord [MAX_CH];

	// Circular buffers for preemphasis
	int   circIdx;
	fmat  circSetpoint;
	fcube circInput;

	// Preemphasis filter
	fcube preemphFIR;
	fmat  filterOut;
	int   firSize;
};

#endif
