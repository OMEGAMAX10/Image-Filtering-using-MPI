# Image-Filtering-using-MPI
The goal of this project is to build a scalable MPI program that can apply multiple filters (smooth, blur, sharpen, mean, emboss) on images with .pnm or .pgm format.

### Installing dependencies on Ubuntu:
&emsp;Run these commands:
```console
foo@bar:~$ sudo apt update && sudo apt upgrade -y
foo@bar:~$ sudo apt install -y gcc make gdb libopenmpi-dev openmpi-bin openmpi-doc openmpi-common
```

### Guide for using the MPI Image Filtering:

&emsp;Run these commands:
```console
foo@bar:~$ mpicc -o homework homework.c -std=c99 -O3 -lm -Wall
foo@bar:~$ time mpirun --oversubscribe -np NUM_PROCS ./homework IMG_IN IMG_OUT FILT1 FILT2 ... FILTK
```
&emsp;where:\
&emsp;&emsp;NUM_PROCS = the number of processes\
&emsp;&emsp;IMG_IN = the name of the input image\
&emsp;&emsp;IMG_OUT = the name of the output image\
&emsp;&emsp;FILT1 FILT2 ... FILTK = the name of the filters (smooth, blur, sharpen, mean, emboss).
  
&emsp;Example:
```console
foo@bar:~$ time mpirun --oversubscribe -np 2 ./homework tree.pnm tree_smooth_sharpen.pnm smooth sharpen
```
