This directory contains the c++ application source for a rudimentary LeapDDSBridge.

Steps:

Into your working directory, copy the files from this directory.

Determine your arch string (see $NDDSHOME/lib, and use one of the available directory names).  If you are using Visual Studio Express, use i86Win32VS2010 (for example) as your architecture string.  If you have the full Visual Studio, you can use the x64Win64VS* versions.

Call rtiddsgen to generate the Type files:

% rtiddsgen -language c++ -inputIdl LeapTypes.idl -ppDisable -replace -example <arch>

This will generate all the necessary Type, TypeSupport and strongly typed writers necessary in the bridge code.



The LeapDDSBridge.cxx file includes several #defines which can be used to reduce the noise level.  You can undef pointables, hands and/or gestures, and also prevent the repeating information to the console about counts and frame rates.  




For Windows:

Open the generated .sln and delete one of the projects, for example the Subscriber project.  
Change the name of the remaining project to "LeapDDSBridge" (or as desired).
Replace the LeapTypes_publisher.cxx file in the Source Files folder with the LeapDDSBridge.cxx file from this directory.
Right-click the LeapDDSBridge project and change the properties:
    Configuration:  All Configurations
    C/C++:
        General: Additional Include Directories:  Add the location of the Leap include directory
    Linker:
        General: Output File:  Change to .\objs\<arch>\LeapDDSBridge.exe
        General: Additional Library Directories:  Add the Leap Motion SDK lib\<archname> directory (like: lib\x86)
        Input:  Additional Dependencies:  Add leap.lib (or leapd.lib) to the list
        Debugging: Generate Program Database File:  change to .\objs\<arch>\LeapDDSBridge.pdb

And build.

For MacOS (Darwin):

Edit the included makefile_x64Darwin12clang4.1 from this directory to match your environment and build and install:

% make -f makefile_x86Darwin12clang4.1
...
% make -f makefile_x86Darwin12clang4.1 install
...
%


