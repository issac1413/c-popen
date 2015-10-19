//Use example process briefly，like as popen
#include "./popen.h"

FILE *fp = popen_scm(cmd.c_str(),"r");

char buf[1024] = {'\0'};
while (fread(buf, sizeof(char), sizeof(buf)-1, fp) > 0){
  //
}

pclose_scm(fp);
