#include <string.h>
#include <stdio.h>

int main(void){
FILE *fptr;

//antager at filen med wastepicker_id hedder wastepicker_id.txt
fptr = fopen("wastepicker_id.txt", "r");

if(fptr == NULL) {
  printf("Not able to open the file.");
}

char wastepicker_ID[20]; //20 chars lang, er nok for langt

/*
fgets læser 1. linje af filen og smider det ind i wastepicker_id
//Antager det vil se såda 
*/
fgets(wastepicker_ID, 20, fptr);

//for at se om det virker. For debug
printf("%s", wastepicker_ID);

// Lukker filen igen
fclose(fptr); 
}