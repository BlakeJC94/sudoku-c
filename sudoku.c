#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>

/**
 * TODO
 * - [_] fix bug in reverting guesses when running
 *   :w | make sudoku | !./sudoku puzzles/2_dim/hard/input_0.txt
 *   - [X] re-implement history
 *   - [_] revise push/pull of checkpoints, prune possibilities
 *     - [X] add hist to arg_in for solve_step()
 * - [_] implement debug macros
 *   - [_] replace asserts
 *   - [_] make solution only non-debug output
 * - [_] implement proper project structure and update makefile
 **/
#define ORDER 2
#define MAX_FILE_LINE_LEN 256
#define MAX_DIGIT_CHARS 3
#define MAX_STEPS 20
#define MAX_GUESS_IND 15
#define DEFAULT_FILE "./demo.txt"

#define SIZE (ORDER*ORDER)
#define TOTAL (SIZE*SIZE)

#define MAX_DIGIT (SIZE)
#define N_DIGITS (SIZE+1)

struct History {
    int sudoku_stack[MAX_GUESS_IND*TOTAL];
    int guess_stack[MAX_GUESS_IND*N_DIGITS];
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


void History_push_checkpoint(struct History *hist, int *p_sudoku,
                             int *p_guess, int s_idx) {

    //DEBUG
    printf("    CALLED History_push_checkpoint, hist->g_idx = %d\n", hist->guess_idx);
    hist->guess_idx++;
    //DEBUG
    printf("      INCREASED hist->g_idx = %d\n", hist->guess_idx);

    int offset = hist->guess_idx;
    for (int s_idx = 0; s_idx < TOTAL; s_idx++) {
        hist->sudoku_stack[offset + s_idx] = p_sudoku[s_idx];
    }
    hist->guess_stack[offset + TOTAL] = s_idx;  // save location of guess
    for (int digit = 1; digit < N_DIGITS; digit++) {
        hist->guess_stack[offset + TOTAL + digit] = p_guess[digit];
    }
}
struct History *History_create(int* p_sudoku){
    struct History *hist = malloc(sizeof(struct History));
    assert(hist != NULL);

    // create dummy guess and index for initial guess_idx (NEVER ACCESSED)
    int dummy_idx = -1;
    int dummy_guess[N_DIGITS];
    for (int i = 0; i < N_DIGITS; i++) {
        dummy_guess[i] = -1;
    }
    int *p_dummy_guess = dummy_guess;

    hist->guess_idx = -1;
    History_push_checkpoint(hist, p_sudoku, p_dummy_guess, dummy_idx);

