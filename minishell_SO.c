/*
Alunos: 
    Alexandre Moura Caldeira - 202104938
    Amanda Paulino Franco dos Santos - 202206108
    Júlia Rodrigues Franco - 202104983
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_ARGS 64
#define BUFFER 1024
#define MAX_CMDS 10

char **historico = NULL;
int contador_historico = 0;
int capacidade_historico = 0;

void adicionar_comando_historico(const char *comando_adicionar) {
    if (contador_historico == capacidade_historico) {
        capacidade_historico = (capacidade_historico == 0) ? 10 : capacidade_historico * 2;
        char **novo_historico = (char **)realloc(historico, capacidade_historico * sizeof(char *));
        if (novo_historico == NULL) {
            perror("Erro ao realocar historico");
            return;
        }
        historico = novo_historico;
    }
    historico[contador_historico] = strdup(comando_adicionar);
    contador_historico++;
}

void mostrar_historico() {
    if (contador_historico == 0) {
        printf("Nenhum historico encontrado.\n");
        return;
    }

    printf("\n--- Histórico ---\n");
    for (int indice = 0; indice < contador_historico; indice++) {
        printf("%d: %s\n", indice + 1, historico[indice]);
    }
    printf("-----------------\n\n");
}

void ler_comando(char *buffer_comando) {
    printf("mini-shell> ");
    fflush(stdout);
    fgets(buffer_comando, BUFFER, stdin);
    buffer_comando[strcspn(buffer_comando, "\n")] = '\0';
}

int analisar_comando(char *string_comando, char **argumentos, int *em_segundo_plano, char **arquivo_entrada, char **arquivo_saida) {
    char *token_atual;
    int indice = 0;
    char *ponteiro_salvo;

    *em_segundo_plano = 0;
    *arquivo_entrada = NULL;
    *arquivo_saida = NULL;

    if (strchr(string_comando, '&')) {
        *em_segundo_plano = 1;
        string_comando[strcspn(string_comando, "&")] = '\0';
    }

    token_atual = strtok_r(string_comando, " \t\n", &ponteiro_salvo);
    while (token_atual != NULL) {
        if (strcmp(token_atual, "<") == 0) {
            *arquivo_entrada = strtok_r(NULL, " \t\n", &ponteiro_salvo);
        } 
        else if (strcmp(token_atual, ">") == 0) {
            *arquivo_saida = strtok_r(NULL, " \t\n", &ponteiro_salvo);
        } 
        else {
            argumentos[indice++] = token_atual;
        }
        token_atual = strtok_r(NULL, " \t\n", &ponteiro_salvo);
    }
    argumentos[indice] = NULL;

    return argumentos[0] != NULL;
}

void executar_simples(char **argumentos, int em_segundo_plano, char *arquivo_entrada, char *arquivo_saida) {
    pid_t id_processo = fork();

    if (id_processo < 0) {
        perror("Erro ao criar processo");
        return;
    }

    if (id_processo == 0) {
        if (arquivo_entrada) {
            int descritor_entrada = open(arquivo_entrada, O_RDONLY);
            if (descritor_entrada < 0) {
                perror("Erro ao abrir arquivo de entrada");
                exit(EXIT_FAILURE);
            }
            dup2(descritor_entrada, STDIN_FILENO);
            close(descritor_entrada);
        }

        if (arquivo_saida) {
            int descritor_saida = open(arquivo_saida, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (descritor_saida < 0) {
                perror("Erro ao abrir arquivo de saída");
                exit(EXIT_FAILURE);
            }
            dup2(descritor_saida, STDOUT_FILENO);
            close(descritor_saida);
        }

        if (execvp(argumentos[0], argumentos) < 0) {
            perror("Erro ao executar comando");
            exit(EXIT_FAILURE);
        }
    } else {
        if (!em_segundo_plano) {
            waitpid(id_processo, NULL, 0);
        }
    }
}

void executar_pipe(char *linha_original_comando, int pipe_em_segundo_plano) {
    char *comandos_pipe[MAX_CMDS];
    int numero_comandos = 0;
    char *copia_linha = strdup(linha_original_comando);
    char *ponteiro_salvo_pipe;
    char *token_atual = strtok_r(copia_linha, "|", &ponteiro_salvo_pipe);

    while (token_atual != NULL && numero_comandos < MAX_CMDS) {
        comandos_pipe[numero_comandos++] = token_atual;
        token_atual = strtok_r(NULL, "|", &ponteiro_salvo_pipe);
    }

    int descritores_pipe[2];
    int entrada_pipe = 0;
    pid_t ids_processos[MAX_CMDS];

    for (int indice = 0; indice < numero_comandos; indice++) {
        char *argumentos_comando[MAX_ARGS];
        int comando_em_segundo_plano = 0;
        char *arquivo_entrada_comando = NULL;
        char *arquivo_saida_comando = NULL;

        char *copia_string_comando = strdup(comandos_pipe[indice]);

        analisar_comando(copia_string_comando, argumentos_comando, &comando_em_segundo_plano, &arquivo_entrada_comando, &arquivo_saida_comando);

        if (argumentos_comando[0] == NULL) {
            fprintf(stderr, "Comando vazio no pipe.\n");
            free(copia_string_comando);
            continue;
        }

        if (indice < numero_comandos - 1) {
            if (pipe(descritores_pipe) < 0) {
                perror("Erro ao criar pipe");
                free(copia_linha);
                free(copia_string_comando);
                return;
            }
        }

        ids_processos[indice] = fork();
        if (ids_processos[indice] < 0) {
            perror("Erro ao criar processo");
            free(copia_linha);
            free(copia_string_comando);
            return;
        }

        if (ids_processos[indice] == 0) {
            if (entrada_pipe != 0) {
                dup2(entrada_pipe, STDIN_FILENO);
                close(entrada_pipe);
            }

            if (indice < numero_comandos - 1) {
                dup2(descritores_pipe[1], STDOUT_FILENO);
                close(descritores_pipe[0]);
                close(descritores_pipe[1]);
            }

            if (arquivo_entrada_comando) {
                int descritor_entrada = open(arquivo_entrada_comando, O_RDONLY);
                if (descritor_entrada < 0) {
                    perror("Erro ao abrir arquivo de entrada no pipe");
                    exit(EXIT_FAILURE);
                }
                dup2(descritor_entrada, STDIN_FILENO);
                close(descritor_entrada);
            }

            if (arquivo_saida_comando) {
                int descritor_saida = open(arquivo_saida_comando, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (descritor_saida < 0) {
                    perror("Erro ao abrir arquivo de saída no pipe");
                    exit(EXIT_FAILURE);
                }
                dup2(descritor_saida, STDOUT_FILENO);
                close(descritor_saida);
            }

            if (execvp(argumentos_comando[0], argumentos_comando) < 0) {
                perror("Erro ao executar comando");
                exit(EXIT_FAILURE);
            }

        } else {
            if (entrada_pipe != 0) close(entrada_pipe);
            if (indice < numero_comandos - 1) {
                close(descritores_pipe[1]);
                entrada_pipe = descritores_pipe[0];
            }
            free(copia_string_comando);
        }
    }

    free(copia_linha);

    if (!pipe_em_segundo_plano) {
        for (int indice = 0; indice < numero_comandos; indice++) {
            waitpid(ids_processos[indice], NULL, 0);
        }
    }
}

int main() {
    char comando_lido[BUFFER];
    char *argumentos_main[MAX_ARGS];
    int em_segundo_plano_main;
    char *arquivo_entrada_main;
    char *arquivo_saida_main;

    while (1) {
        ler_comando(comando_lido);

        if (strcmp(comando_lido, "exit") == 0) {
            for (int indice = 0; indice < contador_historico; indice++) {
                free(historico[indice]);
            }
            free(historico);
            break;
        }

        if (strlen(comando_lido) > 0) {
            adicionar_comando_historico(comando_lido);
        }

        if (strcmp(comando_lido, "history") == 0) {
            mostrar_historico();
            continue;
        }

        if (strchr(comando_lido, '|')) {
            char copia_comando_para_pipe[BUFFER];
            strcpy(copia_comando_para_pipe, comando_lido);
            executar_pipe(copia_comando_para_pipe, 0);
        } else {
            char copia_comando_para_simples[BUFFER];
            strcpy(copia_comando_para_simples, comando_lido);

            if (analisar_comando(copia_comando_para_simples, argumentos_main, &em_segundo_plano_main, &arquivo_entrada_main, &arquivo_saida_main)) {
                executar_simples(argumentos_main, em_segundo_plano_main, arquivo_entrada_main, arquivo_saida_main);
            }
        }

    }

    return 0;
}