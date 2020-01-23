
cc=cl.exe
ccflags=/c /O2 /GA
ccdefs=/DUNICODE
link=link.exe
linkflags=/SUBSYSTEM:WINDOWS
libs=ole32.lib user32.lib
rc=rc.exe

all: looper.exe

clean:
	del looper.res
	del looper.obj
	del looper.exe

looper.obj:
	$(cc) $(ccflags) $(ccdefs) $*.cpp

looper.res:
	$(rc) looper.rc

looper.exe: looper.obj looper.res
	$(link) $(linkflags) -out:looper.exe looper.obj looper.res $(libs)
		