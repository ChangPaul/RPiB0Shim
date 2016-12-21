#include "../include/FileHandling.h"

fmat ImportAsciiMatrix (const char* fname, int *nrows, int *ncols, int padRows)
{
	int   r,c;
	char  buf [512];
	char *p;
	fmat  vals;

	// Open file
	FILE *fid = fopen (fname, "r");
	if (fid == NULL)
	{
		return NULL;
	}

	// Count number of rows and columns
	*nrows = 0;
	while (!feof (fid))
	{
		fgets (buf, 512, fid);
		if (*nrows == 0)
		{
			*ncols = 0;
			p = strtok (buf, " ");
			while (p != NULL)
			{
				(*ncols)++;
				p = strtok (NULL, " ");
			}
		}
		(*nrows)++;
	}
	(*nrows) = *nrows - 1 + padRows;

	// Check that rows and columns were not empty
	if (*ncols < 1 || *nrows < 1)
	{
		fclose (fid);
		return NULL;
	}

	// Go to the beginning of the file
	rewind (fid);

	// Read data from file
	vals = CreateMatrix (*nrows, *ncols);
	for (r = 0; r != *nrows; ++r)
	{
		for (c = 0; c != *ncols; ++c)
		{
			if (r < padRows)
			{
				vals[r][c] = 0.0;
			}
			else
			{
				fscanf (fid, "%f ", &(vals[r][c]));
			}
		}
	}

	fclose (fid);
	return vals;
}
