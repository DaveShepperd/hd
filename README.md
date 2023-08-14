# hd
Hex dump tool.

I wrote this little tool over 30 years ago. Used mainly on Linux nearly daily ever since. Before uploading to github, I added a --head, --tail (and --wide) options so it would be more useful on Windows when built with MinGW32. This because Windows (XP,7,10) doesn't have redirects nor head nor tail nor less (although it does have more that can be piped, at least on W10). I can't speak to Windows11 (yet). The Makefile is for Linux.

<pre>
Usage: hd [-bhwB?] [--head=n] [--tail=n] [--skip=n] file [... file]
where:
 --bytes  or -b   print in bytes (default)
 --shorts or -h   print in halfwords (16 bits)
 --longs  or -w   print in words (32 bits)
 --big    or -B   print assuming Big Endian
 --head=n         print only 'n' leading lines
 --tail=n         print only 'n' last lines
 --skip=n         first skip 'n' bytes on all inputs
 --wide   or -W   set number of columns to 32. (Default is 16.)
 --help   or -?   this message
</pre>


