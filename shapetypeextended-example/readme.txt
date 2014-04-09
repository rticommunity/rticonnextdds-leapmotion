This directory contains the modified ShapeTypeExtended examples as described in the forum post:

http://community.rti.com/howto/howto-use-rti-connext-dds-leap-motion-controller

Steps:

1) (all languages) Copy the ShapeType.idl file to your working directory.
2) (all languages) Copy the STE_Example[*.*] files from your language choice directory, to your working directory.
3) (all languages) Determine the arch you will be using (see $NDDSHOME/lib, the available directories are the arch strings you have access to).
4) Change to your working directory, and depending on Language choice, execute rtiddsgen with the following flags:

Java:

% rtiddsgen -language java -ppDisable -replace -inputIdl ShapeType.idl -example <arch>jdk

Import the directory into a Java project in Eclipse, add $NDDSHOME/class/nddsjava.jar to the build path, and run/debug either STE_ExamplePub or STE_ExampleSub as needed.



c++: (See Note 1)

% rtiddsgen -language c++ -ppDisable -replace -inputIdl ShapeType.idl -example <arch>

c: (See Note 1)

% rtiddsgen -language c -ppDisable -replace -inputIdl ShapeType.idl -example <arch>

c#: (See Note 1)

% rtiddsgen -language c# -ppDisable -replace -inputIdl ShapeType.idl -example <arch>

Note 1: If using Visual Studio Express, you will need to use a 32bit Architecture (i86Win32VS2010, for example).  The code generator will output a valid .sln solution file for the VS edition selected.  Open the .sln in VS, and proceed as normal.  Note that the .sln includes two projects, if you try to "build solution" the second will fail to build due to parallelism in the build.  Right-click on either project and build it individually, then build the other after.



modification history
--------------------
01a,2014-04-08,rip   first

