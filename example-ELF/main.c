#include "shared.h"
#include <stdio.h>

int main() {
	int a;
	scanf("%d", &a);
	printf("foo %d\n", foo(a));
	printf("bar %d\n", bar(a));
}
