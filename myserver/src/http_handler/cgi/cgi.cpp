/*
MyServer
Copyright (C) 2002, 2003, 2004, 2006, 2007, 2008 Free Software Foundation, Inc.
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

#include "stdafx.h"
#include <include/http_handler/cgi/cgi.h>
#include <include/protocol/http/http_headers.h>
#include <include/protocol/http/http.h>
#include <include/protocol/http/http_errors.h>
#include <include/server/server.h>
#include <include/conf/security/security.h>
#include <include/base/base64/mime_utils.h>
#include <include/base/file/file.h>
#include <include/base/file/files_utility.h>
#include <include/base/socket/socket.h>
#include <include/base/utility.h>
#include <include/base/mem_buff/mem_buff.h>
#include <include/filter/filters_chain.h>
#include <include/protocol/http/env/env.h>
#include <include/base/pipe/pipe.h>
#include <include/protocol/http/http_data_handler.h>

#include <string>
#include <sstream>

extern "C" {
#ifdef WIN32
#include <direct.h>
#endif
#include <string.h>
}

using namespace std;

/*!
 *By default use a timeout of 15 seconds on new processes.
 */
int Cgi::cgiTimeout = MYSERVER_SEC(15);

/*!
 *Run the standard CGI and send the result to the client.
 *\param td The HTTP thread context.
 *\param s A pointer to the connection structure.
 *\param scriptpath The script path.
 *\param cgipath The CGI handler path as specified by the MIME type.
 *\param execute Specify if the script has to be executed.
 *\param onlyHeader Specify if send only the HTTP header.
 */
