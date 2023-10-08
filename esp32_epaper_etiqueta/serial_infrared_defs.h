/* Header file com definições da comunicação serial via infrared */

#ifndef SERIAL_INFRARED_DEFS
#define SERIAL_INFRARED_DEFS

/* Definições - UART de comunicação infrared */
#define SERIAL_IR_GPIO_TX                19
#define SERIAL_IR_GPIO_RX                21
#define SERIAL_IR_CONFIG                 SERIAL_8N1
#define SERIAL_IR_BAUDRATE               200

/* Definição - tamanho máximo do buffer de etiqueta */
#define TAMANHO_MAX_ETIQUETA        255

/* Definições - bytes de sincronia */
#define BYTE_1_SINCRONIA            'E'
#define BYTE_2_SINCRONIA            'T'
#define BYTE_3_SINCRONIA            'Q'
#define BYTE_4_SINCRONIA            'C'
#define BYTE_5_SINCRONIA            'O'
#define BYTE_6_SINCRONIA            'M'

/* Definição - timeout */
#define TIMEOUT_SERIAL_INFRARED           2000 //ms

/* Definição - endereço */
#define ENDERECO_ETIQUETA_HIGH            0x00
#define ENDERECO_ETIQUETA_LOW             0x01


/* Definições - estados */
#define ESTADO_BYTE_1_SINCRONIA              0x01
#define ESTADO_BYTE_2_SINCRONIA              0x02
#define ESTADO_BYTE_3_SINCRONIA              0x03
#define ESTADO_BYTE_4_SINCRONIA              0x04
#define ESTADO_BYTE_5_SINCRONIA              0x05
#define ESTADO_BYTE_6_SINCRONIA              0x06
#define ESTADO_ENDERECO_LOW                  0x07
#define ESTADO_ENDERECO_HIGH                 0x08
#define ESTADO_TIPO_ETIQUETA                 0x09
#define ESTADO_TAMANHO_DADOS                 0x0A
#define ESTADO_DADOS_ETIQUETA                0x0B
#define ESTADO_CRC_LOW                       0x0C
#define ESTADO_CRC_HIGH                      0x0D

/* Estrutura de comunicação */
typedef struct __attribute__((__packed__))
{
    unsigned char byte_1_sincronia;
    unsigned char byte_2_sincronia;
    unsigned char byte_3_sincronia;
    unsigned char byte_4_sincronia;
    unsigned char byte_5_sincronia;
    unsigned char byte_6_sincronia;
    unsigned char endereco_low;
    unsigned char endereco_high;
    unsigned char tipo_etiqueta;
    unsigned char tamanho_dados;    
    unsigned char crc_low;
    unsigned char crc_high;
}TEstrutura_comunicacao;

#endif

TEstrutura_comunicacao frame_comunicacao;
unsigned char buffer_etiqueta[TAMANHO_MAX_ETIQUETA] = {0};
int idx_buffer_etiqueta = 0;
int tamanho_para_receber = 0;
char estado = ESTADO_BYTE_1_SINCRONIA;
