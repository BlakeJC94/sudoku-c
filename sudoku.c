#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>

/**
 * TODO
 * - fix bug in reverting guesses when running
 *   :w | make sudoku | !./sudoku puzzles/2_dim/hard/input_0.txt
 *   - re-implement history and prune possibilities
 * - implement debug macros
 *   - replace asserts
 *   - make solution only non-debug output
 * - implement proper project structure and update makefile
 **/
#define ORDER 2
#define MAX_FILE_LINE_LEN 256
#define MAX_DIGIT_CHARS 3
#define MAX_STEPS 50
#define MAX_GUESS_IND 15
#define DEFAULT_FILE "./demo.txt"

#define SIZE (ORDER*ORDER)
#define TOTAL (SIZE*SIZE)

#define MAX_DIGIT (SIZE)
#define N_DIGITS (SIZE+1)

struct History {
    int data[MAX_GUESS_IND*TOTAL*(TOTAL*N_DIGITS)];
    int S0;
    int S1;
    int S2;
    int guess_idx;
};

/**
 * Some global naming conventions
 *   sudoku   : 1-dim integer array containing puzzle data
 *   p_sudoku : pointer to sudoku array
 *   s_idx    : index of sudoku puzzle
 *   num      : entry at index `s_idx` of `sudoku`
 *   digit    : member of set {0, 1, 2, ..., order^2} (len = `N_DIGITS`)
 */


int *sudoku_get_rcb(int* p_sudoku, int s_idx) {
    /* Returns elements (array of entries in s_idx and row/col/blk of s_idx) */
    if (s_idx >= TOTAL) {
        printf("ERROR: s_idx exceeds TOTAL : %d\n", s_idx);
        exit(1);
    }

    static int rcb[1 + 3*(SIZE -1)];
    int *p_rcb = rcb;
    p_rcb[0] = p_sudoku[s_idx];

    int row_start = s_idx / SIZE;
    int col_start = s_idx % SIZE;
    int blk_start = ORDER*(SIZE*(s_idx/(SIZE*ORDER)) + (s_idx % SIZE)/ORDER);

    int row_idx, col_idx, blk_idx;

    int row_i = 1;
    int col_i = SIZE;
    int blk_i = 2*SIZE;

    for (int i = 0; i < SIZE; i++) {
        row_idx = row_start * SIZE + i;
        if (row_idx != s_idx) {
            p_rcb[row_i] = p_sudoku[row_idx]; row_i++;
        }

        col_idx = col_start + i*SIZE;
        if (col_idx != s_idx) {
            p_rcb[col_i] = p_sudoku[col_idx]; col_i++;
        }

        blk_idx = blk_start + (i % ORDER) + (i / ORDER) * SIZE;
        if (blk_idx != s_idx) {
            p_rcb[blk_i] = p_sudoku[blk_idx]; blk_i++;
        }
    }
    return p_rcb;
}


void print_sudoku(int* p_sudoku) {
    /* Prints sudoku entries and grid to stdout */
    int s_idx, num;
    char v_sep;
    // determine row separator
    char h_sep[SIZE*2];
    for (int col = 0; col < SIZE; col++) {
        h_sep[col*2] = '-';
        if (col == SIZE - 1) {
            h_sep[col*2 + 1] = '\n';
        } else {
            if (col % ORDER == ORDER-1){
                h_sep[col*2 + 1] = '*';
            } else {
                h_sep[col*2 + 1] = '-';
            }
        }
    }
    // print each row of numbers and separators
    for (int row = 0; row < SIZE; row++) {
        for (int col = 0; col < SIZE; col++) {
            s_idx = row*SIZE + col;
            num = p_sudoku[s_idx];
            // determine column block separator
            if (col == SIZE - 1) {
                v_sep = '\n';
            } else {
                if (col % ORDER == ORDER-1){
                    v_sep = '|';
                } else {
                    v_sep = ',';
                }
            }
            // print element and col separator
            printf("%d%c", num, v_sep);
        }
        // print row block separator
        if ((row % ORDER == ORDER-1) && (row < SIZE - 1)) {
            printf("%s", h_sep);
        }
    }
}
void print_poss(int *p_poss) {
    int s_idx;
    for (int i = 0; (i<TOTAL) && (p_poss[i*N_DIGITS] != -1); i++) {
        s_idx = p_poss[i*N_DIGITS];
        printf("  Estimates for s_idx %d (row %d, col %d) : ",
               s_idx, s_idx/SIZE, s_idx%SIZE);
        for (int digit=1; digit < N_DIGITS; digit++) {
            if (p_poss[i*N_DIGITS + digit] == 1) {
                printf("%d ", digit);
            }
        }
        printf("\n");
    }
}


