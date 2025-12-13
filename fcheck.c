#include <stdio.h>
#include <stdlib.h>

int test1(){
 return 0; //return 0 if test passes otherwise print error message and exit
}

int test2(){
 return 0; //return 0 if test passes 
}

int test3(){
 return 0; //return 0 if test passes
}

int test4(){
 return 0; //return 0 if test passes
}

int 
main(int argc, char *argv[]){

 if( argc < 2 ){
   fprintf(stderr, "Usage: fcheck <file_system_image>");
   exit(1); //exit 1 if no img file is given
 }
 
 test1();
 test2();
 test3();
 test4();

 exit(0); //exit 0 if all tests pass
}

