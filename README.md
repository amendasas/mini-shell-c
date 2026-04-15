# Mini Shell em C

Projeto desenvolvido para a disciplina de Sistemas Operacionais.

## Funcionalidades

- Execução de comandos do sistema
- Criação de processos com fork e execvp
- Redirecionamento de entrada e saída (<, >)
- Execução em segundo plano (&)
- Pipes (|) entre comandos
- Histórico de comandos

## Como compilar

gcc shell.c -o shell

## Como executar

./shell

## Exemplo

ls -l | grep .c