int* read_sudoku(char* filename) {
    /* Returns sudoku array from filename input */
    static int sudoku[TOTAL];
    int *p_sudoku = sudoku;
    for (int i = 0; i < TOTAL; i++) {
        p_sudoku[i] = 0;
    }

    FILE* file = fopen(filename, "r");
    assert(file != NULL);

    char numstr[MAX_DIGIT_CHARS + 1];
    int numstr_i = 0;

    char line[MAX_FILE_LINE_LEN];
    int line_num = 0;
    int s_idx = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        line_num++;
        // scan through characters in line
        for (int i = 0; i < strlen(line); i++) {
            if (isdigit(line[i])) {
                // add adjacent numerical chars to num_str
                numstr[numstr_i] = line[i];
                numstr_i++;
                // keep hard limit on number of chars per number
                assert(numstr_i < MAX_DIGIT_CHARS);
            } else {
                if (numstr_i > 0) {
                    // non-num char encountered after >1 num chars
                    numstr[numstr_i] = '\0';
                    p_sudoku[s_idx] = atoi(numstr);
                    s_idx++;
                    numstr_i = 0;  // reset number reader
                }
            }
        }
    }
    assert(s_idx == TOTAL);
    return p_sudoku;
}

int* count_digits(int *p_sudoku, int s_idx) {
    static int digit_count[3*N_DIGITS];
    int *p_digit_count = digit_count;
    int *p_rcb;

    // reset digit counter
    for (int digit = 0; digit < 3*N_DIGITS; digit++) {
        p_digit_count[digit] = 0;
    }

    // load entries at row/col/blk of s_idx
    p_rcb = sudoku_get_rcb(p_sudoku, s_idx);
    for (int i = 0; i < 3; i++) {
        // count digit in selected index
        p_digit_count[ i*N_DIGITS + p_rcb[0] ]++;
        // count other digits in row/col/blk
        for (int j = 0; j < SIZE-1; j++) {
            p_digit_count[ i*N_DIGITS + p_rcb[1+i*(SIZE-1)+j] ]++;
        }
        // erase digit 0 counts
        p_digit_count[i*N_DIGITS] = 0;
    }

    return p_digit_count;
}
int check_sudoku(int* p_sudoku) {
    /* Returns 0/1 if valid and incomplete/complete, -1 if invalid */
    int sudoku_status = 1;
    int all_zero = 1;
    int* p_digit_count;

    for (int s_idx = 0; s_idx < TOTAL; s_idx++) {
        // check if sudoku is incomplete
        if ((sudoku_status == 1) && (p_sudoku[s_idx] == 0)) {
            sudoku_status = 0;
        }
        // check if sudoku is not all empty
        if ((all_zero == 1) && (p_sudoku[s_idx] > 0)) {
            all_zero = 0;
        }

        // check if entry at s_idx is invalid
        if (sudoku_status > -1) {
            // count digits in row/col/blk of s_idx
            p_digit_count = count_digits(p_sudoku, s_idx);
            // validate counts of non-zero digits at s_idx
            for (int i = 0; i < 3; i++) {
                for (int digit = 1; digit <= MAX_DIGIT; digit++) {
                    if (p_digit_count[ i*N_DIGITS + digit ] > 1) {
                        sudoku_status = -1;
                        //DEBUG
                        /* if (i == 0) printf("Repeated %d in row %d\n", digit, i); */
                        /* if (i == 1) printf("Repeated %d in col %d\n", digit, i); */
                        /* if (i == 2) printf("Repeated %d in blk %d\n", digit, i); */
                    }
                }
            }
        } else {
            break;
        }
    }
    if (all_zero == 1) sudoku_status = -1;

    return sudoku_status;
}


void History_set_sudoku_poss(struct History *hist, int *p_sudoku,
                             int *p_poss) {
    //DEBUG
    printf("    CALLED Hist_set_sudoku_poss, hist->g_idx = %d\n", hist->guess_idx);
    hist->guess_idx++;
    //DEBUG
    printf("      INCREASED hist->g_idx = %d\n", hist->guess_idx);

    int offset_sudoku = hist->guess_idx * (hist->S1 + hist->S2);
    for (int N1 = 0; N1 < hist->S1; N1++) {
        hist->data[offset_sudoku + N1] = p_sudoku[N1];
    }

    int offset_poss = hist->guess_idx * (hist->S1 + hist->S2) + hist->S1;
    for (int N2 = 0; N2 < hist->S2; N2++) {
        hist->data[offset_poss + N2] = p_poss[N2];
    }
}
struct History *History_create(int* p_sudoku){
    struct History *hist = malloc(sizeof(struct History));
    assert(hist != NULL);

