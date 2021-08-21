#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "dbg.h"  // Add -DNDEBUG in CFLAGS to disable debug output

/**
 * TODO
 * - [_] implement proper project structure and update makefile
 **/
#define ORDER 2                // input size (2: 4x4, 3: 9x9, ...)
#define MAX_FILE_LINE_LEN 256  // max line buffer size for reading file
#define MAX_DIGIT_CHARS 1      // max number of digits per number

#define MAX_ITER 2000
#define MAX_GUESS_IND 15

#define SIZE (ORDER*ORDER)
#define TOTAL (SIZE*SIZE)
#define MAX_DIGIT (SIZE)
#define N_DIGITS (SIZE+1)

#ifdef NDEBUG
#define PRINT_SUDOKUS 0
#else
#define PRINT_SUDOKUS 1
#endif


struct History {
    int sudoku_stack[MAX_GUESS_IND*TOTAL];
    int guess_stack[MAX_GUESS_IND*N_DIGITS];
    int chkp_idx;
};

/**
 * Some global naming conventions
 *   sudoku : 1-dim integer array containing puzzle data
 *   s_idx  : index of sudoku puzzle
 *   num    : entry at index `s_idx` of `sudoku`
 *   rcb    : entries at row/col/blk of s_idx (len = `3*(SIZE-1)`)
 *   digit  : member of set {0, 1, 2, ..., order^2} (len = `N_DIGITS`)
 *   poss   : array of all possibilities (len = `TOTAL + TOTAL*N_DIGITS`)
 *   guess  : choice count, digit availability of inputs at `sudoku[s_idx]`
 */


int *sudoku_get_rcb(int* p_sudoku, int s_idx) {
    /* Returns elements (array of entries in s_idx and row/col/blk of s_idx) */
    check((s_idx < TOTAL), "s_idx (%d) exceeds TOTAL (%d)", s_idx, TOTAL);

    static int rcb[1 + 3*(SIZE -1)];
    int *p_rcb = rcb;

    check((s_idx < TOTAL), "s_idx (%d) exceeds TOTAL (%d)", s_idx, TOTAL);
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
error:
    for (int i = 0; i < 1 + 3*(SIZE-1); i++) {
        p_rcb[i] = -1;
    }
    exit(1);
    return p_rcb;
}


void print_sudoku(int* p_sudoku, int to_stdout) {
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
            if (to_stdout == 1) {
                fprintf(stdout, "%d%c", num, v_sep);
            } else {
                fprintf(stderr, "%d%c", num, v_sep);
            }
        }
        // print row block separator
        if ((row % ORDER == ORDER-1) && (row < SIZE - 1)) {
            if (to_stdout == 1) {
                fprintf(stdout, "%s", h_sep);
            } else {
                fprintf(stderr, "%s", h_sep);
            }
        }
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
    check((file != NULL), "Failed to load file %s", filename);

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
                check((numstr_i <= MAX_DIGIT_CHARS),
                      "Number with chars > MAX_DIGIT_CHARS (%d) found in %s",
                      MAX_DIGIT_CHARS, filename);
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
    fclose(file);
    check((s_idx == TOTAL),
          "Finished reading %s before completing sudoku input", filename);
    return p_sudoku;
error:
    exit(1);
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
                        if (i == 0) debug("Repeated %d in row %d", digit, i);
                        if (i == 1) debug("Repeated %d in col %d", digit, i);
                        if (i == 2) debug("Repeated %d in blk %d", digit, i);
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
    hist->chkp_idx++;

    int offset = hist->chkp_idx;
    for (int s_idx = 0; s_idx < TOTAL; s_idx++) {
        hist->sudoku_stack[offset + s_idx] = p_sudoku[s_idx];
    }
    hist->guess_stack[offset + TOTAL] = s_idx;  // save location of guess
    for (int digit = 1; digit < N_DIGITS; digit++) {
        hist->guess_stack[offset + TOTAL + digit] = p_guess[digit];
    }

    debug("Pushed checkpoint, (chkp_idx %d --> %d)",
          hist->chkp_idx - 1, hist->chkp_idx);
}
struct History *History_create(int* p_sudoku){
    struct History *hist = malloc(sizeof(struct History));
    check_mem((hist != NULL));

    // create dummy guess and index for initial chkp_idx (NEVER ACCESSED)
    int dummy_idx = -1;
    int dummy_guess[N_DIGITS];
    for (int i = 0; i < N_DIGITS; i++) {
        dummy_guess[i] = -1;
    }
    int *p_dummy_guess = dummy_guess;

    hist->chkp_idx = -1;
    History_push_checkpoint(hist, p_sudoku, p_dummy_guess, dummy_idx);

