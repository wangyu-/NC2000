#include "stdlib.h"
#include "math.h"
#include "stdio.h"
double func(int t){
                int sign= t>=0? 1:-1;
        double abs_value=fabs(t);
        double thres=20000;
        if (abs_value>thres){
            //abs_value=thres+(abs_value-thres)*0.3;
            const double r=3.5;
            abs_value=pow(thres,1-1/r)*pow(abs_value,1/r);
        }
        return sign*abs_value;
}
int main()
{
       for(int i=0;i<32767*4;i+=1000){
               printf("%d %f\n",i,func(i));
       }
}