    hist->S0 = MAX_GUESS_IND;
    hist->S1 = TOTAL;
    hist->S2 = TOTAL*N_DIGITS;
    /* hist->data = malloc(sizeof(int) * hist->S0 * hist->S1 * hist->S2); */


    int poss[TOTAL*N_DIGITS];
    for (int i = 0; i < hist->S2; i++) {
        poss[i] = -1;
    }
    int *p_poss = poss;

    hist->guess_idx = -1;
    History_set_sudoku_poss(hist, p_sudoku, p_poss);

    return hist;
}
void History_destroy(struct History *hist) {
    assert(hist != NULL);
    free(hist);
}
int* History_get_sudoku_poss(struct History *hist, int pop_mode) {
    //DEBUG
    printf("    CALLED History_get_sudoku_poss, hist->g_idx = %d, pop_mode = %d\n",
           hist->guess_idx, pop_mode);

    static int sudoku_poss[TOTAL + TOTAL*N_DIGITS];
    int *p_sudoku_poss = sudoku_poss;

    int offset_sudoku = hist->guess_idx*(hist->S1 + hist->S2);
    for (int N1 = 0; N1 < hist->S1; N1++) {
        p_sudoku_poss[N1] = hist->data[offset_sudoku + N1];
    }

    int offset_poss = hist->guess_idx*(hist->S1 + hist->S2) + hist->S1;
    for (int N2 = 0; N2 < hist->S2; N2++) {
        p_sudoku_poss[TOTAL + N2] = hist->data[offset_poss + N2];
    }

    if (pop_mode == 1) {
        // pop current sudoku/poss from top of stack
        if (hist->guess_idx == 0) {
            printf("ERROR : cannot pop from hist stack when guess_idx is 0\n");
            exit(1);
        } else {
            hist->guess_idx--;
            printf("      DECREASED hist->g_idx = %d\n", hist->guess_idx);
        }
    }

    return p_sudoku_poss;
}
void History_print(struct History *hist) {
    printf("History current guess_idx : %d \n", hist->guess_idx);

    int pop_mode = 0;
    int* p_sudoku_poss = History_get_sudoku_poss(hist, pop_mode);

    static int sudoku[TOTAL];
    int *p_sudoku = sudoku;
    for (int i=0; i < TOTAL; i++) {
        p_sudoku[i] = p_sudoku_poss[i];
    }
    printf("Current sudoku : \n");
    print_sudoku(p_sudoku);

    static int poss[TOTAL*N_DIGITS];
    int *p_poss = poss;
    for (int i=0; i < TOTAL*N_DIGITS; i++) {
        p_poss[i] = p_sudoku_poss[TOTAL + i];
    }
    print_poss(p_poss);
}



