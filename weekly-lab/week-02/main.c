#include <stdio.h>

int plus(int, int);
int minus(int, int);

int main(){
    printf("Input two numbers: ");
    int a, b,r,s;
    scanf("%d %d", &a, &b);
    r = plus(a, b);
    s = minus(a, b);
    printf("Result: %d %d\n",r,s);
    return 0;
}
