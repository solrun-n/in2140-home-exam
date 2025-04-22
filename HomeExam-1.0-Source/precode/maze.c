#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#include "maze.h"

// Hjelpefunksjon for å få tilgang til en celle i labyrinten
static inline int index(const Maze* m, int x, int y) {
    return y * m->edgeLen + x;
}

// Hjelpefunksjon for å merke løsningen
void markSolution(Maze* maze, int x, int y) {
    maze->maze[index(maze, x, y)] |= mark;
}

// Rekursiv DFS med tilbakeføring for å finne den korteste stien
static int dfs(Maze* maze, int x, int y, int endX, int endY, int pathLength, int* minPathLength) {
    // Hvis vi har kommet til slutten, merk og returner
    if (x == endX && y == endY) {
        markSolution(maze, x, y);
        return 1;
    }

    char* cell = &maze->maze[index(maze, x, y)];

    // Merk denne cellen som besøkt midlertidig
    if (*cell & tmark) return 0; // Vi har vært her før
    *cell |= tmark;

    // Prøv alle retninger (først ned, deretter andre retninger)
    int dx[] = {0, 0, -1, 1};  // ned, opp, venstre, høyre
    int dy[] = {1, -1, 0, 0};
    int dir[] = {down, up, left, right};
    int opp[] = {up, down, right, left};

    int foundSolution = 0;
    for (int i = 0; i < 4; ++i) {
        int nx = x + dx[i];
        int ny = y + dy[i];

        // Sjekk om vi er innenfor grensene
        if (nx < 0 || ny < 0 || nx >= (int)maze->edgeLen || ny >= (int)maze->edgeLen)
            continue;

        char thisCell = *cell;
        char nextCell = maze->maze[index(maze, nx, ny)];

        // Sjekk om det er en vei i denne retningen
        if ((thisCell & dir[i]) && (nextCell & opp[i])) {
            // DFS til neste celle
            if (dfs(maze, nx, ny, endX, endY, pathLength + 1, minPathLength)) {
                markSolution(maze, x, y); // Denne cellen er på løsningen
                foundSolution = 1;
                break;
            }
        }
    }

    // Tilbakefør dersom vi ikke har funnet løsning i denne veien
    if (!foundSolution) {
        *cell &= ~tmark;
    }

    return foundSolution;
}

// Maze solve funksjon
void mazeSolve(Maze* maze) {
    // Debug: print start og slutt-koordinater
    printf("DEBUG: Løser labyrint fra (%u, %u) til (%u, %u)\n",
           maze->startX, maze->startY, maze->endX, maze->endY);

    int minPathLength = maze->size;
    int success = dfs(maze, maze->startX, maze->startY, maze->endX, maze->endY, 0, &minPathLength);

    if (!success) {
        fprintf(stderr, "WARNING: Fant ingen løsning på labyrinten!\n");
    }

    // Debug: print hvor mange bytes labyrinten består av
    printf("DEBUG: maze->size = %u bytes\n", maze->size);
}
