#include "dim.hpp"

using namespace std;

int main(int argc, char *argv[]) {
	DIM dim;
	dim.init(); // Call at the beginning

	dim.set_beta(32); // Set \beta=32 (See paper)

//	dim.insert(0); // Insert Node 0
//	dim.insert(1); // Insert Node 1
//	dim.insert(2); // Insert Node 2
//	dim.insert(3); // Insert Node 3
//
//	dim.insert(0, 1, 0.4); // Insert Edge (0,1) with prob 0.5
//	dim.insert(1, 2, 0.5); // Insert Edge (1,2) with prob 0.6
//	dim.insert(2, 0, 0.6); // Insert Edge (2,0) with prob 0.7
//	dim.insert(2, 3, 0.7); // Insert Edge (2,3) with prob 0.8
//
//	for (int v = 0; v < 4; v++)
//		printf("Influence of %d is %1.6f\n", v, dim.infest(v));
//	printf("Most influential vertex is %d\n", dim.infmax(1)[0]);
//
//	dim.insert(3, 1, 0.8); // Insert Edge (3,1) with prob 0.8
//	dim.erase(2, 3); // Delete Edge (2,3)
//
//	for (int v = 0; v < 4; v++)
//		printf("Influence of %d is %1.6f\n", v, dim.infest(v));
//	printf("Most influential vertex is %d\n", dim.infmax(1)[0]);
//
//	dim.erase(2); // Insert Node 2
//	dim.change(0, 1, 0.9); // Change prob of Edge (0,1) to 0.9
//
//	for (int v = 0; v < 4; v++)
//		printf("Influence of %d is %1.6f\n", v, dim.infest(v));
//	printf("Most influential vertex is %d\n", dim.infmax(1)[0]);


	dim.insert(0);
	for (int v = 0; v < 1; v++) printf("Influence of %d is %1.6f\n", v, dim.infest(v));
	printf("\n -- \n");

	dim.insert(1);
	for (int v = 0; v < 1; v++) printf("Influence of %d is %1.6f\n", v, dim.infest(v));
	printf("\n -- \n");

	dim.insert(2);
	dim.insert(1, 2, 0.1);
	for (int v = 0; v < 1; v++) printf("Influence of %d is %1.6f\n", v, dim.infest(v));
	printf("\n -- \n");

	dim.insert(3);
	dim.insert(3, 0, 0.3);
	for (int v = 0; v < 1; v++) printf("Influence of %d is %1.6f\n", v, dim.infest(v));
	printf("\n -- \n");


	dim.insert(3);
	dim.insert(3, 0, 0.3);
	for (int v = 0; v < 1; v++) printf("Influence of %d is %1.6f\n", v, dim.infest(v));
	printf("\n -- \n");

	return 0;
}
