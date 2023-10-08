/* Header file com definições sobre bateria */

#ifndef BATERIA_DEFS
#define BATERIA_DEFS

#define NUMERO_LEITURAS_BATERIA          10

/* Relação de tensão x carga da bateria */
#define PONTOS_MAPEADOS_BATERIA   11

#endif

char cargas_mapeadas[PONTOS_MAPEADOS_BATERIA] = { 0,
                                                  3, 
                                                  13,
                                                  22,
                                                  39,
                                                  53,
                                                  62,
                                                  74,
                                                  84,
                                                  94,
                                                  100 };
                             
float tensao_x_carga[PONTOS_MAPEADOS_BATERIA] = {3.2,   //0%
                                                 3.3,   //3%
                                                 3.4,   //13%
                                                 3.5,   //22%
                                                 3.6,   //39%
                                                 3.7,   //53%
                                                 3.8,   //62%
                                                 3.9,   //74%
                                                 4.0,   //84%
                                                 4.1,   //94%
                                                 4.2 }; //100%