void solve_sudoku_revert(int *p_sudoku, int *p_poss, struct History *hist) {
    int pop_mode = 1;
    int* p_sudoku_poss = History_get_sudoku_poss(hist, pop_mode);
    for (int i=0; i < TOTAL; i++) {
        p_sudoku[i] = p_sudoku_poss[i];
    }
    for (int i=0; i < TOTAL*N_DIGITS; i++) {
        p_poss[i] = p_sudoku_poss[TOTAL + i];
    }
    printf("REVERTED TO LAST CHECKPOINT (guess_idx is now %d)\n", hist->guess_idx);
}
int solve_sudoku_step(int *p_sudoku, int *p_poss, int guess_mode) {
    int change_made = 0;
    int n_poss = 0;
    int fill = 0;
    // loop through each s_idx
    for (int s_idx = 0; s_idx < TOTAL; s_idx++) {
        if (p_poss[s_idx*N_DIGITS] > -1) {
            n_poss = 0;
            fill = 0;
            for (int digit = 1; digit < SIZE+1; digit++) {
                n_poss += p_poss[s_idx*N_DIGITS + digit];
                if (p_poss[s_idx*N_DIGITS + digit] == 1) fill = digit;
            }

            // if an s_idx has 1 possibility, make change, set change_made = 1
            if ((guess_mode == 0) && n_poss == 1) {
                // DEBUG
                /* printf("    guess_mode = %d, changing %d from %d to %d\n", */
                /*        guess_mode, s_idx, p_sudoku[s_idx], fill); */
                /* for (int digit = 1; digit < SIZE+1; digit++) { */
                /*     printf("        digit = %d, possibility = %d\n", */
                /*            digit, p_poss[s_idx*N_DIGITS + digit]); */
                /* } */

                p_sudoku[s_idx] = fill;
                change_made = 1;
            }
            if ((guess_mode == 1) && n_poss > 1) {
                // fill in last possibility of first available index, break
                p_poss[s_idx*N_DIGITS + fill] = 0;
                p_sudoku[s_idx] = fill;
                change_made = 2;
                break;
            }
        }
        // if all s_idx have 0 or >1 possibility, leave change_made = 0
    }
    return change_made;
}
int* solve_sudoku_get_poss(int *p_sudoku) {
    static int poss[TOTAL*N_DIGITS];
    int *p_poss = poss;
    for (int i=0; i<TOTAL*N_DIGITS; i++) {
        poss[i] = -1;
    }

    int *p_rcb;
    for (int s_idx = 0; s_idx < TOTAL; s_idx++) {
        if (p_sudoku[s_idx] == 0) {
            // mark s_idx in poss array
            poss[s_idx*N_DIGITS] = s_idx;
            for (int digit = 1; digit <= MAX_DIGIT; digit++) {
                //DEBUG
                /* printf("        digit = %d\n", digit); */
                poss[s_idx*N_DIGITS + digit] = 1;  // reset digit possibilities
            }
            // get digits in row/col/blk and eliminate possibilities
            p_rcb = sudoku_get_rcb(p_sudoku, s_idx);
            for (int i = 1; i < 1 + 3*(SIZE -1); i++) {
                if (p_rcb[i] > 0) poss[s_idx*N_DIGITS + p_rcb[i]] = 0;
            }
        }

        // DEBUG
        /* for (int digit = 1; digit < SIZE+1; digit++) { */
        /*     printf("        digit = %d, possibility = %d\n", */
        /*            digit, p_poss[s_idx*N_DIGITS + digit]); */
        /* } */
    }
    return p_poss;
}
int solve_sudoku(int* p_sudoku, struct History *hist) {
    int solved_flag = -1;
    int status = 0;
    int guess_mode = 0;

    for (int step = 0; step < MAX_STEPS; step++) {
        printf("STARTING STEP %d\n", step);

        int* p_poss = solve_sudoku_get_poss(p_sudoku);

        guess_mode = 0;
        int change_made = solve_sudoku_step(p_sudoku, p_poss, guess_mode);

        if (change_made == 0){

            status = check_sudoku(p_sudoku);
            if (status == 1) {
                // puzzle is solved, set solved_flag = 0, break and return
                printf("COMPLETE :D\n");
                solved_flag = 0;
                break;
            } else if (status == 0) {
                // puzzle is unsolved, make a guess and push to hist
                printf("UNRESOLVED: MAKING GUESS\n");
                guess_mode = 1;
                change_made = solve_sudoku_step(p_sudoku, p_poss, guess_mode);
                if (change_made == 2) {
                    History_set_sudoku_poss(hist, p_sudoku, p_poss);
                } else {
                    printf("NEVER REACHED\n");
                    exit(1);
                }
            } else if (status == -1) {
                // puzzle is borked, pop hist and restore
                printf("INVALID: RESTORING PREVIOUS GUESS\n");
                solve_sudoku_revert(p_sudoku, p_poss, hist);
            }
        }

        //DEBUG
        printf("\n  change_made = %d\n", change_made);
        print_sudoku(p_sudoku);

        if (step == MAX_STEPS) printf("REACHED MAX_STEPS :/\n");
    }

    return solved_flag;
}


char* arg_parse(int argc, char** argv) {
    char* filename;
    if (argc >= 2){
        filename = argv[1];
    } else {
        printf("NO INPUT GIVEN, RUNNING DEMO MODE\n");
        filename = DEFAULT_FILE;
    }
    return filename;
}
int main(int argc, char** argv) {
    char* filename = arg_parse(argc, argv);
    int status;

    printf("READING FILE : %s\n", filename);
    int* p_sudoku = read_sudoku(filename);

    printf("DISPLAYING INPUT\n");
    print_sudoku(p_sudoku);

    printf("CHECKING INPUT: ");
    status = check_sudoku(p_sudoku);
    if (status == -1) {
        printf("INVALID :(\n");
    } else if (status == 0) {
        printf("VALID :)\n");
    } else if (status == 1) {
        printf("COMPLETE :D\n");
    }


    printf("SOLVING ... \n\n");
    struct History *hist = History_create(p_sudoku);
    status = solve_sudoku(p_sudoku, hist);
    if (status == -1) {
        printf("COULDN'T SOLVE PUZZLE :(\n");
    }
    History_destroy(hist);

    printf("\nDISPLAYING OUTPUT\n");
    print_sudoku(p_sudoku);

    return 0;
}
