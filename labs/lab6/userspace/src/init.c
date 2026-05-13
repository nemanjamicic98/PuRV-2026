#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "init.h"
#include "../../conway.h"

extern perfect_cell state[NUM_OF_ROWS][NUM_OF_COLUMNS];

int init_cells(char *file_name)
{
    FILE *fp = fopen(file_name, "r");
    if (!fp)
    {
        perror("fopen");
        return 1; 
    }

    char line[NUM_OF_COLUMNS + 5]; 

    for (int r = 0; r < NUM_OF_ROWS; r++)
    {
        if (!fgets(line, sizeof(line), fp))
        {
            fprintf(stderr,
                    "Greska: %s ima manje od %d redova (nedostaje red %d).\n",
                    file_name, NUM_OF_ROWS, r);
            fclose(fp);
            return 1;
        }

        line[strcspn(line, "\r\n")] = '\0';
        
        size_t len = strlen(line);

        if ((int)len != NUM_OF_COLUMNS)
        {
            fprintf(stderr,
                    "Greska: red %d u %s ima %zu znakova, ocekivano %d.\n",
                    r, file_name, len, NUM_OF_COLUMNS);
            fclose(fp);
            return 1;
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
                return 1;
            }

            state[r][c].row = r;
            state[r][c].col = c;
            state[r][c].new_state = ch - '0';
            state[r][c].old_state = state[r][c].new_state;
            state[r][c].is_finished = 0;
        }
    }

    fclose(fp);

    return 0;
}