    return hist;
error:
    exit(1);
}
void History_destroy(struct History *hist) {
    if (hist != NULL) free(hist);
}
int* History_pull_checkpoint(struct History *hist, int pop_mode) {
    static int checkpoint[TOTAL + TOTAL*N_DIGITS];
    int *p_checkpoint = checkpoint;

    int offset = hist->chkp_idx;
    for (int s_idx = 0; s_idx < TOTAL; s_idx++) {
        p_checkpoint[s_idx] = hist->sudoku_stack[offset + s_idx];
    }
    p_checkpoint[TOTAL] = hist->guess_stack[offset + TOTAL];  // load guess loc
    for (int digit = 1; digit < N_DIGITS; digit++) {
        p_checkpoint[TOTAL + digit] = hist->guess_stack[offset + TOTAL + digit];
    }

    // pop current sudoku/poss from top of stack if requested
    if (pop_mode == 1) {
        check((hist->chkp_idx > 0),
              "Cannot pop checkpoint from stack when chkp_idx = %d",
              hist->chkp_idx);
        hist->chkp_idx--;
        debug("Popped checkpoint (chkp_idx %d --> %d)",
              hist->chkp_idx + 1, hist->chkp_idx);
    } else {
        debug("Pulled checkpoint, (chkp_idx %d --> %d)",
              hist->chkp_idx - 1, hist->chkp_idx);
    }

    return p_checkpoint;
error:
    for (int i=0; i < TOTAL + TOTAL*N_DIGITS; i++) {
        p_checkpoint[i] = -1;
    }
    exit(1);
    return p_checkpoint;
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
int solve_sudoku_revert(int *p_sudoku, int *p_poss, struct History *hist) {
    int step_made_revert = 0;

    int g_idx = -1;
    int fill = 0;
    int *p_checkpoint;
    int p_guess[1+MAX_DIGIT];

    int pop_mode = 1;
    int n_guess = 0;

    // go back to last checkpoint with another available branch
    while (n_guess == 0) {
        p_checkpoint = History_pull_checkpoint(hist, pop_mode);  // chkp_idx--

        // load guess at checkpoint
        for (int digit=1; digit < N_DIGITS; digit++) {
            n_guess += p_checkpoint[TOTAL + digit];
            p_guess[digit] = p_checkpoint[TOTAL + digit];
        }
        p_guess[0] = n_guess;
    }
    g_idx = p_checkpoint[TOTAL];
    // load sudoku
    for (int s_idx=0; s_idx < TOTAL; s_idx++) {
        p_sudoku[s_idx] = p_checkpoint[s_idx];
    }
    if (n_guess == 1) {
        // if there's only 1 other branch ,automatically apply
        fill = fill_idx(p_guess);
        debug("s_idx %d, %d --> %d",
              g_idx, p_sudoku[g_idx], fill);
        p_sudoku[g_idx] = fill;
        step_made_revert = 1;
        if (PRINT_SUDOKUS) print_sudoku(p_sudoku, 0);
    } else {
        // otherwise, add to other entries of p_poss
        for (int digit = 1; digit <= MAX_DIGIT; digit++) {
            p_poss[g_idx*(1+digit)] = p_guess[digit];
        }
    }
    debug("Reverted checkpoint (chap_idx %d)", hist->chkp_idx);
    return step_made_revert;
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
int solve_sudoku_guess(int *p_sudoku, int *p_poss, struct History *hist) {
    int guess_made = 0;

    int p_guess[1+MAX_DIGIT];
    int n_guess = 0;
    int fill = 0;

    for (int s_idx = 0; s_idx < TOTAL; s_idx++) {
        // load guess from poss
        if (p_sudoku[s_idx] == 0){
            n_guess = 0;
            for (int digit = 1; digit <= MAX_DIGIT; digit++){
                p_guess[digit] = p_poss[s_idx*(1+digit)];
                n_guess++;
            }

            debug("s_idx %d, n_guess %d: %d %d %d %d",
                    s_idx, n_guess, p_guess[1], p_guess[2], p_guess[3], p_guess[4]);
            if (n_guess > 1) {
                fill = fill_idx(p_guess);
                // remove from possibilities, mark branch as active
                p_guess[fill] = 0;
                // record remaining branches (come back to this state and
                // follow another branch if active branch is invalid)
                History_push_checkpoint(hist, p_sudoku, p_guess, s_idx);
                // make change and follow active branch
                debug("s_idx %d, %d --> %d",
                      s_idx, p_sudoku[s_idx], fill);
                p_sudoku[s_idx] = fill;
                guess_made = 1;
                debug("BREAK GUESS LOOP");
                break;
            }
        }
    }
    return guess_made;
}
int* sudoku_get_guess(int* p_sudoku, int s_idx) {
    static int guess[1+N_DIGITS];
    int* p_guess = guess;
    p_guess[0] = 0;  // count of possibilities at s_idx
    for (int digit = 1; digit <= MAX_DIGIT; digit++) {
        p_guess[digit] = 1;  // reset digit possibilities
    }
    // get digits in row/col/blk and eliminate possibilities
    int* p_rcb = sudoku_get_rcb(p_sudoku, s_idx);
    for (int i = 1; i < 1 + 3*(SIZE -1); i++) {
        if (p_rcb[i] > 0) {
            p_guess[p_rcb[i]] = 0;
        }
    }
    // get n_guess
    int n_guess = 0;
    for (int digit=1; digit <= MAX_DIGIT; digit++) {
        n_guess += p_guess[digit];
    }
    p_guess[0] = n_guess;
    return p_guess;
}
int solve_sudoku_step(int *p_sudoku, int* p_poss) {
    /* Fills s_idx if 1 possibility, stores guessed in p_poss if >1 */
    int step_made = 0;

    int *p_guess;

    int fill = 0;
    int n_guess = 0;
    for (int s_idx = 0; s_idx < TOTAL; s_idx++) {
        if (p_sudoku[s_idx] == 0) {
            // get guess at empty s_idx
            p_guess = sudoku_get_guess(p_sudoku, s_idx);
            n_guess = p_guess[0];
            debug("n_guess %d: %d %d %d %d",
                    n_guess, p_guess[1], p_guess[2], p_guess[3], p_guess[4]);
            // process guess
            if (n_guess == 0){
                // break if no answer possible
                debug("==== TRACE : n_guess is 0, break loop");
                break;
            } else if (n_guess == 1){
                // apply if 1 answer possible
                fill = fill_idx(p_guess);
                debug("s_idx %d, %d --> %d",
                      s_idx, p_sudoku[s_idx], fill);
                p_sudoku[s_idx] = fill;
                step_made = 1;
                if (PRINT_SUDOKUS) print_sudoku(p_sudoku, 0);
            } else {
                // stow guess if more than 1 answer possible
                p_poss[s_idx*(1+MAX_DIGIT)] = n_guess;
                for (int digit = 1; digit <= MAX_DIGIT; digit++){
                    p_poss[s_idx*(1+digit)] = p_guess[digit];
                }
            }
        }
    }
    return step_made;
}
int solve_sudoku(int* p_sudoku, struct History *hist) {
    /* Driver for solver loop, returns -1/0 if output is unsolved/solved */
    int solved_flag = -1;
    int status = 0;
    int step_made = 0;

    // create new possibility array TODO couple with p_sudoku in struct
    static int poss[TOTAL*(1+MAX_DIGIT)];
    int *p_poss = poss;
    for (int s_idx=0; s_idx < TOTAL; s_idx++) {
        poss[s_idx*(1+MAX_DIGIT)] = -1;  // counts of guesses
        for (int digit = 1; digit <= MAX_DIGIT; digit++) {
            poss[s_idx*(1+digit)] = 0;
        }
    }

    for (int iter = 1; iter <= MAX_ITER; iter++) {
        debug("STARTING ITER %d", iter);

        step_made = solve_sudoku_step(p_sudoku, p_poss);
        status = check_sudoku(p_sudoku);
        debug("step_made = %d", step_made);

        if (status == -1) {
            debug("INVALID: RESTORING PREVIOUS GUESS");
            step_made = solve_sudoku_revert(p_sudoku, p_poss, hist);
            status = check_sudoku(p_sudoku);  // update status after revert
        }

        if (status == 0) {
            if (step_made == 0) {
                debug("UNRESOLVED: MAKING GUESS");
                solve_sudoku_guess(p_sudoku, p_poss, hist);
            } else {
                debug("UNRESOLVED: CONTINUING SOLVE LOOP");
            }
        } else if (status == 1) {
            debug("COMPLETE :D\n");
            solved_flag = 0;
            break;
        }

        if (PRINT_SUDOKUS) {
            debug("Displaying end of iter %d result", iter);
            print_sudoku(p_sudoku, 0);  //DEBUG
        }
        check_debug((iter < MAX_ITER), "REACHED MAX_ITER");
    }
    return solved_flag;
}


char* arg_parse(int argc, char** argv) {
    check((argc >= 2), "Usage: \n\t $ ./sudoku [path to sudoku txt]")
    char* filename;
    filename = argv[1];
    return filename;
error:
    exit(1);
}
int main(int argc, char** argv) {

    char* filename = arg_parse(argc, argv);
    int status;

    debug("Reading file %s", filename);
    int* p_sudoku = read_sudoku(filename);

    if (PRINT_SUDOKUS) {
        debug("Displaying input ");
        print_sudoku(p_sudoku, 0);
    }

    debug("Checking input ");
    status = check_sudoku(p_sudoku);
    check((status > -1), "Invalid input from file %s", filename);

    /* if (status == -1) { */
    /*     debug("VALID :("); */
    /* } else if (status == 0) { */
    /*     debug("VALID :)"); */
    /* } else if (status == 1) { */
    /*     debug("COMPLETE :D"); */
    /* } */


    struct History *hist = History_create(p_sudoku);
    /* debug("CREATED HISTORY RECORD \n\n"); */
    debug("SOLVING ... ");
    status = solve_sudoku(p_sudoku, hist);
    check((status > -1), "Couldn't solve puzzle, try increasing MAX_ITER?");
    /* if (status == -1) { */
    /*     debug("COULDN'T SOLVE PUZZLE :("); */
    /* } */
    History_destroy(hist);

    if (PRINT_SUDOKUS) {
        debug("Displaying output ");
    }

    print_sudoku(p_sudoku, 1);

    return 0;
error:
    return -1;
}
