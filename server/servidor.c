#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <stdatomic.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define PORTA 5000
#define TAM_BUFFER 1024
#define TAM_ACUMULADO 4096
#define TAM_MSG_ALARME 256

#define PERIODO_SENSORES_US 1000000
#define PERIODO_CONTROLE_US 1000000

#define LIMITE_TEMPERATURA_CRITICA 35.0
#define LIMITE_UMIDADE_CRITICA 20.0

typedef struct {
    float temperatura;
    float umidade;
    float luminosidade;

    int aquecedor;
    int ventilador;
    int bomba;

    int modo_automatico;
    int alarme_ativo;

    float temperatura_desejada;
} EstadoEstufa;

EstadoEstufa estado = {
    .temperatura = 25.0,
    .umidade = 60.0,
    .luminosidade = 70.0,

    .aquecedor = 0,
    .ventilador = 0,
    .bomba = 0,

    .modo_automatico = 0,
    .alarme_ativo = 0,

    .temperatura_desejada = 27.0
};

pthread_mutex_t mutex_estado = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_alarme = PTHREAD_COND_INITIALIZER;

atomic_int executando = ATOMIC_VAR_INIT(1);

int evento_alarme_pendente = 0;
char mensagem_alarme[TAM_MSG_ALARME] = "";

void remover_quebra_linha(char *texto) {
    texto[strcspn(texto, "\r\n")] = '\0';
}

const char *ligado_desligado(int valor) {
    if (valor) {
        return "ON";
    }

    return "OFF";
}

void limitar_valores_estado(void) {
    if (estado.temperatura < 0.0) {
        estado.temperatura = 0.0;
    }

    if (estado.temperatura > 60.0) {
        estado.temperatura = 60.0;
    }

    if (estado.umidade < 0.0) {
        estado.umidade = 0.0;
    }

    if (estado.umidade > 100.0) {
        estado.umidade = 100.0;
    }

    if (estado.luminosidade < 0.0) {
        estado.luminosidade = 0.0;
    }

    if (estado.luminosidade > 100.0) {
        estado.luminosidade = 100.0;
    }
}

void verificar_e_sinalizar_alarme(void) {
    if (estado.alarme_ativo || evento_alarme_pendente) {
        return;
    }

    if (estado.temperatura >= LIMITE_TEMPERATURA_CRITICA) {
        snprintf(
            mensagem_alarme,
            sizeof(mensagem_alarme),
            "Temperatura critica detectada: %.1f C",
            estado.temperatura
        );

        evento_alarme_pendente = 1;
        pthread_cond_signal(&cond_alarme);
    }
    else if (estado.umidade <= LIMITE_UMIDADE_CRITICA) {
        snprintf(
            mensagem_alarme,
            sizeof(mensagem_alarme),
            "Umidade critica detectada: %.1f%%",
            estado.umidade
        );

        evento_alarme_pendente = 1;
        pthread_cond_signal(&cond_alarme);
    }
}

void montar_status_com_mutex(char *resposta, size_t tamanho) {
    pthread_mutex_lock(&mutex_estado);

    snprintf(
        resposta,
        tamanho,
        "STATUS: TEMP=%.1f C; UMIDADE=%.1f%%; LUMINOSIDADE=%.1f%%; AQUECEDOR=%s; VENTILADOR=%s; BOMBA=%s; MODO=%s; TEMP_DESEJADA=%.1f C; ALARME=%s",
        estado.temperatura,
        estado.umidade,
        estado.luminosidade,
        ligado_desligado(estado.aquecedor),
        ligado_desligado(estado.ventilador),
        ligado_desligado(estado.bomba),
        estado.modo_automatico ? "AUTOMATICO" : "MANUAL",
        estado.temperatura_desejada,
        estado.alarme_ativo ? "ATIVO" : "INATIVO"
    );

    pthread_mutex_unlock(&mutex_estado);
}

