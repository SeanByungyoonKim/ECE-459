// from the Cilk manual: http://supertech.csail.mit.edu/cilk/manual-5.4.6.pdf
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CUTOFF 7

int safe(char * config, int i, int j)
{
    int r, s;
    for (r = 0; r < i; r++)
    {
        s = config[r];
        if (j == s || i-r==j-s || i-r==s-j)
            return 0;
    }
    return 1;
}

int count = 0;
int cut = 1;

void nqueens(char *config, int n, int i)
{
    char *new_config;
    int j, status;

    /* try each possible position for queen <i> */
    
    if (i==n) {
        #pragma omp atomic
        count++;
    }
    cut++;
    #pragma omp parallel for private (new_config) shared(count,n,i) 
    for (j=0; j<n; j++) { 
        /* allocate a temporary array and copy the config into it */
        new_config = malloc((i+1)*sizeof(char));
        memcpy(new_config, config, i*sizeof(char));
        status = safe(new_config, i, j);
        if (status) {
            new_config[i] = j;
            if (cut <= CUTOFF) {
                nqueens(new_config,n,i+1);
            }
            else {
                #pragma omp task shared(n,i)
                nqueens(new_config, n, i+1);
            }
        }
        #pragma omp taskwait
        free(new_config);
    }        

    return;
}

int main(int argc, char *argv[])
{
    int n;
    char *config;

    if (argc < 2)
    {
        printf("%s: number of queens required\n", argv[0]);
        return 1;
    }

    n = atoi(argv[1]);
    config = malloc(n * sizeof(char));

    // omp_set_num_threads(n);
    printf("running queens %d\n", n);
    nqueens(config,n,0);  
    printf("# solutions: %d\n", count);
    free(config);

    return 0;
}
