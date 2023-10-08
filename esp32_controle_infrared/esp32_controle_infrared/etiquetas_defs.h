/* Header file com definições das etiquetas do projeto */

#ifndef ETIQUETAS_DEFS
#define ETIQUETAS_DEFS

/* Definições - tamanho dos campos */
#define TAMANHO_NOME_PRODUTO           15
#define TAMANHO_PRECO_PRODUTO          8
#define TAMANHO_QTDE_OU_PESO           4
#define TAMANHO_COMPLEMENTO            10
#define TAMANHO_TITULO_OFERTA          50

/* Estrutura de etiqueta para produtos por unidade ou kg */
typedef struct __attribute__((__packed__))
{
    char nome_produto[TAMANHO_NOME_PRODUTO];
    char preco_produto[TAMANHO_PRECO_PRODUTO];
    char un_ou_kg[TAMANHO_QTDE_OU_PESO];    
}TEtiqueta_un_ou_kg;

/* Estrutura de etiqueta para ofertas */
typedef struct __attribute__((__packed__))
{
    char titulo_oferta[TAMANHO_TITULO_OFERTA];
    char nome_produto[TAMANHO_NOME_PRODUTO];
    char preco_produto[TAMANHO_PRECO_PRODUTO];
    char complemento_produto[TAMANHO_COMPLEMENTO];    
}TEtiqueta_oferta;

#endif