void processar_comando(const char *comando, char *resposta, size_t tamanho) {
    if (strcmp(comando, "STATUS") == 0) {
        montar_status_com_mutex(resposta, tamanho);
        return;
    }

    pthread_mutex_lock(&mutex_estado);

    if (strcmp(comando, "HEATER_ON") == 0) {
        estado.aquecedor = 1;
        snprintf(resposta, tamanho, "OK: Aquecedor ligado.");
    }
    else if (strcmp(comando, "HEATER_OFF") == 0) {
        estado.aquecedor = 0;
        snprintf(resposta, tamanho, "OK: Aquecedor desligado.");
    }
    else if (strcmp(comando, "FAN_ON") == 0) {
        estado.ventilador = 1;
        snprintf(resposta, tamanho, "OK: Ventilador ligado.");
    }
    else if (strcmp(comando, "FAN_OFF") == 0) {
        estado.ventilador = 0;
        snprintf(resposta, tamanho, "OK: Ventilador desligado.");
    }
    else if (strcmp(comando, "PUMP_ON") == 0) {
        estado.bomba = 1;
        snprintf(resposta, tamanho, "OK: Bomba ligada.");
    }
    else if (strcmp(comando, "PUMP_OFF") == 0) {
        estado.bomba = 0;
        snprintf(resposta, tamanho, "OK: Bomba desligada.");
    }
    else if (strcmp(comando, "AUTO_ON") == 0) {
        estado.modo_automatico = 1;
        snprintf(resposta, tamanho, "OK: Modo automatico ativado.");
    }
    else if (strcmp(comando, "AUTO_OFF") == 0) {
        estado.modo_automatico = 0;
        snprintf(resposta, tamanho, "OK: Modo manual ativado.");
    }
    else if (strncmp(comando, "SET_TEMP ", 9) == 0) {
        float nova_temperatura = atof(comando + 9);

        if (nova_temperatura < 5.0 || nova_temperatura > 45.0) {
            snprintf(resposta, tamanho, "ERRO: Temperatura desejada deve estar entre 5 e 45 graus.");
        } else {
            estado.temperatura_desejada = nova_temperatura;
            snprintf(resposta, tamanho, "OK: Temperatura desejada alterada para %.1f C.", estado.temperatura_desejada);
        }
    }
    else if (strcmp(comando, "RESET_ALARM") == 0) {
        estado.alarme_ativo = 0;
        evento_alarme_pendente = 0;
        mensagem_alarme[0] = '\0';

        snprintf(resposta, tamanho, "OK: Alarme resetado.");
    }
    else if (strcmp(comando, "HELP") == 0) {
        snprintf(
            resposta,
            tamanho,
            "COMANDOS: STATUS, AUTO_ON, AUTO_OFF, SET_TEMP valor, HEATER_ON, HEATER_OFF, FAN_ON, FAN_OFF, PUMP_ON, PUMP_OFF, RESET_ALARM, HELP, QUIT"
        );
    }
    else if (strcmp(comando, "QUIT") == 0) {
        snprintf(resposta, tamanho, "OK: Encerrando conexao com o cliente.");
    }
    else {
        snprintf(resposta, tamanho, "ERRO: Comando desconhecido. Use HELP para listar os comandos.");
    }

    pthread_mutex_unlock(&mutex_estado);
}

void *thread_sensores(void *arg) {
    (void)arg;

    printf("[THREAD SENSORES] Iniciada. Periodo: 1 segundo.\n");

    while (atomic_load(&executando)) {
        pthread_mutex_lock(&mutex_estado);

        if (estado.aquecedor && !estado.ventilador) {
            estado.temperatura += 0.7;
        }
        else if (estado.ventilador && !estado.aquecedor) {
            estado.temperatura -= 0.5;
        }
        else {
            if (estado.temperatura > 24.0) {
                estado.temperatura -= 0.1;
            }
            else if (estado.temperatura < 24.0) {
                estado.temperatura += 0.1;
            }
        }

        if (estado.bomba) {
            estado.umidade += 1.0;
        } else {
            estado.umidade -= 0.2;
        }

        int variacao_luz = (rand() % 3) - 1;
        estado.luminosidade += variacao_luz * 0.5;

        limitar_valores_estado();
        verificar_e_sinalizar_alarme();

        pthread_mutex_unlock(&mutex_estado);

        usleep(PERIODO_SENSORES_US);
    }

    return NULL;
}

