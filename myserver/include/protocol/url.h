/* -*- mode: c++ -*- */
/*
MyServer
Copyright (C) 2009 Free Software Foundation, Inc.
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef URL_H
# define URL_H
# include "stdafx.h"

# include <string>

using namespace std;

class Url
{
public:
  Url (string &, u_short port);
  Url (const char*, u_short port);
  string& getProtocol ();
  u_short getPort ();
  string& getResource ();
  string& getHost ();
  string& getQuery ();
  string& getCredentials ();
protected:
  void parse (string &, u_short port);
  string protocol;
  string credentials;
  string host;
  string query;
  u_short port;
  string resource;
};
#endif
