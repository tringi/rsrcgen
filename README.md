# rsrcgen
*Windows Version Info and Manifest generator*

Generates VERSIONINFO-containing Win32 resource file and WinSxS assembly manifest for your EXE or DLL according to details specified in the .info file, optionally incrementing build number.

To use as part of the build process in Visual Studio, in project properties, navigate to "Configuration Properties / Build Events / Pre-Build Event" and add following to the "Command Line" field:

    "$(SolutionDir)rsrcgen.exe" "$(ProjectDir)$(AssemblyName).info" "dir=$(ProjectDir)\" debug=$(UseDebugLibraries) platform=$(PlatformTarget)

You will need to edit the path to the rsrcgen.exe file above.

## Command line
First command line parameter is a **full** path to the .info file (details below). All following parameters should be in ``abc=xyz`` format. These parameters define another variables available in the .info file.

## Info file
The .info file is nothing else than a basic Windows INI file. Open the rsrcgen.info file in you favorite text editor to understand what the following talks about.

#### [generator]
* ``autoincrement`` - the only option available now: when **true** rsrcgen will search in ``[description]`` for ``build``, increments it and writes it back into the .info file

#### [description]
This section contains predefined variables. Command line parameters take precedence. Variables can be referenced in any value in the .info file using ``%name%`` syntax. Evaluation is recursive so beware endless loops.

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
* ``private`` - private build, specify details int ``PrivateBuild`` in [versioninfo:values]
* ``special`` - special build, specify what is special in ``SpecialBuild`` in [versioninfo:values]

#### [manifest]
Specifies details of generated manifest file.
* ``filename`` - path to the resulting .manifest file; if empty or missing none is generated
* ``architecture`` - binary architecture: x86, amd64, ia64, arm, arm64, etc.; typically passed through command line parameter
* ``assemblyVersion``, ``assemblyIdentityName``, ``assemblyDescription`` - version and description
* ``requestedExecutionLevel`` - if present, rsrscgen generates trustInfo block, typically: asInvoker, highestAvailable or requireAdministrator
* ``dpiAware`` - if present, rsrscgen generates application/windowsSettings block, typically: true or true/pm
* ``supportedOS:#`` - where # is number (1 to N supported OS's, consecutive), if present rsrcgen generates compatibility block; value is GUID followed by optional comment
* ``dependentAssembly`` - numbered same as supportedOS, value contains name, version and publicKeyToken of the dependent assembly separated by spaces

## Building the program
Rebuild of the source should be straightforward, Dev-C++ project is included. To refresh manifest and VERSIONINFO for the rsrcgen.exe itself, run:
``rsrcgen.exe PATH\rsrcgen.info dir=PATH debug=false platform=x86``

## Notes
This tool is actually nothing much more than a very simple string replacing tool with predefined content. Written as very simplistic, for single purpose, does not even free allocated memory.
