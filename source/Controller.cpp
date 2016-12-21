#include "../include/Controller.h"

//------------------------------------------------------------
// This should be exported to Computation.h (when integrating with ConsTru)

fmat CreateMatrix (const int nrows, const int ncols)
{
	int i;
	fmat mat = (fmat) calloc (nrows, sizeof (fvec));
	for (i = 0; i != nrows; ++i)
	{
		mat[i] = (fvec) calloc (ncols, sizeof (float));
	}
	return mat;
}


fcube CreateCube (const int nslices, const int nrows, const int ncols)
{
	int s, r;
	fcube cube = (fcube) calloc (nslices, sizeof (fmat));
	for (s = 0; s != nslices; ++s)
	{
		cube[s] = (fmat) calloc (nrows, sizeof (fvec));
		for (r = 0; r != nrows; ++r)
		{
			cube[s][r] = (fvec) calloc (ncols, sizeof (float));
		}
	}
	return cube;
}

/************************************************************
 * FUNCTION:    Controller (constructor)
 * DESCRIPTION:
 ************************************************************/

Controller::Controller (fmat vals, int nch, int ns, float tr) :
    shimvals  (vals),
    nslices   (ns),
    extTR     (tr)
{
	unsigned char sdata[3];
	int init_ord [MAX_CH] = {4, 0, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18};

	//----------------------------------------
	// Initialise channel order

	nchannels = (nch - 3 >= MAX_CH) ? MAX_CH : nch - 3;			// Exclude gradient channels
	nchannels = (nchannels < 1) ? 1 : nchannels;				// Minimum one channel
	for (int i = 0; i != nchannels; ++i)
	{
		ch_ord[i] = init_ord[i];
	}

	//----------------------------------------
	// Initialise hardware

	wiringPiSetup ();

	// Setup ISR: Input trigger
	// pin = 0 (wiringPi pin convention; gpio header pin 11)
	wiringPiISR (0, INT_EDGE_RISING, UpdateSlicenum);

	// Setup Spi: DAC (write to shim amps)
	// channel = 0; speed = 5MHz; mode = 1
	wiringPiSPISetupMode (0, 5e6, 1);

	// Initialise DAC registers: common offsets
//	sdata[0] = 0x02; sdata[1] = 0x0D; sdata[2] = 0x55;				// Common offset must be 8 times smaller than the gain
	sdata[0] = 0x02; sdata[1] = 0x09; sdata[2] = 0xC0;				// Common offset must be 8 times smaller than the gain
	wiringPiSPIDataRW (0, sdata, 3);

	sdata[0] = 0x03; sdata[1] = 0x09; sdata[2] = 0xC0;				// Common offset must be 8 times smaller than the gain
	wiringPiSPIDataRW (0, sdata, 3);

	// Initialise DAC registers: channels gains and offsets
	for (int i = 0; i != MAX_CH; ++i)
	{
		// Init gains: Convert from 12V span to 5V (0x6AAA)
		sdata[0] = 0x48+i; sdata[1] = 0x6A; sdata[2] = 0xAA;		// Gain must be 8 times the common offset
		wiringPiSPIDataRW (0, sdata, 3);

		// Init offsets
		sdata[0] = 0x88+i; sdata[1] = 0x80; sdata[2] = 0x00;
		wiringPiSPIDataRW (0, sdata, 3);
	}

	// Reset DAC outputs
	ResetShimCurrents (0);
}

/************************************************************
 * FUNCTION:    Controller (destructor)
 * DESCRIPTION:
 ************************************************************/
 
Controller::~Controller()
{
	// Free memory
	if (firSize != 0)
	{
		free (circSetpoint);
		free (circInput);
		free (filterOut);
	}
}

/************************************************************
 * FUNCTION:    SetFilter
 * DESCRIPTION:
 ************************************************************/

void Controller::SetFilter (fcube filter, int ord)
{
	int num_ch = (nchannels + 3 > MAX_CH) ? MAX_CH : nchannels + 3;

	// Deep copy filter
	firSize    = ord;
	preemphFIR = CreateCube (num_ch, num_ch, 2*ord);
	for (int r = 0; r != num_ch; ++r)
	{
		for (int c = 0; c != num_ch; ++c)
		{
			for (int k = 0; k != 2*ord; ++k)
			{
				preemphFIR[r][c][k] = filter[r][c][k];
			}
		}
	}

	// Allocate memory for filter
	if (firSize != 0)
	{
		circIdx = 0;
		circSetpoint = CreateMatrix (num_ch, firSize);
		circInput    = CreateCube   (num_ch, num_ch, firSize);
		filterOut    = CreateMatrix (num_ch, num_ch);
	}

	// Debug: Print filter
	printf ("Filter order: %i\n", ord);
	for (int r = 0; r != num_ch; ++r)
	{
		for (int k = 0; k != 2*ord; ++k)
			printf ("%f, ", preemphFIR[4][r][k]);
		printf ("\n");
	}
}

/************************************************************
 * FUNCTION:    FieldCamera
 * DESCRIPTION:
 ************************************************************/

int Controller::FieldCamera (float secDur, int useFilter)
{
	float  TDELAY = 2.5;
//	float TDELAY = 0.01;
	float  dt, dur;
	time_t startTime, prevTrig;

	if (shimvals == NULL || nchannels == 0 || nslices == 0)
	{
		return -1;
	}

	//----------------------------------
	// Iterate through channels

	for (int s = 0; s != nslices; ++s)
	{
		time (&startTime);
		while ((dur = difftime (time (NULL), startTime)) < 2*TDELAY + secDur)
		{
			// Measure time between triggers
			prevTrig = trigTime;
			while ((dt = difftime (trigTime, prevTrig)) == 0)
			{
				// Apply currents (or reset currents)
				(dur >= TDELAY && dur < TDELAY + secDur)
				? ApplyShimCurrents (useFilter ? Preemphasis (shimvals[s]) : shimvals [s])
				: ResetShimCurrents (useFilter);
			}

			// Reset start time (if more than 1s delay between triggers)
			if (dt > 1.0)
			{
				time (&startTime);
			}
		}
		printf ("Slice %i complete\n", s);
	}

	return 0;
}

