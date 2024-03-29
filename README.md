# rsrcgen
*Windows Version Info and Manifest generator*

:warning: 2.0.0 is broken, pull and revert to previous prebuilt EXEs for usable version :warning:

Generates VERSIONINFO-containing Win32 resource file and WinSxS assembly manifest for your EXE or DLL according to details specified in the .info file, optionally auto-incrementing build number.

To use as part of the build process in Visual Studio, in project properties, navigate to:  
*"Configuration Properties / Build Events / Pre-Build Event"* and add following line to the *"Command Line"* field:

    "$(SolutionDir)rsrcgen.exe" "$(ProjectDir)$(AssemblyName).info" "dir=$(ProjectDir)\" debug=$(UseDebugLibraries) platform=$(PlatformTarget)

You will need to edit the path to the rsrcgen.exe file above.

## Command line
First command line parameter is the .info file (details below). All following parameters should be in ``abc=xyz`` format. These parameters define another variables available in the .info file.

## Info file
The .info file is simple INI file. Open the [rsrcgen.info](rsrcgen.info) file in you favorite text editor as an example. The file contains following sections:

#### [generator]
* ``autoincrement`` - when **true** the rsrcgen will search in ``[description]`` for ``build``, increments it and writes it back into the .info file
* ``manifestcharset`` - character set used for the .manifest file, ``UTF-8`` by default, can be optionally set to ``UTF-16``
* ``versioncharset`` - character set used for the VERSIONINFO .rc file, none (CP_ACP) by default, can be optionally set to ``UTF-16`` or ``UTF-8``
* ``charset`` - optionally sets defaults for ``manifestcharset`` and ``versioncharset`` 

#### [include]
List of additional .info files to search for prefedined variable values in. Example:

    something = ..\..\solution.info
    abcdefghi = ..\dlls\dllcommon.info

Names must be unique but are otherwise ignored, only the path is significant, absolute or relative (to the current .info file)
The INI section names are the same.

#### [description]
This section contains predefined variables.

    name = value

Variables can be referenced in any value in the .info file using ``%name%`` syntax. Evaluation is recursive so beware of endless loops.

Note that command line parameters take precedence.

#### [versioninfo]
Defines details of the VERSIONINFO resource.
* ``filename`` - path to the resulting .rc file; if empty or missing none is generated
* ``language`` - language (LANGID) of the description; use **0x0409** syntax
* ``codepage`` - prefered codepage of single-byte-character string APIs, usually ignored
* ``manifest`` - if the .rc file should reference the manifest created in the last section
* ``dll`` - true/false, false means exe
* ``fileversion`` and ``productversion`` - canonical numeric versions, components separated by commas

#### [versioninfo:values]
Every value defined here is included into the VERSIONINFO resource. Some value names are recognized and translated. Some versions of Windows display all of them, more recent ones display only some of the known. See rsrcgen.info for examples and [MSDN](https://msdn.microsoft.com/en-us/library/windows/desktop/aa381058(v=vs.85).aspx) for list of the recognized strings as well as more details.

#### [versioninfo:ff]
Current build file flags.
* ``debug`` - debug build
* ``prerelease`` - not intended for general use
* ``private`` - private build, specify details in ``PrivateBuild`` in [versioninfo:values]
* ``special`` - special build, specify what is special in ``SpecialBuild`` in [versioninfo:values]

#### [manifest]
Specifies details of generated manifest file.  
For all available options see: https://docs.microsoft.com/en-us/windows/win32/sbscs/application-manifests

* ``filename`` - path to the resulting .manifest file; if empty or missing none is generated
* ``architecture`` - binary architecture, typically passed through command line parameter
  * one of: ``x86``, ``amd64``, ``ia64``, ``arm``, ``arm64``
* ``assemblyVersion``, ``assemblyIdentityName``, ``assemblyDescription`` - version and description
* ``requestedExecutionLevel`` - if present, rsrscgen generates trustInfo block, options are:
  * ``asInvoker``
  * ``highestAvailable`` 
  * ``requireAdministrator``
* ``dpiAware`` - if present, rsrscgen generates application/windowsSettings block
  * ``False``
  * ``True`` - 
  * ``Per monitor`` - per-monitor awareness on Windows 8.1 and later, otherwise same as ``False``
  * ``True/PM`` - per-monitor or fallback to ``True`` on Windows 8 and earlier.
* ``dpiAwareness`` - list of `,`-separated entries, overrides ``dpiAware`` element; supported on Windows 10 build 1607
  * ``Unaware`` - no awareness, disables API to change this programmatically
  * ``System`` -
  * ``PerMonitor`` - 
  * ``PerMonitorV2`` - Windows 10 version 1703, use ``"PerMonitorV2,PerMonitor"``
* ``gdiScaling`` - Windows 10 version 1703 and later
  * ``True`` or ``False``
* ``heapType`` - 
  * ``SegmentHeap`` - Windows 10 version 2004, less memory usage, more CPU usage tradeoff
* ``longPathAware`` - Windows 10 version 1607, if True then MAX_PATH limit no longer applies to number of APIs
  * ``True`` or ``False``
* ``activeCodePage`` - 
  * ``UTF-8`` - Windows 10 version 2004, documentation: [Use UTF-8 code pages in Windows apps](https://docs.microsoft.com/en-us/windows/apps/design/globalizing/use-utf8-code-page)
  * ``Legacy`` - Windows 11, if CP_ACP is UTF-8 reverts the process to the system locale code page (Windows-1252/437)
  * ``en-US`` - or other locale name, Windows 11 and later, sets appropriate locale code page
* ``printerDriverIsolation`` - run user-mode printer driver components in separated process
  * ``True`` or ``False``
* ``disableWindowFiltering`` - enables application to see immersive windows
  * ``True`` or ``False``
* ``highResolutionScrollingAware``
  * ``True`` or ``False``
* ``ultraHighResolutionScrollingAware``
  * ``True`` or ``False``
* ``supportedOS:#`` - where # is number (1 to N supported OS's, consecutive), if present rsrcgen generates compatibility block; value is GUID followed by optional comment
* ``maxversiontested`` - highest OS version number under which the application was tested; for example Windows 10 19H1 is "10.0.18362.0"
* ``dependentAssembly`` - numbered same as supportedOS, value contains name, version and publicKeyToken of the dependent assembly separated by spaces

## Building the program
Rebuild of the source should be straightforward, Visual Studio solution is included. Before building, place ``rsrcgen.exe`` into tools directory.

## Notes
This tool is actually nothing much more than a very simple string replacing tool with predefined content. Written as very simplistic, for single purpose, does not even free allocated memory.
