/*-----------------------------------------------------------
 * FILE:        B0Shim
 * DESCRIPTION: Controller software for B0 shimming
 *---------------------------------------------------------*/

#include "include/FileHandling.h"
#include "include/Controller.h"
#include "include/INIReader.h"

//-----------------------------------------------------------
//                        MAIN
//-----------------------------------------------------------

int main (int argc, char *argv[])
{
	int   status;
	float dvals = 0.0;			// Debug

	char *configFile = (char *) "config.ini";

	//------------------------------------------------
	// Parse config file

	INIReader reader (configFile);
	if (reader.ParseError() < 0)
	{
		printf ("Error: could not open config file: %s\n", configFile);
		return 1;
	}

	std::string shimFile      = reader.Get ("Files", "shimvals",      "shimvals.txt");
	std::string preemphFolder = reader.Get ("Files", "preemphFilter", "");
	float estTR = reader.GetReal ("Protocol", "estimateTR", 0.0);

	//---------------------------
	// Read shim values from file

	int  nslices, nchannels;
	printf ("B0Shim: Reading shim values from: %s\n", shimFile.c_str());

//	fmat shimvals = ImportAsciiMatrix (shimFile.c_str(), &nslices, &nchannels, 1);
	fmat shimvals = ImportAsciiMatrix (shimFile.c_str(), &nslices, &nchannels, 0);
	if (shimvals == NULL)
	{
		printf ("B0Shim (Error): Invalid/empty input shim file.");
		return 1;
	}

	int r,c;
	for (r = 0; r != nslices; ++r)
	{
		for(c = 0; c != nchannels; ++c)
			printf("%f ", shimvals[r][c]);
		printf("\n");
	}
	printf("\n");

	//---------------------------
	// Read preemphasis filter from text files

	fcube preemphFilter = NULL;
	int firOrd = 0;
	if (!preemphFolder.empty())
	{
		printf ("%s\n", preemphFolder.c_str());
		preemphFilter = (fcube) calloc (nchannels, sizeof (fmat));

		char buffer[32];
		int nOutputs;
		for (int k = 0; k != nchannels; ++k)
		{
			sprintf (buffer, "%s/ch%i.csv", preemphFolder.c_str(), k);
			preemphFilter[k] = ImportAsciiMatrix (buffer, &nOutputs, &firOrd, 0);
			if (preemphFilter[k] == NULL)
			{
				printf ("Missing preemphasis filter file (%s): Replacing with zeros.\n", buffer);
				preemphFilter[k] = CreateMatrix (nchannels, firOrd);
			}
		}
	}

	//---------------------------
	// Run B0 Shimming

	Controller ctrl  (shimvals, nchannels, nslices, estTR);
	ctrl.SetFilter   (preemphFilter, firOrd >> 1);
	ctrl.FieldCamera (5.0, 1);
//	ctrl.DynamicShim (0);

	free (shimvals);
	if (preemphFilter != NULL)
	{
		free (preemphFilter);
	}
	return status;
}
