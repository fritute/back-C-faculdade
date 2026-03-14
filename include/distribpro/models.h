#ifndef DISTRIBPRO_MODELS_H
#define DISTRIBPRO_MODELS_H

typedef struct {
    int id;
    char nome[200];
    char email[200];
    char senha_hash[255];
    char role[20]; /* "admin" ou "operador" */
    char criado_em[30];
    char atualizado_em[30];
} User;

typedef struct {
    int id;
    char nome[200];
    char categoria[50];
    char unidade[30];
    double preco;
    int estoque;
    int estoque_min;
    char sku[100];
    int fornecedor_id;
    char status[20];
    char *descricao;
    char criado_em[30];
    char atualizado_em[30];
} Produto;

typedef struct {
    int id;
    char nome[200];
    char tipo[3]; /* "PJ" ou "PF" */
    char doc[30];
    char email[200];
    char tel[30];
    char cidade[100];
    char estado[3];
    double limite;
    char status[20];
    char criado_em[30];
    char atualizado_em[30];
} Cliente;

typedef struct {
    int id;
    char nome[200];
    char cnpj[20];
    char contato[150];
    char email[200];
    char tel[30];
    char categoria[50];
    int prazo;
    char status[20];
    char criado_em[30];
    char atualizado_em[30];
} Fornecedor;

typedef struct {
    int id;
    int cliente_id;
    int produto_id;
    int qtd;
    double valor;
    char destino[300];
    char data_entrega[30];
    char status[20];
    char *obs;
    char criado_em[30];
    char atualizado_em[30];
} Pedido;

typedef struct {
    int id;
    char razao_social[200];
    char cnpj[20];
    char email[200];
    char tel[30];
    char endereco[300];
    char atualizado_em[30];
} ConfigEmpresa;

#endif // DISTRIBPRO_MODELS_H
