#include "app/board_io.h"

int main(void)
{
	/* Initialize board functions before any sensor or bridge application. */
	return board_io_init();
}
