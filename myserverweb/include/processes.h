/*
*MyServer
*Copyright (C) 2002,2003,2004 The MyServer Team
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef PROCESSES_H
#define PROCESSES_H

#include "../stdafx.h"
#include "../include/filemanager.h"
#include "../include/stringutils.h"

/*!
*Structure used for start a new process.
*/
#ifndef StartProcInfo_IN
#define StartProcInfo_IN
struct StartProcInfo
{
	/*! STDIN file for new process.  */
	FileHandle stdIn;	
	
	/*! STDOUT file for new process.  */
	FileHandle stdOut;
	
	/*! STDERR file for new process.  */
	FileHandle stdError;
	
	char *cmdLine;
	char *cwd;
	
	/*! added for unix support.  */
	char *cmd;
	char *arg;
	
	void *envString;
};
#endif

int execHiddenProcess(StartProcInfo* spi,u_long timeout=0xFFFFFFFF);
int execConcurrentProcess(StartProcInfo* spi);
int terminateProcess(u_long);
#endif