int Cgi::send(HttpThreadContext* td, ConnectionPtr s, 
              const char* scriptpath, const char *cgipath, 
              int execute, int onlyHeader)
{
   /* 
   *Use this flag to check if the CGI executable is 
   *nph (Non Parsed Header).  
   */
  int nph = 0;
  ostringstream cmdLine;
  u_long nbw = 0;
  u_long nbw2 = 0;

  FiltersChain chain;
  Process cgiProc;
  u_long procStartTime;

  StartProcInfo spi;
  string moreArg;
  string tmpCgiPath;
  string tmpScriptPath;
  u_long nBytesRead;
  u_long headerSize = 0;
  bool useChunks = false;
  bool keepalive = false;
  bool headerCompleted = false;
  u_long headerOffset = 0;

  /*!
   *Standard CGI uses STDOUT to output the result and the STDIN 
   *to get other params like in a POST request.
   */
  Pipe stdOutFile;
  File stdInFile;
  int subString = cgipath[0] == '"';
  int len = strlen(cgipath);
  int i;

  td->scriptPath.assign(scriptpath);
  
  
  /* Do not modify the text between " and ".  */

  /* Are we in a string block?  */
  for(i = 1; i < len; i++)
  {
    if(!subString && cgipath[i] == ' ')
      break;
    if(cgipath[i] == '"' && cgipath[i - 1] != '\\')
      subString = !subString;
  }

  checkDataChunks(td, &keepalive, &useChunks);

  /*
   *Save the cgi path and the possible arguments.
   *the (x < len) case is when additional arguments are specified. 
   *If the cgipath is enclosed between " and " do not consider them 
   *when splitting directory and file name.
   */
  if(i < len)
  {
    string tmpString(cgipath);
    int begin = tmpString[0] == '"' ? 1 : 0;
    int end   = tmpString[i] == '"' ? i : i - 1;
    tmpCgiPath.assign(tmpString.substr(begin, end - 1));
    moreArg.assign(tmpString.substr(i, len - 1));  
  }
  else
  {
    int begin = (cgipath[0] == '"') ? 1 : 0;
    int end   = (cgipath[len] == '"') ? len-1 : len;
    tmpCgiPath.assign(&cgipath[begin], end-begin);
    moreArg.assign("");
  }
  FilesUtility::splitPath(tmpCgiPath, td->cgiRoot, td->cgiFile);
    
  tmpScriptPath.assign(scriptpath);
  FilesUtility::splitPath(tmpScriptPath, td->scriptDir, td->scriptFile);
  

  chain.setProtocol(td->http);
  chain.setProtocolData(td);
  chain.setStream(td->connection->socket);
  
  if(execute)
  {
    const char *args = 0;
    if(td->request.uriOpts.length())
      args = td->request.uriOpts.c_str();
    else if(td->pathInfo.length())
      args = &td->pathInfo[1];
    
    if(cgipath && strlen(cgipath))
      cmdLine << td->cgiRoot << "/" << td->cgiFile << " " << moreArg << " " 
              << td->scriptFile <<  (args ? args : "" ) ;
    else
      cmdLine << td->scriptDir << "/" << td->scriptFile << " " 
              << moreArg << " " << (args ? args : "" );

    if(td->scriptFile.length() > 4 && td->scriptFile[0] == 'n'
       && td->scriptFile[1] == 'p' && td->scriptFile[2] == 'h' 
       && td->scriptFile[3] == '-' )
      nph = 1; 
    else
      nph = 0;

    if(cgipath && strlen(cgipath))
    {
      spi.cmd.assign(td->cgiRoot);
      spi.cmd.append("/");
      spi.cmd.append(td->cgiFile);
      spi.arg.assign(moreArg);
      spi.arg.append(" ");
      spi.arg.append(td->scriptFile);
      if(args)
      {
        spi.arg.append(" ");
        spi.arg.append(args);
      }
    }
    else
    {
      spi.cmd.assign(scriptpath);
      spi.arg.assign(moreArg);
      if(args)
      {
        spi.arg.append(" ");
        spi.arg.append(args);
      }
    }

  }
  else
  {
    if(!FilesUtility::fileExists(tmpCgiPath.c_str()))
    {
      td->connection->host->warningsLogRequestAccess(td->id);
      if(tmpCgiPath.length() > 0)
      {
        string msg;
        msg.assign("Cgi: Cannot find the ");
        msg.append(tmpCgiPath);
        msg.append(" executable");
        td->connection->host->warningsLogWrite(msg.c_str());
      }
      else
      {
        td->connection->host->warningsLogWrite(
                                    "Cgi: Executable file not specified");
      }
      td->connection->host->warningsLogTerminateAccess(td->id);    
      td->scriptPath.assign("");
      td->scriptFile.assign("");
      td->scriptDir.assign("");
      chain.clearAllFilters(); 
      return td->http->raiseHTTPError(500);

    }
    

     /* Check if the CGI executable exists.  */
    if(!FilesUtility::fileExists(tmpScriptPath.c_str()))
    {
      td->scriptPath.assign("");
      td->scriptFile.assign("");
      td->scriptDir.assign("");
      chain.clearAllFilters(); 
      return td->http->raiseHTTPError(500);
    }

    spi.arg.assign(moreArg);
    spi.arg.append(" ");
    spi.arg.append(td->scriptFile);    
    
    cmdLine << "\"" << td->cgiRoot << "/" << td->cgiFile << "\" " 
            << moreArg << " " << td->scriptFile;
  
    spi.cmd.assign(td->cgiRoot);
    spi.cmd.append("/");
    spi.cmd.append(td->cgiFile);
    
    if(td->cgiFile.length() > 4 && td->cgiFile[0] == 'n'  
       && td->cgiFile[1] == 'p' && td->cgiFile[2] == 'h' 
       && td->cgiFile[3] == '-' )
      nph = 1;
    else
      nph = 0;
  }

  /*
   *Open the stdout file for the new CGI process. 
   */
  if(stdOutFile.create())
  {
    td->connection->host->warningsLogRequestAccess(td->id);
    td->connection->host->warningsLogWrite
                          ("Cgi: Cannot create CGI stdout file");
    td->connection->host->warningsLogTerminateAccess(td->id);
    chain.clearAllFilters(); 
    return td->http->raiseHTTPError(500);
  }

  /*! Open the stdin file for the new CGI process. */
  if(stdInFile.openFile(td->inputDataPath, 
                        File::MYSERVER_OPEN_READ | File::MYSERVER_OPEN_ALWAYS))
  {
    td->connection->host->warningsLogRequestAccess(td->id);
    td->connection->host->warningsLogWrite("Cgi: Cannot open CGI stdin file");
    td->connection->host->warningsLogTerminateAccess(td->id);
    stdOutFile.close();
    chain.clearAllFilters(); 
    return td->http->raiseHTTPError(500);
  }
  
  /*
   *Build the environment string used by the CGI started
   *by the execHiddenProcess(...) function.
   *Use the td->buffer2 to build the environment string.
   */
  (td->buffer2->getBuffer())[0] = '\0';
  Env::buildEnvironmentString(td, td->buffer2->getBuffer());
  
  /*
   *With this code we execute the CGI process.
   *Fill the StartProcInfo struct with the correct values and use it
   *to run the process.
   */
  spi.cmdLine = cmdLine.str();
  spi.cwd.assign(td->scriptDir);

  spi.stdError = (FileHandle) stdOutFile.getWriteHandle();
  spi.stdIn = (FileHandle) stdInFile.getHandle();
  spi.stdOut = (FileHandle) stdOutFile.getWriteHandle();
  spi.envString = td->buffer2->getBuffer();
  
  if(spi.stdError ==  (FileHandle)-1 || 
       spi.stdIn == (FileHandle)-1 || 
       spi.stdOut == (FileHandle)-1)
  {
    td->connection->host->warningsLogRequestAccess(td->id);
    td->connection->host->warningsLogWrite("Cgi: Invalid base/file/file.handler");
    td->connection->host->warningsLogTerminateAccess(td->id);
    stdOutFile.close();
    chain.clearAllFilters(); 
    return td->http->raiseHTTPError(500);
  }

  /* Execute the CGI process. */
  {
    if( cgiProc.execConcurrentProcess(&spi) == -1)
    {
      stdInFile.closeFile();
      stdOutFile.close();
      td->connection->host->warningsLogRequestAccess(td->id);
      td->connection->host->warningsLogWrite
                                       ("Cgi: Error in the CGI execution");
      td->connection->host->warningsLogTerminateAccess(td->id);
      chain.clearAllFilters(); 
      return td->http->raiseHTTPError(500);
    }
    /* Close the write stream of the pipe on the server.  */
    stdOutFile.closeWrite();  
  }

  /* Reset the buffer2 length counter. */
  td->buffer2->setLength(0);

  /* Read the CGI output.  */
  nBytesRead = 0;

  procStartTime = getTicks();

  headerSize = 0;

  /* Parse initial chunks of data looking for the HTTP header.  */
  while(!headerCompleted && !nph)
  {
    bool term;
    /* Do not try to read using a small buffer as this has some
       bad influence on the performances.  */
    if(td->buffer2->getRealLength() - headerOffset - 1 < 512)
      break;
    
    nBytesRead = 0;
    
    term = stdOutFile.pipeTerminated();
      
    if(stdOutFile.read(td->buffer2->getBuffer() + headerOffset, 
                       td->buffer2->getRealLength() - headerOffset - 1, 
                       &nBytesRead))
    {
      stdInFile.closeFile();
      stdOutFile.close();
      td->connection->host->warningsLogRequestAccess(td->id);
      td->connection->host->warningsLogWrite
        ("Cgi: Error reading from CGI std out file");
      td->connection->host->warningsLogTerminateAccess(td->id);
      chain.clearAllFilters();
      return td->http->raiseHTTPError(500);
    }
      
    if(nBytesRead == 0)
    {
      if((int)(getTicks() - procStartTime) > cgiTimeout)
         break;
      else
      {
        if(term)
          break;
        
        continue;
      }
    }

    headerOffset += nBytesRead;

    if(headerOffset > td->buffersize2 - 5)
      (td->buffer2->getBuffer())[headerOffset] = '\0';
    
    if(headerOffset == 0)
    {
      td->connection->host->warningsLogRequestAccess(td->id);
      td->connection->host->warningsLogWrite("Cgi: Error CGI zero bytes read");
      td->connection->host->warningsLogTerminateAccess(td->id);
      td->http->raiseHTTPError(500);
      stdOutFile.close();
      stdInFile.closeFile();
      chain.clearAllFilters(); 
      cgiProc.terminateProcess();
      return 0;
    }

    /* Standard CGI can include an extra HTTP header.  */
    nbw = 0;
    for(u_long i = std::max(0, (int)headerOffset - (int)nBytesRead - 10); 
        i < headerOffset; i++)
    {
      char *buff = td->buffer2->getBuffer();
      if( (buff[i] == '\r') && (buff[i+1] == '\n') 
          && (buff[i+2] == '\r') && (buff[i+3] == '\n') )
      {
        /*
         *The HTTP header ends with a \r\n\r\n sequence so 
         *determine where it ends and set the header size
         *to i + 4.
         */
        headerSize = i + 4 ;
        headerCompleted = true;
        break;
      }
      else if((buff[i] == '\n') && (buff[i+1] == '\n'))
      {
        /*
         *\n\n case.
         */
        headerSize = i + 2;
        headerCompleted = true;
        break;
      }
    }
  }

  /* Send the header.  */
  if(!nph)
  {
    if(keepalive)
      td->response.connection.assign("keep-alive");

    /* Send the header.  */
    if(headerSize)
      HttpHeaders::buildHTTPResponseHeaderStruct(td->buffer2->getBuffer(),
                                                 &td->response, 
                                                 &(td->nBytesToRead));
    /*
     *If we have not to append the output send data 
     *directly to the client.  
     */
    if(!td->appendOutputs)
    {
      string* location = td->response.getValue("Location", 0);

      /*
       *If it is present Location: foo in the header 
       *send a redirect to `foo'.  
       */
      if(location && location->length())
      {
        td->http->sendHTTPRedirect(location->c_str());
        stdOutFile.close();
        stdInFile.closeFile();
        chain.clearAllFilters(); 
        cgiProc.terminateProcess();
        return 1;
      }

      HttpHeaders::buildHTTPResponseHeader(td->buffer->getBuffer(),
                                           &td->response);

      td->buffer->setLength((u_int)strlen(td->buffer->getBuffer()));

      if(chain.write(td->buffer->getBuffer(),
                     static_cast<int>(td->buffer->getLength()), &nbw2))
      {
        stdInFile.closeFile();
        stdOutFile.close();
        chain.clearAllFilters(); 
        /* Remove the connection on sockets error.  */
        cgiProc.terminateProcess();
        return 0;
      }
    }
  }

  if(!nph && onlyHeader)
  {
    stdOutFile.close();
    stdInFile.closeFile();
    chain.clearAllFilters(); 
    cgiProc.terminateProcess();
    return 1;
  }


  /* Create the output filters chain.  */
  if(td->mime && Server::getInstance()->getFiltersFactory()->chain(&chain,
                                                                   td->mime->filters, 
                                                                   td->connection->socket, 
                                                                   &nbw, 
                                                                   1))
  {
    td->connection->host->warningsLogRequestAccess(td->id);
    td->connection->host->warningsLogWrite("Cgi: Error loading filters");
    td->connection->host->warningsLogTerminateAccess(td->id);
    stdOutFile.close();
    stdInFile.closeFile();
    cgiProc.terminateProcess();
    chain.clearAllFilters(); 
    return td->http->raiseHTTPError(500);
  }

  if(headerOffset - headerSize)
  {
    /* Flush the buffer.  Data from the header parsing can be present.  */
    if(HttpDataHandler::appendDataToHTTPChannel(td, 
                                                td->buffer2->getBuffer() + headerSize, 
                                                headerOffset - headerSize,
                                                &(td->outputData),
                                                &chain,
                                                td->appendOutputs, 
                                                useChunks))
    {
      stdOutFile.close();
      stdInFile.closeFile();
      chain.clearAllFilters(); 
      /* Remove the connection on sockets error.  */
      cgiProc.terminateProcess();
      return 0;    
    }

    nbw += headerOffset - headerSize;
  }

  /* Send the rest of the data until we can read from the pipe.  */
  for(;;)
  {
    int aliveProcess = 0;

    /* Process timeout.  */
    if((int)(getTicks() - procStartTime) > cgiTimeout)
    {
      stdOutFile.close();
      stdInFile.closeFile();
      chain.clearAllFilters(); 
      /* Remove the connection on sockets error.  */
      cgiProc.terminateProcess();
      return 0;       
    }
    
    aliveProcess = cgiProc.isProcessAlive();

    /* Read data from the process standard output file.  */
    if(stdOutFile.read(td->buffer2->getBuffer(), 
                       td->buffer2->getRealLength(), 
                       &nBytesRead))
    {
      stdOutFile.close();
      stdInFile.closeFile();
      chain.clearAllFilters(); 
      /* Remove the connection on sockets error.  */
      cgiProc.terminateProcess();
      return 0;      
    }
    
    if(!aliveProcess && !nBytesRead)
      break;

    if(nBytesRead && 
       HttpDataHandler::appendDataToHTTPChannel(td, 
                                                td->buffer2->getBuffer(),
                                                nBytesRead,
                                                &(td->outputData),
                                                &chain,
                                                td->appendOutputs, 
                                                useChunks))
    {
      stdOutFile.close();
      stdInFile.closeFile();
      chain.clearAllFilters(); 
      /* Remove the connection on sockets error.  */
      cgiProc.terminateProcess();
      return 0;       
    }

    nbw += nBytesRead;
  }
  
  
  /* Send the last null chunk if needed.  */
  if(useChunks && chain.write("0\r\n\r\n", 5, &nbw2))
  {
    stdOutFile.close();
    stdInFile.closeFile();
    chain.clearAllFilters(); 
  
    /* Remove the connection on sockets error.  */
    cgiProc.terminateProcess();
    return 0;       
  }

  /* Update the Content-Length field for logging activity.  */
  td->sentData += nbw;

  chain.clearAllFilters();   

  cgiProc.terminateProcess();

  /* Close the stdin and stdout files used by the CGI.  */
  stdOutFile.close();
  stdInFile.closeFile();

  /* Delete the file only if it was created by the CGI module.  */
  if(!td->inputData.getHandle())
    FilesUtility::deleteFile(td->inputDataPath.c_str());
  
  return 1;  
}

/*!
 *Set the CGI timeout for the new processes.
 *\param nt The new timeout value.
 */
void Cgi::setTimeout(int nt)
{
   cgiTimeout = nt;
}

/*!
 *Get the timeout value for CGI processes.
 */
int Cgi::getTimeout()
{
  return cgiTimeout;
}
