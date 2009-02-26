import xml.dom.minidom
import local
import config
import remote
import remoteSvn
import remoteGenericUrl
import console
import re
import string

class MyServerGet:
    def __init__(self, repositoryFile = config.REP_LIST_FILE, listDir = config.PLUGIN_LIST_FILES_DIR):
        self.__rep = repositoryFile
        self.__listDir = listDir
        self.__repManager = remote.RepositoryManager()
        self.__repManager.addSupportedRepository("svn",remoteSvn.RepositorySvn)
        self.__repManager.addSupportedRepository("http",remoteGenericUrl.RepositoryGenericUrl)
        self.__repManager.addSupportedRepository("ftp",remoteGenericUrl.RepositoryGenericUrl)
        self.__list = []
        self.loadRepositoryList()
        self.__versionRegex = re.compile(r'^([1-2]?[1-9]?[0-9])?(.[1-2]?[0-9]?[0-9])?(.[1-2]?[0-9]?[0-9])?(.[1-2]?[0-9]?[0-9])?$')
        
    
    def __versionConverter(self,versionString):
        numbers = [int(string.replace(dirtyNumber,'.','')) for dirtyNumber in self.__versionRegex.search(versionString).groups() if dirtyNumber != None]
        result = 0
        for i in range(len(numbers)):
            result = numbers[i] << 8*(4-i)
        return result
        
    def loadRepositoryList(self):
        list = xml.dom.minidom.parse(self.__rep)
        repXmlList = list.getElementsByTagName("REPOSITORY")
        self.__list = [local.ListManager(repXml.firstChild.data,self.__listDir) for repXml in repXmlList]
        
    def update(self, arg):
        for list in self.__list:
            rep = self.__repManager.getRepository(list.repository)
            rep = rep(list.repository)
            rep.synchronizeListWithRepository(list)
    
  
    def __search(self,arg):
        results = []
        for list in self.__list:
            plugins = list.getPluginsList()
            for plugin in plugins:
                if string.find(plugin["name"][0]["value"],arg)!=-1:
                    results.append(plugin)
        return results
        
    def search(self,arg):
        arg = " ".join(arg)     
        if config.verbose:
            results = ["- " + plugin["name"][0]["value"] + " (compatible with GNU MyServer version ["+plugin.getMyServerMinVersion() + "," + plugin.getMyServerMinVersion() + "])\n    " + plugin["description"][0]["value"] + "\n    Dependences: " + " ".join(dep["value"] + "[" + dep["min-version"] + "," + dep["max-version"] + "]"  for dep in plugin["depends"]) for plugin in self.__search(arg)]
        else: 
            results = [plugin["name"][0]["value"] + " - " + plugin["description"][0]["value"] for plugin in self.__search(arg)]
        console.writeln("\n".join(results))
    
    def __find(self,arg):
        for list in self.__list:
            plugins = list.getPluginsList()
            for plugin in plugins:
                if string.lower(plugin["name"][0]["value"]) == string.lower(arg):
                    return (plugin,list)
        return [None, None]
    
    def __recursiveDependencesChecker(self,pluginRoot):
        result = []
        errors = []
        for dep in pluginRoot["depends"]:
            (plugin, list) = self.__find(dep["value"])
            if plugin == None:
                errors.append((pluginRoot,"Missing dependence: %s" % (dep["value"])))
                continue
            minversion = self.__versionConverter(dep["min-version"])
            maxversion = self.__versionConverter(dep["max-version"])
            pluginversion = self.__versionConverter(plugin["version"][0]["value"])
            if pluginversion < minversion or pluginversion > maxversion:
                errors.append((pluginRoot,"%s%s is going to be installed, but %s requires version > %s and version < %s" % (dep["value"],plugin["version"][0]["value"],pluginRoot["version"][0]["value"],dep["min-version"],dep["max-version"])))
                continue
            result.append((plugin, list))
            [res, er] = self.__recursiveDependencesChecker(plugin)
            result.extend(res)
            errors.extend(er)
        return (result,errors)    
    
    def install(self,args):
        for arg in args:
            (plugin,list) = self.__find(arg)
            if plugin == None:
                console.writeln("No plugin with this name is present in the plugin database.")
                return None
            
            myserverVersion = self.__versionConverter(config.MYSERVER_VERSION)
            minversion = self.__versionConverter(plugin.getMyServerMinVersion())
            maxversion = self.__versionConverter(plugin.getMyServerMaxVersion())
            if myserverVersion < minversion or myserverVersion > maxversion:
                console.writeln("Plugin incompatible with GNU MyServer installed version.")
                return None
            
            
            toInstall = [(plugin,list)]
            (result,errors) = self.__recursiveDependencesChecker(plugin)
            if len(errors) != 0:
                for (plugin,msg) in errors:
                    console.writeln("Error while install %s: %s" % (plugin["name"][0]["value"],msg))
                return None
            toInstall.extend(result)
            
            console.write ("the following plugins will be installed:\n %s\n do you want to continue?[Y|n] " % (", ".join(plugin["name"][0]["value"] for (plugin,list) in toInstall)))
            resp = string.lower(console.readln())
            
            while not resp in  ("y\n","n\n","\n"):
                console.write ("the following plugins will be installed:\n %s\n do you want to continue?[Y|n] " % (", ".join(plugin["name"][0]["value"] for (plugin,list) in toInstall)))
                resp = string.lower(console.readln())
            if resp == 'n\n':
                console.writeln ("Install aborted.")
                return None
            
            downloadErrors = []
            for (plugin,list) in toInstall:
                rep = self.__repManager.getRepository(list.repository)
                rep = rep(list.repository)
                if not rep.getPluginBinary(list,plugin):
                    downloadErrors.append(plugin)
            
            if len(downloadErrors) != 0:
                console.writeln ("Errors retriving the follow plugins packages: %s" % (" ".join("%s-%s-%s.tar.gz" % (plugin["name"][0]["value"],plugin["version"][0]["value"],config.arch) for plugin in downloadErrors)))
                return None
                
            for (plugin,list) in toInstall:
                filename = config.MYSERVER_PLUGIN_DIR + "/%s-%s-%s.tar.gz" % (plugin["name"][0]["value"],plugin["version"][0]["value"],config.arch)
                import tarfile
                console.writeln("Exctracting plugin package..")
                try:
                    tarf = tarfile.open(filename.encode("ascii"),"r|gz")
                    tarf.extractall (config.MYSERVER_PLUGIN_DIR)
                except Exception:
                    console.writeln("Error while exctracting plugin package!")
                    return None
                import os
                os.remove(filename)
                console.writeln("plugin %s installed." % (plugin["name"][0]["value"]))