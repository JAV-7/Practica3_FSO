/*           
        Para compilar incluir la librería  m (matemáticas)
        Ejemplo:
            gcc -o mercator mercator.c  -lm
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <semaphore.h> // Cabecera para especIfico para manejo de semAforos
#include <fcntl.h> // Llamada al sistema de Unix/Linux utilizada para manipular descriptores de archivos abiertos
#include <sys/types.h>

#define NPROCS 4
#define SERIES_MEMBER_COUNT 200000

typedef struct { 
    double sums[NPROCS];
    int proc_count;
    int start_all;
    double x_val;
    double res;
    sem_t mutex; // SemAforo para indicar espera de procesos para afectar variable global
    sem_t s_start; // SemAforo para indicar arranque de procesos
    sem_t s_finish; // SemAforo para indicar fin de procesos
} SHARED;

SHARED *shared;


double get_member(int n, double x)
{
    int i;
    double numerator = 1;
    for(i=0; i<n; i++ )
        numerator = numerator*x;
    if (n % 2 == 0)
        return ( - numerator / n );
    else
        return numerator/n;
}

void proc(int proc_num)
{
    int i;
    // INICIO DE SECCION CRITICA
    printf("Proceso %d en espera de semaforo\n", proc_num);
    sem_wait(&shared->s_start);
    printf("Proceso %d entra a zona de calculo\n", proc_num);
    // Cada proceso realiza el cálculo de los términos que le tocan
    shared->sums[proc_num] = 0;
    for(i=proc_num; i<SERIES_MEMBER_COUNT;i+=NPROCS)
        shared->sums[proc_num] += get_member(i+1, shared->x_val);
    // Incrementa la variable proc_count que es la cantidad de procesos que terminaron su cálculo
    printf("Proceso %d sale de zona de calculo\n", proc_num);
    sem_wait(&shared->mutex);
    printf("Proceso %d entra a zona de mutex\n", proc_num);
    shared->proc_count++;
    if(shared->proc_count == NPROCS)
    	sem_post(&shared->s_finish);
    printf("Proceso %d sale de zona de mutex\n", proc_num);
    sem_post(&shared->mutex);
    exit(0);
}

void master_proc()
{
    int i;
    // Obtener el valor de x desde el archivo entrada.txt
    FILE *fp = fopen("entrada.txt","r");
    if(fp==NULL)
        exit(1);

    fscanf(fp,"%lf",&shared->x_val);
    fclose(fp);
    // InstrucciOn para que empiecen los calculos de todos los procesos
    printf("Se leyO el archivo\n");
    shared->start_all = 1;
    for(i = 0; i < NPROCS; i++)
    	sem_post(&shared->s_start);

    // Espera a que todos los procesos terminen su cálculo
    sem_wait(&shared->s_finish);
    // Una vez que todos terminan, suma el total de cada uno
    shared->res = 0;
    for(i=0; i<NPROCS; i++)
        shared->res += shared->sums[i];
    exit(0);
}

int main()
{
    int *threadIdPtr;

    long long start_ts;
    long long stop_ts;
    long long elapsed_time;
    long lElapsedTime;
    struct timeval ts;
    int i;
    int p;
    int shmid;
    int status;

    // Solicita y conecta la memoria compartida
   shmid = shmget(IPC_PRIVATE, sizeof(SHARED), 0666 | IPC_CREAT);
   
   shared = shmat(shmid, NULL, 0);
   if (shared == (void *) -1) {
      perror("shmat");
      exit(1);
   }

    // inicializa las variables en memoria compartida
    shared->proc_count = 0;
    shared->start_all = 0;
    
    // DeclaraciOn de semAforos
    sem_init(&shared->s_finish, 1, 0);
    sem_init(&shared->s_start, 1, 0);
    sem_init(&shared->mutex, 1, 1);
    
    
    gettimeofday(&ts, NULL);
    start_ts = ts.tv_sec; // Tiempo inicial
    for(i=0; i<NPROCS;i++)
    {
        p = fork();
        if(p==0)
            proc(i);
    }
    p = fork();
    if(p==0)
        master_proc();
    printf("El recuento de ln(1 + x) miembros de la serie de Mercator es %d\n",SERIES_MEMBER_COUNT);

    for(int i=0;i<NPROCS+1;i++)
    {
        wait(&status);
        if(status==0x100)   // Si el master_proc termina con error
        {
            fprintf(stderr,"Proceso no puede abrir el archivo de entrada\n");
            break;
        }
    }

    gettimeofday(&ts, NULL);
    stop_ts = ts.tv_sec; // Tiempo final
    elapsed_time = stop_ts - start_ts;

    printf("Tiempo = %lld segundos\n", elapsed_time);
    printf("El resultado es %10.8f\n", shared->res);
    printf("Llamando a la función ln(1 + %f) = %10.8f\n",shared->x_val, log(1+shared->x_val));
    // Desconecta y elimnina la memoria compartida
    shmdt(shared);
    shmctl(shmid,IPC_RMID,NULL);
}
