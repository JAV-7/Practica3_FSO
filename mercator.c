/*      
	Practica 03 - Semáforos
	Estudiantes:
		Francisco Javier Ramos Jiménez
		Jesús Alejandro de la Rosa Arroyo
	Profesor:
		José Luis ELvira Valenzuela	
	Materia: 
		Fundamentos de Sistemas Operativos
	16/04/2026
	
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
// Cabecera para específico para manejo de semáforos de POSIX
#include <semaphore.h>
// Llamada al sistema de Unix/Linux utilizada para manipular descriptores de archivos abiertos
#include <fcntl.h>
// DefiniciOn de tipos
#include <sys/types.h>

#define NPROCS 4
#define SERIES_MEMBER_COUNT 200000

typedef struct {
    double sums[NPROCS];
    int proc_count;
    int start_all;
    double x_val;
    double res;
    /* En un principio, se intentó implementar los semáforos fuera de la estructura, sin embargo,
     * esto representó un problema; cada proceso se estancaba en wait. Tras checar dicho problema,
     * nos percatamos que pareciera que cada proceso tenía un semáforo propio debido a cómo se definen
     * estos en main. Primero, se intentó declararlos a nivel de archivo, pero generó errores.
     * Entonces, se decidió incluirlos dentro de la estructura shared, la cual asegura que TODOS
     * los procesos tienen acceso a esta.
     */
    sem_t mutex; // Semáforo para indicar espera de procesos para afectar proc_count
    sem_t s_start; // Semáforo para indicar arranque de procesos
    sem_t s_finish; // Semáforo para indicar fin de procesos
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
    // Inicio de sección de cálculo
    printf("Proceso %d en espera de semaforo\n", proc_num);
    sem_wait(&shared->s_start); // Espera a instrucción para arrancar cAlculo
    printf("Proceso %d entra a zona de calculo\n", proc_num);
    // Cada proceso realiza el cálculo de los términos que le tocan
    shared->sums[proc_num] = 0;
    for(i=proc_num; i<SERIES_MEMBER_COUNT;i+=NPROCS)
        shared->sums[proc_num] += get_member(i+1, shared->x_val);
   
    printf("Proceso %d sale de zona de calculo\n", proc_num);

    // Zona de semáforo mutex, que asegura la modificación de proc_count sin que se sobreescriba
    sem_wait(&shared->mutex);
    printf("Proceso %d entra a zona de mutex\n", proc_num);

    // Incrementa la variable proc_count que es la cantidad de procesos que terminaron su cálculo
    shared->proc_count++;

    // Si proc_count llega a ser igual al número de procesos, le hace post al semáforo s_finish
    if(shared->proc_count == NPROCS)
    // Señal de terminación de los cálculos individuales
    sem_post(&shared->s_finish);
    printf("Proceso %d sale de zona de mutex\n", proc_num);
    // Libera el semáforo para que el siguiente proceso pueda entrar a la SC
    sem_post(&shared->mutex);
    exit(0);
}

void master_proc()
{
    int i;
    int result; // Variable para verificar el resultado de fscanf
    // Obtener el valor de x desde el archivo entrada.txt
    FILE *fp = fopen("entrada.txt","r");
    if(fp==NULL)
        exit(1);

    result = fscanf(fp,"%lf",&shared->x_val);
    if(result != 1) {
        fprintf(stderr, "Error: No se pudo leer el valor de x desde entrada.txt\n");
        fclose(fp);
        exit(1);
    }
    fclose(fp);
    // Instrucción para que empiecen los calculos de todos los procesos
    printf("Se leyO el archivo\n");
    /* shared->start_all = 1; Esta instrucción queda obsoleta debido a la implementación
     * del semáforo s_start
     */
    for(i = 0; i < NPROCS; i++)
    sem_post(&shared->s_start);

    /* Espera a que todos los procesos terminen su cálculo.
     * Solo se activa una vez, aunque otra alternativa es poner s_wait en proc() a nivel
     * de sem_post(&shared->mutex) después de este y tratar el sem_wait(&shared->s_finish)
     * con un ciclo for de NPROCS tal como en sem_post(&shared->s_start)
     */
    sem_wait(&shared->s_finish);
    // Una vez que todos terminan, suma el total de cada uno
    shared->res = 0;
    for(i=0; i<NPROCS; i++)
        shared->res += shared->sums[i];
    exit(0);
}

int main()
{
    long long start_ts;
    long long stop_ts;
    long long elapsed_time;
    struct timeval ts;
    int i;
    int p;
    int shmid;
    int status;

    // Solicita y conecta la memoria compartida
   
    /* Se modificó esta lInea de código debido a segmentation fault e invalid argument.
     * IPC_PRIVATE le indica al SO que implemente un segmento único y privado, a diferencia
     * del anterior. Se evita el uso de comandos como ipcrm para limpiar buffer.
     * Si se le asigna -1 a shmid, significa que hubo un error. En dicho caso, abortar el
     * programa.
     */
   shmid = shmget(IPC_PRIVATE, sizeof(SHARED), 0666 | IPC_CREAT);
   if (shmid == -1) {
    perror("shmget");
    exit(1);
   }
   
   /* Al compilar el programa tras hacer los cambios de semáforos, se generó un error
    * de segmentation fault en esta variable. La razón: se accedía a memoria inválida.
    * Si algo fallaba en la asignación de shared, se le asignaba (void *) -1.
    * En este cambio, se verifica si se le asignó un valor inválido. Si sucede, aborta el
    * programa y comunica el error.
    * En caso de no funcionar, debería limpiarse el buffer. Correr ipcs -m y buscar el proceso
    * a borrar con ipcrm -m <ID>
    */
   shared = shmat(shmid, NULL, 0);
   if (shared == (void *) -1) {
      perror("shmat");
      exit(1);
   }

    // inicializa las variables en memoria compartida
    shared->proc_count = 0;
    shared->start_all = 0;
   
    /* DeclaraciOn de semáforos
     * s_finish inicia en 0, el 1 indica que es compartido entre procesos. NOTA: si es 0,
     * significa que es un semáforo compartido entre hilos, lo cual no se busca en esta practica.
     */
    sem_init(&shared->s_finish, 1, 0);
    sem_init(&shared->s_start, 1, 0);
    // Mutex inicializado en 1, indicando que el primer proceso que llegue puede acceder a la secciOn.
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

    for(i=0;i<NPROCS+1;i++)
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
    // sem_destroy libera los recursos del sistema usados por semAforos POSIX
    sem_destroy(&shared->s_start);
    sem_destroy(&shared->s_finish);
    sem_destroy(&shared->mutex);
    shmdt(shared);
    shmctl(shmid,IPC_RMID,NULL);
   
    return 0; 
}

