#include <unistd.h>

int main()
{
    int i;

    for (i = 0; i < 60; i++) {
        sleep(1);
    }

    return 0;
}