    return hist;
}
void History_destroy(struct History *hist) {
    assert(hist != NULL);
    free(hist);
}
/* int* History_get_sudoku_poss(struct History *hist, int pop_mode) { */
int* History_pull_checkpoint(struct History *hist, int pop_mode) {
    //DEBUG
    printf("    CALLED History_pull_checkpoint, hist->g_idx = %d, pop_mode = %d\n",
           hist->guess_idx, pop_mode);

    static int checkpoint[TOTAL + TOTAL*N_DIGITS];
    int *p_checkpoint = checkpoint;

    int offset = hist->guess_idx;
    for (int s_idx = 0; s_idx < TOTAL; s_idx++) {
        p_checkpoint[s_idx] = hist->sudoku_stack[offset + s_idx];
    }
    p_checkpoint[TOTAL] = hist->guess_stack[offset + TOTAL];  // load guess loc
    for (int digit = 1; digit < N_DIGITS; digit++) {
        p_checkpoint[TOTAL + digit] = hist->guess_stack[offset + TOTAL + digit];
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

    return p_checkpoint;
}
/** UNUSED
 * void History_print(struct History *hist) {
 *     printf("History current guess_idx : %d \n", hist->guess_idx);
 *
 *     int pop_mode = 0;
 *     int* p_checkpoint = History_pull_checkpoint(hist, pop_mode);
 *
 *     static int sudoku[TOTAL];
 *     int *p_sudoku = sudoku;
 *     for (int i=0; i < TOTAL; i++) {
 *         p_sudoku[i] = p_checkpoint[i];
 *     }
 *     printf("Current sudoku : \n");
 *     print_sudoku(p_sudoku);
 *
 *     static int poss[TOTAL*N_DIGITS];
 *     int *p_poss = poss;
 *     for (int i=0; i < TOTAL*N_DIGITS; i++) {
 *         p_poss[i] = p_checkpoint[TOTAL + i];
 *     }
 *     print_poss(p_poss);
 * }
**/



void solve_sudoku_revert(int *p_sudoku, int *p_guess, struct History *hist) {
    int pop_mode = 1;
    int *p_checkpoint;

    int n_guess = 0;
    // go back to last checkpoint with another available branch
    while (n_guess == 0) {
        p_checkpoint = History_pull_checkpoint(hist, pop_mode); // reduces g_idx

        for (int digit=1; digit < N_DIGITS; digit++) {
            n_guess += p_checkpoint[TOTAL + digit];
        }
    }
    //DEBUG
    printf("REVERTED CHECKPOINT (guess_idx is now %d)\n", hist->guess_idx);
    for (int s_idx=0; s_idx < TOTAL; s_idx++) {
        p_sudoku[s_idx] = p_checkpoint[s_idx];
    }
}
int* poss_get_guess(int s_idx, int *p_poss){
    /* Returns n_poss at s_idx [0] and possibilities at s_idx [1,..,N_DIGITS] */
    static int guess[1+N_DIGITS];
    int* p_guess = guess;

    p_guess[0] = 0;  // count of possibilities at s_idx
    for (int digit = 1; digit < N_DIGITS; digit++) {
        p_guess[0] += p_poss[s_idx*N_DIGITS + digit];
        p_guess[digit] = p_poss[s_idx*N_DIGITS + digit];
    }

    return p_guess;
}
int fill_idx(int *p_guess) {
    /* Return last possible digit in guess */
    int fill = 0;
    for (int digit = 1; digit < N_DIGITS; digit++) {
        if (p_guess[digit] == 1) {
            fill = digit;
        }
    }
    return fill;
}
int solve_sudoku_step(int *p_sudoku, int *p_poss, struct History *hist) {
    /* Loops over sudoku and matches possibilities, guesses if specified */
    int change_made = 0;
    int* p_guess;
    int n_guess = 0;
    int fill = 0;
    // loop through each s_idx
    for (int s_idx = 0; s_idx < TOTAL; s_idx++) {
        //DEBUG
        printf("   solve_step: s_idx = %d, p_sudoku[s_idx] = %d\n",
               s_idx, p_poss[s_idx*N_DIGITS]);
        if (p_sudoku[s_idx] == 0) {
            p_guess = poss_get_guess(s_idx, p_poss);
            n_guess = p_guess[0];
            //DEBUG
            printf("   solve_step: s_idx = %d, n_guess = %d\n",
                   s_idx, n_guess);

            if (n_guess == 1) {
                /* p_sudoku[s_idx] = fill_idx(p_guess, guess_mode); */
                /* change_made = 1; */
                fill = fill_idx(p_guess);
                printf("   solve_step: s_idx = %d, %d --> %d\n",
                       s_idx, p_sudoku[s_idx], fill);
                p_sudoku[s_idx] = fill;
                change_made = 1;
            }
        }
    }
    return change_made;
}
int solve_sudoku_guess(int *p_sudoku, int *p_poss, struct History *hist) {
    int guess_made = 0;
    int* p_guess;
    int n_guess = 0;
    int fill = 0;

    for (int s_idx = 0; s_idx < TOTAL; s_idx++) {
        if (p_poss[s_idx*N_DIGITS] > -1) {
            // s_idx marked with some possibilities available
            p_guess = poss_get_guess(s_idx, p_poss);
            n_guess = p_guess[0];
            if (n_guess > 1) {
                fill = fill_idx(p_guess);
                // remove from possibilities, mark branch as active
                p_guess[fill] = 0;
                // record remaining branches (come back to this state and
                // follow another branch if active branch is invalid)
                History_push_checkpoint(hist, p_sudoku, p_guess, s_idx);
                // make change and follow active branch
                p_sudoku[s_idx] = fill;
                guess_made = 1;
                printf("BREAK GUESS LOOP\n");
                break;
            }
        }
    }
    return guess_made;
}
int* solve_sudoku_get_poss(int *p_sudoku) {
    static int poss[TOTAL*N_DIGITS];
    int *p_poss = poss;
    for (int i=0; i<TOTAL*N_DIGITS; i++) {
        poss[i] = 0;
    }

    int *p_rcb;
    for (int s_idx = 0; s_idx < TOTAL; s_idx++) {
        poss[s_idx*N_DIGITS] = s_idx;
        if (p_sudoku[s_idx] == 0) {
            // mark s_idx in poss array
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
        /* if (p_poss[s_idx*N_DIGITS] > -1) { */
        /*     printf("        (idx %d) \n", s_idx); */
        /*     for (int digit = 1; digit < SIZE+1; digit++) { */
        /*         printf("          digit = %d, possibility = %d\n", */
        /*                digit, p_poss[s_idx*N_DIGITS + digit]); */
        /*     } */
        /* } */
    }
    return p_poss;
}
/* void print_poss() { */
/* } */
int solve_sudoku(int* p_sudoku, struct History *hist) {
    /* Driver for solver loop, returns -1/0 if output is unsolved/solved */
    int solved_flag = -1;
    int status = 0;
    int step_made = 0;
    int guess_made = 0;

    for (int step = 1; step <= MAX_STEPS; step++) {
        printf("STARTING STEP %d\n", step);

        int* p_poss = solve_sudoku_get_poss(p_sudoku);

        step_made = solve_sudoku_step(p_sudoku, p_poss, hist);
        printf("\n  TRACE ---- step_made = %d\n", step_made);

        if (step_made == 0){

            status = check_sudoku(p_sudoku);
            if (status == 1) {
                // puzzle is solved, set solved_flag = 0, break and return
                printf("COMPLETE :D\n");
                solved_flag = 0;
                break;
            } else if (status == 0) {
                // puzzle is unsolved, make a guess and push to hist
                printf("UNRESOLVED: MAKING GUESS\n");
                guess_made = solve_sudoku_guess(p_sudoku, p_poss, hist);
            } else if (status == -1) {
                // puzzle is borked, pop hist and restore
                printf("INVALID: RESTORING PREVIOUS GUESS\n");
                solve_sudoku_revert(p_sudoku, p_poss, hist);
            }
        }

        //DEBUG
        printf("\n  TRACE ---- STEP STATUS = %d\n", step_made + guess_made);
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


    struct History *hist = History_create(p_sudoku);
    printf("CREATED HISTORY RECORD \n\n");
    printf("SOLVING ... \n\n");
    status = solve_sudoku(p_sudoku, hist);
    if (status == -1) {
        printf("COULDN'T SOLVE PUZZLE :(\n");
    }
    History_destroy(hist);

    printf("\nDISPLAYING OUTPUT\n");
    print_sudoku(p_sudoku);

    return 0;
}