/************************************************************
 * FUNCTION:    DynamicShim
 * DESCRIPTION:
 ************************************************************/

int Controller::DynamicShim (int useFilter)
{
	float dvals = 0.0;                     // DEBUG
	int  status;
	fvec outvals = NULL;

	if (shimvals == NULL || nchannels == 0 || nslices == 0)
	{
		return -1;
	}

	//----------------------------------
	// Dynamic shimming

	g_slice = 0;
	while (g_slice < nslices + 1)
	{
		switch (useFilter)
		{
			case 1: outvals = Preemphasis (shimvals [g_slice]); break;
			case 2:    // DEBUG: saw-tooth
				if (outvals == NULL)
					outvals = (fvec) calloc (nchannels, sizeof (float));
		        	for (int i = 0; i != nchannels; ++i) outvals[i] = dvals;
	        		dvals  = (dvals >= 2.5) ? -2.5 : dvals + 0.001;
				break;
			default: outvals = shimvals [g_slice];
		}
		status = ApplyShimCurrents (outvals);
//		status = ApplyShimCurrents (useFilter ? Preemphasis (shimvals [g_slice]) : shimvals [g_slice]);
	}

	//----------------------------------
	// Reset shim to zero
	status = ResetShimCurrents (useFilter);

	return status;
}

/************************************************************
 * FUNCTION:    Preemphasis
 * DESCRIPTION:
 ************************************************************/

static int count = 0;
fvec Controller::Preemphasis (fvec vals)
{
	if (preemphFIR == NULL || firSize == 0)
	{
		return vals;
	}

	int num_ch = (nchannels + 3 > MAX_CH) ? MAX_CH : nchannels + 3;
	static fvec outvals = (fvec) calloc (nchannels, sizeof (float));

	//--------------------------------------------
	// Update circular setpoint buffer

	for (int r = 0; r != num_ch; ++r)
	{
		circSetpoint[r][circIdx] = vals[r];
		outvals[r] = 0.0;
	}

	//--------------------------------------------
	// Calculate output from preemphasis filters

	int nextIdx = (circIdx == 0) ? firSize - 1 : circIdx - 1;

	// Iterate setpoints
	for (int r = 0; r != num_ch; ++r)
	{
		// Iterate channel inputs
		for (int c = 0; c != num_ch; ++c)
		{
			// Iterate filter
			filterOut[r][c] = 0;
			for (int i = 0; i != firSize; ++i)
			{
				int idx = (circIdx + i) % firSize;
				filterOut[r][c] += preemphFIR[r][c][i]*circSetpoint[r][idx];
			}
			for (int i = firSize; i != 2*firSize; ++i)
			{
				int idx = (circIdx + i) % firSize;
				filterOut[r][c] -= preemphFIR[r][c][i]*circInput[r][c][idx];
			}

//			filterOut[r][c] = (filterOut[r][c] >  5.0) ?  5.0 : filterOut[r][c];
//			filterOut[r][c] = (filterOut[r][c] < -5.0) ? -5.0 : filterOut[r][c];

			// Update circular buffer value
			circInput[r][c][nextIdx] = filterOut[r][c];

			// Update output values
			outvals[c] += filterOut[r][c];
		}
	}

	// Update circular buffer index
	circIdx = nextIdx;

	return outvals;
}

//-----------------------------------------------------
// FUNCTION:    ApplyShimCurrents
// DESCRIPTION: Applies the shim currents to the shim
//              amplifier (through the DAC).
// INPUTS:      vals - array of shim values for each channel
//                     of the shim amplifier (values are
//                     in Amps).
// RETURN:      Status code
//-----------------------------------------------------

int Controller::ApplyShimCurrents (fvec vals)
{
	int status, num_ch;

	unsigned char txbuf[3];
	int dac_val;

	// Apply shim currents
	num_ch = (nchannels > MAX_CH) ? MAX_CH : nchannels;
	for (int i = 0; i != num_ch; ++i)
	{
		dac_val = (int)(0x199A*vals [ch_ord[i]] + 0x8000);	// 1.0A = 0x199A
		if (dac_val > 0xFFFF)
		{
			dac_val = 0xFFFF;
		}
		if (dac_val < 0x0000)
		{
			dac_val = 0x0000;
		}

		// Write to shim amps
		txbuf[0] = 0xC8 + i;
		txbuf[1] = dac_val >> 8;
		txbuf[2] = dac_val & 0xFF;
		status = wiringPiSPIDataRW (0, txbuf, 3);

//		printf ("Shim ch: %i, term ch: %i\n", i, ch_ord[i]);
	}

	return status;
}

int Controller::ResetShimCurrents (int useFilter)
{
	fvec vals = (fvec) calloc (nchannels, sizeof(float));
	int status = ApplyShimCurrents ((useFilter) ? Preemphasis (vals) : vals);
	free (vals);

	return status;
}

//-----------------------------------------------------
// FUNCTION:    UpdateSlicenum
// DESCRIPTION: Interrupt service routine
//-----------------------------------------------------

void UpdateSlicenum (void)
{
	g_slice++;
	time (&trigTime);
}
