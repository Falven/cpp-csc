# C.S.C. (Command Swann Cam)
###Program to send a scan command to your swann cam every delta_sec seconds.

Francisco Aguilera

##Building:
The project uses CMake to generate a Makefile. The provided Makefile should work fine on department machines, however, in case you would like to run the program elsewhere, or re-generate the Makefile:

out-of-source: Create a directory somewhere outside the source to hold the build files.
`mkdir ~/Desktop/csc-build`

Then simply run Cmake with the proper generator and source directory.
`cmake -G "Unix Makefiles" ~/Desktop/csc-build/`

Change directories to the build tree.
`cd ~/Desktop/csc-build/`

Make the project.
`make`

Run the project.
`./chat ...`

##Invocation:
`chmod a+x csc // if not built, make the file executable by all`
`./csc [options] [help | [host [port [username [password [delta_secs]]]]]]`

Examples:
.\csc mySubdomain.myDomain.com 80 myUsername myPassword 60
.\csc -t 60 -o 90 -u myUsername -h mySubdomain.myDomain.com -p myPassword