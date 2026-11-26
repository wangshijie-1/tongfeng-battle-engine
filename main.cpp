#include <thread>
#include "GameEngine.h"

int main()
{
    GameEngine game;

    std::thread t(&GameEngine::run, &game);

    t.join();

    return 0;
}