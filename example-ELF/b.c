#include "shared.h"
#include "math.h"

int zoo(int x) {
	return x*2;
}

int bar(int x) {
	return sqrt(zoo(x));
}
