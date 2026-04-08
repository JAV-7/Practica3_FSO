/*           
        Para compilar incluir la librería librt (real-time)
        que incluye la API de colas de mensajes POSIX (mq_*)
        y la librería m (matemáticas)
        

        De esta forma:
            gcc mercator_msg.c -o mercator_msg -lrt -lm
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

#define NPROCS 4
#define SERIES_MEMBER_COUNT 200000

#define QUEUE_START "/start_queue"
#define QUEUE_RESULT "/result_queue"

// Mensaje de inicio (master -> esclavos)
typedef struct {
    int proc_num;
    double x;
} start_msg;

// Mensaje de resultado (esclavos -> master)
typedef struct {
    double partial_sum;
} result_msg;

// Función de la serie de Mercator
double get_member(int n, double x)
{
    double numerator = 1.0;

    for(int i = 0; i < n; i++)
        numerator *= x;

    if (n % 2 == 0)
        return (-numerator / n);
    else
        return (numerator / n);
}

// Proceso esclavo
void proc()
{
    mqd_t q_start, q_result;
    start_msg msg;
    result_msg res;

    q_start = mq_open(QUEUE_START, O_RDONLY);
    q_result = mq_open(QUEUE_RESULT, O_WRONLY);

    if(q_start == -1 || q_result == -1) {
        perror("Error abriendo colas en esclavo");
        exit(1);
    }

    // bloqueante sin busy waiting
    mq_receive(q_start, (char*)&msg, sizeof(msg), NULL);

    int proc_num = msg.proc_num;
    double x = msg.x;

    res.partial_sum = 0.0;

    for(int i = proc_num; i < SERIES_MEMBER_COUNT; i += NPROCS)
        res.partial_sum += get_member(i + 1, x);

    mq_send(q_result, (char*)&res, sizeof(res), 0);

    mq_close(q_start);
    mq_close(q_result);

    exit(0);
}

// Proceso maestro
void master_proc()
{
    mqd_t q_start, q_result;
    start_msg msg;
    result_msg res;

    q_start = mq_open(QUEUE_START, O_WRONLY);
    q_result = mq_open(QUEUE_RESULT, O_RDONLY);

    if(q_start == -1 || q_result == -1) {
        perror("Error abriendo colas en master");
        exit(1);
    }

    FILE *fp = fopen("entrada.txt", "r");
    if(fp == NULL) {
        perror("No se pudo abrir entrada.txt");
        exit(1);
    }

    double x;
    fscanf(fp, "%lf", &x);
    fclose(fp);

    // Enviar trabajo a esclavos
    for(int i = 0; i < NPROCS; i++) {
        msg.proc_num = i;
        msg.x = x;
        mq_send(q_start, (char*)&msg, sizeof(msg), 0);
    }

    // Recibir resultados
    double total = 0.0;

    for(int i = 0; i < NPROCS; i++) {
        mq_receive(q_result, (char*)&res, sizeof(res), NULL);
        total += res.partial_sum;
    }

    printf("El recuento de miembros es %d\n", SERIES_MEMBER_COUNT);
    printf("Resultado (serie) = %10.8f\n", total);
    printf("Resultado real ln(1 + %f) = %10.8f\n", x, log(1 + x));

    mq_close(q_start);
    mq_close(q_result);

    exit(0);
}

int main()
{
    struct timeval ts;
    long long start_ts, stop_ts;

    // Configurar atributos de colas
    struct mq_attr attr_start;
    attr_start.mq_flags = 0;
    attr_start.mq_maxmsg = 10;
    attr_start.mq_msgsize = sizeof(start_msg);
    attr_start.mq_curmsgs = 0;

    struct mq_attr attr_result;
    attr_result.mq_flags = 0;
    attr_result.mq_maxmsg = 10;
    attr_result.mq_msgsize = sizeof(result_msg);
    attr_result.mq_curmsgs = 0;

    // Crear colas
    mqd_t q_start = mq_open(QUEUE_START, O_CREAT | O_RDWR, 0666, &attr_start);
    mqd_t q_result = mq_open(QUEUE_RESULT, O_CREAT | O_RDWR, 0666, &attr_result);

    if(q_start == -1 || q_result == -1) {
        perror("Error creando colas");
        exit(1);
    }

    gettimeofday(&ts, NULL);
    start_ts = ts.tv_sec;

    // Crear procesos esclavos
    for(int i = 0; i < NPROCS; i++) {
        if(fork() == 0)
            proc();
    }

    // Crear proceso maestro
    if(fork() == 0)
        master_proc();

    // Esperar a todos
    for(int i = 0; i < NPROCS + 1; i++)
        wait(NULL);

    gettimeofday(&ts, NULL);
    stop_ts = ts.tv_sec;

    printf("Tiempo = %lld segundos\n", (stop_ts - start_ts));

    // Limpiar colas
    mq_close(q_start);
    mq_close(q_result);

    mq_unlink(QUEUE_START);
    mq_unlink(QUEUE_RESULT);

    return 0;
}