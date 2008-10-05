/*
MyServer
Copyright (C) 2002-2008 Free Software Foundation, Inc.
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


#include <include/conf/security/security_domain.h>


SecurityDomain::SecurityDomain ()
{
  this->name.assign ("");
}

SecurityDomain::SecurityDomain (string& name)
{
  this->name.assign (name);
}
SecurityDomain::SecurityDomain (const char* name)
{
  this->name.assign (name);
}


SecurityDomain::~SecurityDomain ()
{

}

/*!
 *Get the stored value for [name].
 */
string *SecurityDomain::getValue (string &name)
{
  return NULL;
}