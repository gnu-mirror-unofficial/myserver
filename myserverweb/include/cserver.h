/*
*MyServer
*Copyright (C) 2002 The MyServer Team
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

#ifndef CSERVER_IN
#define CSERVER_IN

#include "../stdafx.h"
#include "../include/clientsThread.h"
#include "../include/utility.h"
#include "../include/cXMLParser.h"
#include "../include/utility.h"
#include "../include/connectionstruct.h"
#include "../include/sockets.h"
#include "../include/MIME_manager.h"
#include "../include/vhosts.h"
#include "../include/protocols_manager.h"
#include "../include/connectionstruct.h"


#ifdef WIN32
unsigned int __stdcall listenServer(void* pParam);
#endif
#ifdef NOT_WIN
void* listenServer(void* pParam);
#endif
#define MAX_ALLOWED_IPs 8
/*
*Defined in myserver.cpp
*/
extern int rebootMyServerConsole;

struct listenThreadArgv
{
	u_long port;
	MYSERVER_SOCKET serverSocket;
	int SSLsocket;
};
class cserver
{
#ifdef WIN32
	friend  unsigned int __stdcall listenServer(void* pParam);
	friend  unsigned int __stdcall startClientsTHREAD(void* pParam);
#endif
#ifdef HAVE_PTHREAD
	friend  void* listenServer(void* pParam);
	friend  void* startClientsTHREAD(void* pParam);
#endif
	friend class ClientsTHREAD;
#ifdef WIN32
	friend LRESULT CALLBACK MainWndProc(HWND,UINT,WPARAM,LPARAM);
#endif
#ifdef WIN32
	friend int __stdcall control_handler (u_long control_type);
#endif
#ifdef NOT_WIN
	friend int control_handler (u_long control_type);
#endif
private:
	void stopThreads();
	int pausing;/*Used when rebooting to load new configuration files*/
	protocols_manager protocols;
	cXMLParser configurationFileManager;
	cXMLParser languageParser;
	u_long nThreads;
	u_long verbosity;
	u_long buffersize;
	u_long buffersize2;
	u_long getNumConnections();
	char ipAddresses[MAX_IP_STRING_LEN*MAX_ALLOWED_IPs];/*!Buffer that contains all the local machine IP values*/
	char serverName[MAX_COMPUTERNAME_LENGTH+1];
	char languageFile[MAX_PATH];
	char path[MAX_PATH];
	char serverAdmin[32];
	void initialize(int);
	LPCONNECTION addConnectionToList(MYSERVER_SOCKET s,MYSERVER_SOCKADDRIN *asock_in,char *ipAddr,char *localIpAddr,int port,int localPort,int);
    u_long nConnections;
	u_long maxConnections;
	void clearAllConnections();
	int deleteConnection(LPCONNECTION,int);
	u_long connectionTimeout;
	u_long socketRcvTimeout;
	u_long maxLogFileSize;
	int createServerAndListener(u_long);
	void loadSettings();
	myserver_mutex *c_mutex;
	LPCONNECTION connectionToParse;
	ClientsTHREAD *threads;
	LPCONNECTION connections;
	void createListenThreads();
	void reboot();
public:
	int stopListening;/*Stop listening threads. Pay attention they are stopped by mustEndServer*/
	cserver();
	~cserver();
	dynamic_protocol* getDynProtocol(char *protocolName);
	int addConnection(MYSERVER_SOCKET,MYSERVER_SOCKADDRIN*);
	int connections_mutex_lock();
	int connections_mutex_unlock();
	LPCONNECTION getConnectionToParse(int);
	LPCONNECTION findConnection(MYSERVER_SOCKET);
	u_long getTimeout();
	char *getAddresses();
	void *envString;
	vhostmanager vhostList;
	MIME_Manager mimeManager;
	char  *getPath();
	u_long getNumThreads();
	char  *getDefaultFilenamePath(u_long ID=0);
	char  *getServerName();
	u_long  getVerbosity();
	char *getServerAdmin();
	int getMaxLogFileSize();
	int  mustUseLogonOption();
	void  setVerbosity(u_long);
	void start();
	void stop();
	void terminate();
}; 
extern class cserver *lserver;
#ifdef WIN32
LRESULT CALLBACK MainWndProc(HWND,UINT,WPARAM,LPARAM); 
#endif

#endif
