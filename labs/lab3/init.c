#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conway.h"
#include "init.h"

extern perfect_cell matrix[NUM_OF_ROWS][NUM_OF_COLUMNS];

void init_cells()
{
    FILE *fp = fopen("initial.txt", "r");
    if (!fp)
    {
        perror("fopen: initial.txt");
        exit(EXIT_FAILURE);
    }

    char line[NUM_OF_COLUMNS + 2]; /* +2 za '\n' i '\0' */

    for (int r = 0; r < NUM_OF_ROWS; r++)
    {
        if (!fgets(line, sizeof(line), fp))
        {
            fprintf(stderr,
                    "Greska: initial.txt ima manje od %d redova (nedostaje red %d).\n",
                    NUM_OF_ROWS, r);
            fclose(fp);
            exit(EXIT_FAILURE);
        }

        /* Ukloni newline ako postoji */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
        {
            line[--len] = '\0';
        }

        if ((int)len != NUM_OF_COLUMNS)
        {
            fprintf(stderr,
                    "Greska: red %d u initial.txt ima %zu znakova, ocekivano %d.\n",
                    r, len, NUM_OF_COLUMNS);
            fclose(fp);
            exit(EXIT_FAILURE);
        }

        for (int c = 0; c < NUM_OF_COLUMNS; c++)
        {
            char ch = line[c];
            if (ch != '0' && ch != '1')
            {
                fprintf(stderr,
                        "Greska: neocekivani karakter '%c' na poziciji (%d, %d).\n",
                        ch, r, c);
                fclose(fp);
                exit(EXIT_FAILURE);
            }

            matrix[r][c].row = r;
            matrix[r][c].column = c;
            matrix[r][c].is_alive = ch - '0';
            matrix[r][c].old_state = matrix[r][c].is_alive;
            matrix[r][c].is_finished = 0;
        }
    }

    fclose(fp);
}