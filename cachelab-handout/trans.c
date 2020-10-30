/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);
void print(int M, int N, int K[M][N]) {
    int i, j;

    for (i = 0; i < M; i++) {
        for (j = 0; j < N; ++j) {
            printf("%d\t", K[i][j]);
        }
        printf("\n");
    }
    printf("=============\n");
}

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, k;
    int t1, t2, t3, t4, t5, t6, t7, t8;
    // print(N, M, A);

    for (j = 0; j < M; j+=8) {
        for (i = 0; i < N; i+=4) {
            for (k = 0; (k < 4) && (i+k < N); k++) {
                if (i+k < N) {
                    t1 = A[i+k][j];
                    if (j+1 < M) {
                        t2 = A[i+k][j+1];
                    }
                    if (j+2 < M) {
                        t3 = A[i+k][j+2];
                    }
                    if (j+3 < M) {
                        t4 = A[i+k][j+3];
                    }
                    if (j+4 < M) {
                        t5 = A[i+k][j+4];
                    }
                    if (j+5 < M) {
                        t6 = A[i+k][j+5];
                    }
                    if (j+6 < M) {
                        t7 = A[i+k][j+6];
                    }
                    if (j+7 < M) {
                        t8 = A[i+k][j+7];
                    }

                    B[j][i+k] = t1;
                    if (j+1 < M) {
                        B[j+1][i+k] = t2;
                    }
                    if (j+2 < M) {
                        B[j+2][i+k] = t3;
                    }
                    if (j+3 < M) {
                        B[j+3][i+k] = t4;
                    }
                }

                // write to right
                if ((i+k+4) < N) {
                    if (j+4 < M) {
                        B[j][i+k+4] = t5;
                    }
                    if (j+5 < M) {
                        B[j+1][i+k+4] = t6;
                    }
                    if (j+6 < M) {
                        B[j+2][i+k+4] = t7;
                    }
                    if (j+7 < M) {
                        B[j+3][i+k+4] = t8;
                    }
                } else {
                    if (j+4 < M) {
                        B[j+4][i+k] = t5;
                    }
                    if (j+5 < M) {
                        B[j+5][i+k] = t6;
                    }
                    if (j+6 < M) {
                        B[j+6][i+k] = t7;
                    }
                    if (j+7 < M) {
                        B[j+7][i+k] = t8;
                    }
                }
            }

            // print(M, N, B);
            // transfer nums to down side
            for (k = 0; k < 4; k++) {
                if (j+k < M) {
                    if (i+4 < N) {
                        t1 = B[j+k][i+4];
                    }
                    if (i+5 < N) {
                        t2 = B[j+k][i+5];
                    }
                    if (i+6 < N) {
                        t3 = B[j+k][i+6];
                    }
                    if (i+7 < N) {
                        t4 = B[j+k][i+7];
                    }
                }

                if ((j+k+4) < M) {
                    if (i+4 < N) {
                        B[j+k+4][i] = t1;
                    }
                    if (i+5 < N) {
                        B[j+k+4][i+1] = t2;
                    }
                    if (i+6 < N) {
                        B[j+k+4][i+2] = t3;
                    }
                    if (i+7 < N) {
                        B[j+k+4][i+3] = t4;
                    }
                }
            }

            // print(M, N, B);
        }
    }   
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

