Visual Memory
=============

Display processes' memory as images. Can be used for finding where in memory a program stores its images, or just because memory patterns look cool!  

Building:
---------
Requires [SFML 2.0](http://sfml-dev.org/download.php). Windows only.  

Command line:
-------------
	visualmemory {<processname> | <processid>} ({RGB24 | RGBA32 | Mono8 | BGR24 | ABGR32}) (<minimum memory size>)  
processname: the name of the executable from which the process was started (e.g. explorer.exe). Will chose an arbitrary process if multiple match.  
processid: the process id of the process (e.g. 30220). Preferred, because it is guaranteed unique  
mode: The color format to interpret the memory as. Default is RGB24.  
minimum memory size: Skip over any memory blocks smaller than this size, in bytes. (e.g. 2097152). Default is 1MB.  

Controls:
---------
Arrow keys: Pan image  
Number keys 1-4: Set byte offset (e.g., if in RGB24 format, RGBRGBRGB... would be read as XRGBRGBRG... by increasing offset by one)  
[ and ] : Decrease, increase image width  
, and . : Decrease, increase pixel offset (essentially skips the first n pixels of the memory block)  
Enter: Next memory block  
F1: Display image/debug information (mainly for me)  
F12: Save image (saved as out.png in working directory)  
Escape: Exit  