void *thread_controle_automatico(void *arg) {
    (void)arg;

    printf("[THREAD CONTROLE] Iniciada. Periodo: 1 segundo.\n");

    while (atomic_load(&executando)) {
        pthread_mutex_lock(&mutex_estado);

        if (estado.modo_automatico) {
            if (estado.temperatura < estado.temperatura_desejada - 0.5) {
                estado.aquecedor = 1;
                estado.ventilador = 0;
            }
            else if (estado.temperatura > estado.temperatura_desejada + 0.5) {
                estado.aquecedor = 0;
                estado.ventilador = 1;
            }
            else {
                estado.aquecedor = 0;
                estado.ventilador = 0;
            }

            if (estado.umidade < 45.0) {
                estado.bomba = 1;
            }
            else if (estado.umidade > 60.0) {
                estado.bomba = 0;
            }
        }

        pthread_mutex_unlock(&mutex_estado);

        usleep(PERIODO_CONTROLE_US);
    }

    return NULL;
}

void *thread_alarme(void *arg) {
    (void)arg;

    printf("[THREAD ALARME] Iniciada. Aguardando eventos criticos.\n");

    while (atomic_load(&executando)) {
        pthread_mutex_lock(&mutex_estado);

        while (!evento_alarme_pendente && atomic_load(&executando)) {
            pthread_cond_wait(&cond_alarme, &mutex_estado);
        }

        if (!atomic_load(&executando)) {
            pthread_mutex_unlock(&mutex_estado);
            break;
        }

        estado.alarme_ativo = 1;

        char mensagem_local[TAM_MSG_ALARME];
        snprintf(mensagem_local, sizeof(mensagem_local), "%s", mensagem_alarme);

        evento_alarme_pendente = 0;

        pthread_mutex_unlock(&mutex_estado);

        printf("[THREAD ALARME] ALARME ATIVADO: %s\n", mensagem_local);
    }

    return NULL;
}

