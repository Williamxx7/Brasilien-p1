
// det her er punkt 3: Function der connecter når brugeren trykker “Start connection” (for at give bruger fake kontrol)
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

bool userConnectionStarted = false;

void handleConnectionCommand(char *command)
{
    // sammenlign streng i C
    if (strcmp(command, "START_CONNECTION") == 0)
    {
        lcd_clear();
        lcd_setCursor(0, 0);
        lcd_print("Connecting...");
        delay(1000);

        lcd_clear();
        lcd_setCursor(0, 0);
        lcd_print("Connected!");
        printf("connection successful\n");

        userConnectionStarted = true;

        SerialBT_println("CONNECTION_OK");
    }
}
