/* === S Y N F I G ========================================================= */
/*!	\file trgt_dv.cpp
**	\brief ppm Target Module
**
**	$Id$
**
**	\legal
**	Copyright (c) 2002-2005 Robert B. Quattlebaum Jr., Adrian Bentley
**
**	This package is free software; you can redistribute it and/or
**	modify it under the terms of the GNU General Public License as
**	published by the Free Software Foundation; either version 2 of
**	the License, or (at your option) any later version.
**
**	This package is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
**	General Public License for more details.
**	\endlegal
**
** === N O T E S ===========================================================
**
** ========================================================================= */

/* === H E A D E R S ======================================================= */

#define SYNFIG_TARGET

#ifdef USING_PCH
#	include "pch.h"
#else
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <ETL/stringf>
#include "trgt_dv.h"
#include <stdio.h>
#include <algorithm>
#include <functional>
#include <ETL/clock>

#endif

/* === M A C R O S ========================================================= */

using namespace synfig;
using namespace std;
using namespace etl;

/* === G L O B A L S ======================================================= */

SYNFIG_TARGET_INIT(dv_trgt);
SYNFIG_TARGET_SET_NAME(dv_trgt,"dv");
SYNFIG_TARGET_SET_EXT(dv_trgt,"dv");
SYNFIG_TARGET_SET_VERSION(dv_trgt,"0.1");
SYNFIG_TARGET_SET_CVS_ID(dv_trgt,"$Id$");

/* === M E T H O D S ======================================================= */


dv_trgt::dv_trgt(const char *Filename)
{
	file=NULL;
	filename=Filename;
	buffer=NULL;
	wide_aspect=false;
	color_buffer=0;
		set_remove_alpha();

}

dv_trgt::~dv_trgt()
{
	if(file)
		pclose(file);
	file=NULL;
	delete [] buffer;
	delete [] color_buffer;
}

bool
dv_trgt::set_rend_desc(RendDesc *given_desc)
{
	// Set the aspect ratio
	if(wide_aspect)
	{
		// 16:9 Aspect
		given_desc->set_wh(160,90);

		// Widescreen should be progressive scan
		given_desc->set_interlaced(false);
	}
	else
	{
		// 4:3 Aspect
		given_desc->set_wh(400,300);

		// We should be interlaced
		given_desc->set_interlaced(true);
	}

	// but the pixel res should be 720x480
	given_desc->clear_flags(),given_desc->set_wh(720,480);

	// NTSC Frame rate is 29.97
	given_desc->set_frame_rate(29.97);

	// The pipe to encodedv is PPM, which needs RGB data
	//given_desc->set_pixel_format(PF_RGB);

	// Set the description
	desc=*given_desc;

	return true;
}

bool
dv_trgt::init()
{
	imagecount=desc.get_frame_start();

	string command;

	int p[2];
  
	if (pipe(p)) {
		synfig::error(_("Unable to open pipe to encodedv"));
		return false;
	};
  
	pid_t pid = fork();
  
	if (pid == -1) {
		synfig::error(_("Unable to open pipe to encodedv"));
		return false;
	}
  
	if (pid == 0){
		// Child process
		// Dup pipeout to stdin
		if( dup2( p[0], STDIN_FILENO ) == -1 ){
			synfig::error(_("Unable to open pipe to encodedv"));
			return false;
		}
		
		// Open filename to stdout
		FILE* outfile = fopen(filename.c_str(),"wb");
		if( outfile == NULL ){
			synfig::error(_("Unable to open pipe to encodedv"));
			return false;
		}
		int outfilefd = fileno(outfile);
		if( outfilefd == -1 ){
			synfig::error(_("Unable to open pipe to encodedv"));
			return false;
		}
		if( dup2( outfilefd, STDOUT_FILENO ) == -1 ){
			synfig::error(_("Unable to open pipe to encodedv"));
			return false;
		}
		
		if(wide_aspect)
			execlp("encodedv", "encodedv", "-w", "1", "-", (const char *)NULL);
		else
			execlp("encodedv", "encodedv", "-", (const char *)NULL);
		// We should never reach here unless the exec failed
		synfig::error(_("Unable to open pipe to encodedv"));
		return false;
	} else {
		// Parent process
		// Close pipein, not needed
		close(p[0]);
		// Save pipeout to file handle, will write to it later
		file = fdopen(p[1], "wb");
		if (file == NULL) {
			synfig::error(_("Unable to open pipe to encodedv"));
			return false;
		}
	}

	// Sleep for a moment to let the pipe catch up
	etl::clock().sleep(0.25f);

	return true;
}

void
dv_trgt::end_frame()
{
	fprintf(file, " ");
	fflush(file);
	imagecount++;
}

bool
dv_trgt::start_frame(synfig::ProgressCallback */*callback*/)
{
	int w=desc.get_w(),h=desc.get_h();

	if(!file)
		return false;

	fprintf(file, "P6\n");
	fprintf(file, "%d %d\n", w, h);
	fprintf(file, "%d\n", 255);

	delete [] buffer;
	buffer=new unsigned char[3*w];

	delete [] color_buffer;
	color_buffer=new Color[w];

	return true;
}

Color *
dv_trgt::start_scanline(int /*scanline*/)
{
	return color_buffer;
}

bool
dv_trgt::end_scanline()
{
	if(!file)
		return false;

	convert_color_format(buffer, color_buffer, desc.get_w(), PF_RGB, gamma());

	if(!fwrite(buffer,1,desc.get_w()*3,file))
		return false;

	return true;
}
