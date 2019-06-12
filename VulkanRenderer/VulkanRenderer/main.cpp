#include <crtdbg.h>
#include "Application.h"

int main() 
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	Application* game = new Application();
	game->Init();

	game->Run();

	delete game;

	return 0;
}
