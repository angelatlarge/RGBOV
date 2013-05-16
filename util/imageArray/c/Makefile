all:	genimg1

genimg1:	genimg.c lodepng.c
	gcc -std=c99 -o genimg.exe genimg.c lodepng.c
	
genimg2:	genimg2.c 
	cc -o genimg2 genimg2.c -L/usr/local/lib liblept.a libz.a