int criar_socket_servidor(void) {
    int servidor_fd;
    int opcao = 1;
    struct sockaddr_in endereco;

    servidor_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (servidor_fd < 0) {
        perror("Erro ao criar socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(servidor_fd, SOL_SOCKET, SO_REUSEADDR, &opcao, sizeof(opcao)) < 0) {
        perror("Erro em setsockopt");
        close(servidor_fd);
        exit(EXIT_FAILURE);
    }

    memset(&endereco, 0, sizeof(endereco));

    endereco.sin_family = AF_INET;
    endereco.sin_addr.s_addr = INADDR_ANY;
    endereco.sin_port = htons(PORTA);

    if (bind(servidor_fd, (struct sockaddr *)&endereco, sizeof(endereco)) < 0) {
        perror("Erro no bind");
        close(servidor_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(servidor_fd, 1) < 0) {
        perror("Erro no listen");
        close(servidor_fd);
        exit(EXIT_FAILURE);
    }

    return servidor_fd;
}

int enviar_tudo(int cliente_fd, const char *dados, size_t tamanho) {
    size_t total_enviado = 0;

    while (total_enviado < tamanho) {
        ssize_t enviados = send(
            cliente_fd,
            dados + total_enviado,
            tamanho - total_enviado,
            0
        );

        if (enviados < 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("[THREAD REDE] Erro ao enviar dados");
            return -1;
        }

        if (enviados == 0) {
            printf("[THREAD REDE] Conexao encerrada durante envio.\n");
            return -1;
        }

        total_enviado += (size_t)enviados;
    }

    return 0;
}

int enviar_resposta(int cliente_fd, const char *resposta) {
    char mensagem[TAM_BUFFER];

    int tamanho = snprintf(mensagem, sizeof(mensagem), "%s\n", resposta);

    if (tamanho < 0) {
        printf("[THREAD REDE] Erro ao montar mensagem de resposta.\n");
        return -1;
    }

    if ((size_t)tamanho >= sizeof(mensagem)) {
        printf("[THREAD REDE] Resposta muito longa para o buffer.\n");
        return -1;
    }

    return enviar_tudo(cliente_fd, mensagem, (size_t)tamanho);
}

int processar_linha_comando(int cliente_fd, char *linha) {
    char resposta[TAM_BUFFER];

    remover_quebra_linha(linha);

    if (strlen(linha) == 0) {
        return 0;
    }

    memset(resposta, 0, sizeof(resposta));

    printf("[THREAD REDE] Comando recebido: %s\n", linha);

    processar_comando(linha, resposta, sizeof(resposta));

    if (enviar_resposta(cliente_fd, resposta) < 0) {
        printf("[THREAD REDE] Falha ao enviar resposta. Encerrando conexao com cliente.\n");
        return 1;
    }

    if (strcmp(linha, "QUIT") == 0) {
        printf("[THREAD REDE] Conexao encerrada pelo comando QUIT.\n");
        return 1;
    }

    return 0;
}

void atender_cliente(int cliente_fd) {
    char buffer[TAM_BUFFER];
    char acumulado[TAM_ACUMULADO];
    size_t usado = 0;

    memset(acumulado, 0, sizeof(acumulado));

    if (enviar_resposta(
        cliente_fd,
        "Servidor da Estufa Inteligente conectado. Digite HELP para ver os comandos."
    ) < 0) {
        close(cliente_fd);
        return;
    }

    while (1) {
        memset(buffer, 0, sizeof(buffer));

        int bytes_recebidos = recv(cliente_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_recebidos <= 0) {
            printf("[THREAD REDE] Cliente desconectado.\n");
            break;
        }

        if (usado + (size_t)bytes_recebidos >= sizeof(acumulado)) {
            if (enviar_resposta(
                cliente_fd,
                "ERRO: Linha de comando muito longa. Buffer interno foi limpo."
            ) < 0) {
                break;
            }

            usado = 0;
            memset(acumulado, 0, sizeof(acumulado));
            continue;
        }

        memcpy(acumulado + usado, buffer, (size_t)bytes_recebidos);
        usado += (size_t)bytes_recebidos;
        acumulado[usado] = '\0';

        char *inicio = acumulado;
        char *quebra = NULL;
        int encerrar_conexao = 0;

        while ((quebra = strchr(inicio, '\n')) != NULL) {
            *quebra = '\0';

            encerrar_conexao = processar_linha_comando(cliente_fd, inicio);

            inicio = quebra + 1;

            if (encerrar_conexao) {
                break;
            }
        }

        if (encerrar_conexao) {
            break;
        }

        size_t restante = strlen(inicio);

        if (restante > 0) {
            memmove(acumulado, inicio, restante);
        }

        usado = restante;
        acumulado[usado] = '\0';
    }

    close(cliente_fd);
}

void *thread_rede(void *arg) {
    (void)arg;

    int servidor_fd;
    int cliente_fd;
    struct sockaddr_in endereco_cliente;
    socklen_t tamanho_cliente;

    servidor_fd = criar_socket_servidor();

    printf("[THREAD REDE] Servidor TCP iniciado na porta %d.\n", PORTA);
    printf("[THREAD REDE] Aguardando conexoes...\n");

    while (atomic_load(&executando)) {
        tamanho_cliente = sizeof(endereco_cliente);

        cliente_fd = accept(
            servidor_fd,
            (struct sockaddr *)&endereco_cliente,
            &tamanho_cliente
        );

        if (cliente_fd < 0) {
            perror("Erro no accept");
            continue;
        }

        printf("[THREAD REDE] Cliente conectado: %s\n", inet_ntoa(endereco_cliente.sin_addr));

        atender_cliente(cliente_fd);

        printf("[THREAD REDE] Aguardando nova conexao...\n");
    }

    close(servidor_fd);

    return NULL;
}

int main(void) {
    pthread_t tid_sensores;
    pthread_t tid_controle;
    pthread_t tid_rede;
    pthread_t tid_alarme;

    srand(time(NULL));
    signal(SIGPIPE, SIG_IGN);

    printf("Servidor da Estufa Inteligente iniciado.\n");
    printf("Criando threads do servidor...\n");

    if (pthread_create(&tid_sensores, NULL, thread_sensores, NULL) != 0) {
        perror("Erro ao criar thread de sensores");
        return EXIT_FAILURE;
    }

    if (pthread_create(&tid_controle, NULL, thread_controle_automatico, NULL) != 0) {
        perror("Erro ao criar thread de controle");
        return EXIT_FAILURE;
    }

    if (pthread_create(&tid_rede, NULL, thread_rede, NULL) != 0) {
        perror("Erro ao criar thread de rede");
        return EXIT_FAILURE;
    }

    if (pthread_create(&tid_alarme, NULL, thread_alarme, NULL) != 0) {
        perror("Erro ao criar thread de alarme");
        return EXIT_FAILURE;
    }

    pthread_join(tid_sensores, NULL);
    pthread_join(tid_controle, NULL);
    pthread_join(tid_rede, NULL);
    pthread_join(tid_alarme, NULL);

    pthread_mutex_destroy(&mutex_estado);
    pthread_cond_destroy(&cond_alarme);

    return EXIT_SUCCESS;
}
