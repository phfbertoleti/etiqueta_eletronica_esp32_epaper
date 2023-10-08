/* Software: etiqueta eletrônica com ESP32 e display ePaper
 * Autor: Pedro Bertoleti
 */

/* Definição da placa utilizada */
#define LILYGO_T5_V213

#include <boards.h>
#include <GxEPD.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include "epaper_defs.h"
#include "etiquetas_defs.h"
#include "serial_infrared_defs.h"
#include "bateria_defs.h"

/* Definição - tempo para dispositivo acordar */
#define FATOR_US_PARA_S      (unsigned long)1000000
#define TEMPO_DE_SLEEP       (unsigned long)3600      

/* Definição - baudrate da serial de debug */
#define SERIAL_DEBUG_BAUDRATE        115200

/* Definição - bitmask do gpio de wakeup (GPIO33) */
#define WAKEUP_BITMASK                0x200000000 /* = 2^33 */

/* Variaveis persistentes mesmo em deep sleep */
RTC_DATA_ATTR TEtiqueta_un_ou_kg ultima_etiqueta_un_ou_kg;
RTC_DATA_ATTR TEtiqueta_oferta ultima_etiqueta_oferta;
RTC_DATA_ATTR char ultimo_tipo_etiqueta;

/* Variáveis globais */
unsigned long timestamp_recepcao_serial;
esp_adc_cal_characteristics_t adc_cal; /* Variável de estrutura que contem as informacoes para calibracao */

/* Protótipos */
bool maquina_estados_serial_IR(char byte_recebido);
bool maquina_estados_resetada = false;
unsigned short calcula_crc (unsigned char *ptr, unsigned int lng);
void escreve_etiqueta_un_kg(TEtiqueta_un_ou_kg etiqueta);
void escreve_etiqueta_oferta(TEtiqueta_oferta etiqueta);
unsigned long diferenca_tempo(unsigned long tref);
void desenha_etiqueta(unsigned char tipo_etiqueta);
int calculo_carga_bateria(float tensao_bateria);
void configura_adc_bateria(void);
float le_tensao_bateria(void);
void mostra_display_carga_bateria(void);
void desenha_ultima_etiqueta_valida(void);

/*
 * Implementações
 */
/* Função: máquina de estados da recepção serial via infrared
 * Parâmetros: byte recebido
 * Retorno: true: recepção completa e íntegra
 *          false: recepção incompleta ou não-íntegra
 */
bool maquina_estados_serial_IR(char byte_recebido)
{
    bool status_recepcao = false;
    unsigned short crc_recebido = 0x0000;
    unsigned short crc_calculado = 0x0000;
    
    switch (estado)
    {
        case ESTADO_BYTE_1_SINCRONIA:
            Serial.println("Estado: ESTADO_BYTE_1_SINCRONIA");
            if (byte_recebido == BYTE_1_SINCRONIA)
            {
                frame_comunicacao.byte_1_sincronia = byte_recebido;
                estado = ESTADO_BYTE_2_SINCRONIA;
            }
            
            break;

        case ESTADO_BYTE_2_SINCRONIA:
            Serial.println("Estado: ESTADO_BYTE_2_SINCRONIA");
            if (byte_recebido == BYTE_2_SINCRONIA)
            {
                frame_comunicacao.byte_2_sincronia = byte_recebido;
                estado = ESTADO_BYTE_3_SINCRONIA;
            }
            else
            {
                estado = ESTADO_BYTE_1_SINCRONIA;
            }
            
            break;    

         case ESTADO_BYTE_3_SINCRONIA:
            Serial.println("Estado: ESTADO_BYTE_3_SINCRONIA"); 
            if (byte_recebido == BYTE_3_SINCRONIA)
            {
                frame_comunicacao.byte_3_sincronia = byte_recebido;
                estado = ESTADO_BYTE_4_SINCRONIA;
            }
            else
            {
                estado = ESTADO_BYTE_1_SINCRONIA;
            }
            
            break;   

         case ESTADO_BYTE_4_SINCRONIA:
            Serial.println("Estado: ESTADO_BYTE_4_SINCRONIA");
            if (byte_recebido == BYTE_4_SINCRONIA)
            {
                frame_comunicacao.byte_4_sincronia = byte_recebido;
                estado = ESTADO_BYTE_5_SINCRONIA;
            }
            else
            {
                estado = ESTADO_BYTE_1_SINCRONIA;
            }
            
            break;   

        case ESTADO_BYTE_5_SINCRONIA:
            Serial.println("Estado: ESTADO_BYTE_5_SINCRONIA");
            if (byte_recebido == BYTE_5_SINCRONIA)
            {
                frame_comunicacao.byte_5_sincronia = byte_recebido;
                estado = ESTADO_BYTE_6_SINCRONIA;
            }
            else
            {
                estado = ESTADO_BYTE_1_SINCRONIA;
            }
            
            break;      

        case ESTADO_BYTE_6_SINCRONIA:
            Serial.println("Estado: ESTADO_BYTE_6_SINCRONIA");
            if (byte_recebido == BYTE_6_SINCRONIA)
            {
                frame_comunicacao.byte_6_sincronia = byte_recebido;
                estado = ESTADO_ENDERECO_LOW;
            }
            else
            {
                estado = ESTADO_BYTE_1_SINCRONIA;
            }
            
            break;  

        case ESTADO_ENDERECO_LOW:
            Serial.println("Estado: ESTADO_ENDERECO_LOW");
            if (byte_recebido == ENDERECO_ETIQUETA_LOW)
            {
                frame_comunicacao.endereco_low = byte_recebido;
                estado = ESTADO_ENDERECO_HIGH;
            }
            else
            {
                estado = ESTADO_BYTE_1_SINCRONIA;
            }
            
            break;    

        case ESTADO_ENDERECO_HIGH:
            Serial.println("Estado: ESTADO_ENDERECO_HIGH");
            if (byte_recebido == ENDERECO_ETIQUETA_HIGH)
            {
                frame_comunicacao.endereco_high = byte_recebido;
                estado = ESTADO_TIPO_ETIQUETA;
            }
            else
            {
                estado = ESTADO_BYTE_1_SINCRONIA;
            }
            
            break;            

        case ESTADO_TIPO_ETIQUETA:
            Serial.println("Estado: ESTADO_TIPO_ETIQUETA");
            if ( (byte_recebido == TIPO_ETIQUETA_UNIDADE_OU_KG) || 
                 (byte_recebido == TIPO_ETIQUETA_OFERTA) )
            {
                frame_comunicacao.tipo_etiqueta = byte_recebido;
                estado = ESTADO_TAMANHO_DADOS;
            }
            else
            {
                estado = ESTADO_BYTE_1_SINCRONIA;
            }
            
            break;  

        case ESTADO_TAMANHO_DADOS:
            Serial.println("Estado: ESTADO_TAMANHO_DADOS");
            frame_comunicacao.tamanho_dados = byte_recebido;
            idx_buffer_etiqueta = 0;
            tamanho_para_receber = (int)frame_comunicacao.tamanho_dados;
            memset(buffer_etiqueta, 0x00, sizeof(buffer_etiqueta));
            estado = ESTADO_DADOS_ETIQUETA;
            
            break;    


        case ESTADO_DADOS_ETIQUETA:
            Serial.println("Estado: ESTADO_DADOS_ETIQUETA");
            if (tamanho_para_receber > 1)
            {
                buffer_etiqueta[idx_buffer_etiqueta] = byte_recebido;
                idx_buffer_etiqueta++;
                tamanho_para_receber--;  
            }
            else
            {
                /* Último byte */
                buffer_etiqueta[idx_buffer_etiqueta] = byte_recebido;
                tamanho_para_receber--;
                estado = ESTADO_CRC_LOW;
            }
            
            break; 
            
        case ESTADO_CRC_LOW:
            Serial.println("Estado: ESTADO_CRC_LOW");
            frame_comunicacao.crc_low = byte_recebido;
            estado = ESTADO_CRC_HIGH;
            break;  

        case ESTADO_CRC_HIGH:
            Serial.println("Estado: ESTADO_CRC_HIGH");
            frame_comunicacao.crc_high = byte_recebido;
            crc_recebido = frame_comunicacao.crc_high;
            crc_recebido = crc_recebido << 8;
            crc_recebido = crc_recebido | frame_comunicacao.crc_low;
            crc_calculado = calcula_crc(buffer_etiqueta, frame_comunicacao.tamanho_dados);

            if (crc_recebido == crc_calculado)
            {
                status_recepcao = true;
                Serial.println("CRC ok");
            }
            else
            {
                status_recepcao = false;
                Serial.println("Falha de CRC");
            }
            
            estado = ESTADO_BYTE_1_SINCRONIA;
            break;  
    
    }

    return status_recepcao;
}

/* Função: calcua CRC de um buffer desejado
 * Parâmetros: ponteiro para o buffer e tamanho do buffer
 * Retorno: CRC calculado
 */
unsigned short calcula_crc(unsigned char *ptr, unsigned int lng)
{
    unsigned int i, j;
    unsigned short CRC;

    CRC = 0xFFFF;  // initialize CRC
    for (i = 0; i < lng; i++)
    {
        CRC ^= (unsigned short)*(ptr + i);  
        for (j = 0; j < 8; j++)
        {
            if (CRC & 1)
            { 
                // test LSB
                CRC >>= 1;  
                CRC ^= 0xA001;  
            }
            else
            {
                CRC >>= 1; 
            }
        }
    }
    return CRC; 
}

/* Função: escreve etiqueta (unidade ou kg) no display
 * Parâmetros: estrutura com os campos da etiqueta
 * Retorno: nenhum
 */
void escreve_etiqueta_un_kg(TEtiqueta_un_ou_kg etiqueta)
{
    display.setRotation(1);
    display.setTextColor(GxEPD_BLACK);
    display.fillScreen(GxEPD_WHITE);

    /* Nome do produto */
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(15,15);
    display.println(etiqueta.nome_produto);

    /* Preço */
    display.setFont(&FreeSerifBold24pt7b);
    display.setCursor(15,65);
    display.println(etiqueta.preco_produto);

    /* Unidade ou kg */
    display.setFont(&FreeSerifItalic12pt7b);
    display.setCursor(180,85);
    display.println(etiqueta.un_ou_kg);

    /* Escreve a carga da bateria */
    mostra_display_carga_bateria();

    /* Deixa em RAM persistente a ultima etiqueta de unidade ou kg */
    ultima_etiqueta_un_ou_kg = etiqueta;
    ultimo_tipo_etiqueta = TIPO_ETIQUETA_UNIDADE_OU_KG;

    display.update();
}

/* Função: escreve etiqueta (oferta) no display
 * Parâmetros: estrutura com os campos da etiqueta
 * Retorno: nenhum
 */
void escreve_etiqueta_oferta(TEtiqueta_oferta etiqueta)
{
    display.setRotation(1);
    display.fillScreen(GxEPD_WHITE);
    
    /* Titulo da oferta */
    display.setTextColor(GxEPD_WHITE);
    display.fillRect(0, 0, 212, 17, GxEPD_BLACK);
    display.setCursor(15,8);
    display.setFont(&FreeSerif9pt7b);
    display.println(etiqueta.titulo_oferta);
    
    /* Acerta cor da fonte para as outras informações */
    display.setTextColor(GxEPD_BLACK);
    
    /* Nome do produto */
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(5,40);
    display.println(etiqueta.nome_produto);

    /* Preço */
    display.setFont(&FreeSerifBold24pt7b);
    display.setCursor(15,80);
    display.println(etiqueta.preco_produto);

    /* Complemento */
    display.setFont(&FreeSerifItalic12pt7b);
    display.setCursor(130,100);
    display.println(etiqueta.complemento_produto);
    
    /* Escreve a carga da bateria */
    mostra_display_carga_bateria();

    /* Deixa em RAM persistente a ultima etiqueta de oferta */
    ultima_etiqueta_oferta = etiqueta;
    ultimo_tipo_etiqueta = TIPO_ETIQUETA_OFERTA;

    display.update();    
}

/* Função: calcula a diferença de tempo (em ms) entre o instante
 *         atual e uma referencia
 * Parâmetros: referência de tempo
 * Retorno:  diferença de tempo calculada
 */
unsigned long diferenca_tempo(unsigned long tref)
{
    return (millis() - tref);
}

/* Função: desenha etiqueta conforme dados recebidos
 *         na comunicação serial via infrared
 * Parâmetros: tipo de etiqueta
 * Retorno:  nenhum
 */
void desenha_etiqueta(unsigned char tipo_etiqueta)
{
    switch(frame_comunicacao.tipo_etiqueta)
    {
        case TIPO_ETIQUETA_UNIDADE_OU_KG:
            Serial.println("Recebida uma etiqueta do tipo unidade ou kg. Escrevendo etiqueta no display...");
            TEtiqueta_un_ou_kg etiqueta_un_kg;
            memset((unsigned char *)&etiqueta_un_kg, 0x00, sizeof(TEtiqueta_un_ou_kg));
            memcpy((unsigned char *)&etiqueta_un_kg, buffer_etiqueta, sizeof(TEtiqueta_un_ou_kg));
            escreve_etiqueta_un_kg(etiqueta_un_kg);
            
            break;

        case TIPO_ETIQUETA_OFERTA:
            Serial.println("Recebida uma etiqueta do tipo oferta. Escrevendo etiqueta no display...");
            TEtiqueta_oferta etiqueta_oferta;
            memset((unsigned char *)&etiqueta_oferta, 0x00, sizeof(TEtiqueta_oferta));
            memcpy((unsigned char *)&etiqueta_oferta, buffer_etiqueta, sizeof(TEtiqueta_oferta));
            escreve_etiqueta_oferta(etiqueta_oferta);

            break;

        default:
            break;          
    }
}

/* Função: configura ADC para leitura da tensão de bateria
 * Parâmetros: nenhum
 * Retorno:  nenhum
 */
void configura_adc_bateria(void)
{
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_7,ADC_ATTEN_DB_11);
    
    esp_adc_cal_value_t adc_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_cal);
}

/* Função: le tensão da bateria
 * Parâmetros: nenhum
 * Retorno:  tensão da bateria (V)
 */
float le_tensao_bateria(void)
{
    unsigned long leitura_adc_bateria = 0;
    unsigned long soma_leitura_adc_bateria = 0;
    float tensao_bateria = 0.0;
    float tensao_adc = 0.0;
    int i;

    for(i=0; i<NUMERO_LEITURAS_BATERIA; i++)
    {
        leitura_adc_bateria = adc1_get_raw(ADC1_CHANNEL_7);
        soma_leitura_adc_bateria = soma_leitura_adc_bateria + leitura_adc_bateria;
    }

    leitura_adc_bateria = soma_leitura_adc_bateria / NUMERO_LEITURAS_BATERIA;
    tensao_adc = esp_adc_cal_raw_to_voltage(leitura_adc_bateria, &adc_cal);  //unidade: mV
    tensao_adc = tensao_adc / 1000.0; //unidade: V

    /*         bateria                 adc
                  4.1V     -------     2.05V 
           tensao_bateria  -------     tensao_adc  
       tensao_bateria = tensao_adc*(4.1/2.05)
    */

    tensao_bateria = tensao_adc*(4.1/2.05);
    
    return tensao_bateria;
}


/* Função: calcula a porcentagem de carga restante da bateria
 * Parâmetros: tensão da bateria 
 * Retorno: porcentagem de carga restante da bateria (0..100%)
 */
int calculo_carga_bateria(float tensao_bateria)
{
    int carga_bateria = 0;
    float carga_bateria_float = 0.0;
    int i;
    int idx_menor_distancia;
    bool carga_calculada = false;
    float distancias[(PONTOS_MAPEADOS_BATERIA-1)] = {0.0};
    float menor_distancia;
    float x0, y0, x1, y1, m;

    /* Verifica se a tensão da bateria já está nos niveis mapeados */
    for (i=0; i<PONTOS_MAPEADOS_BATERIA; i++)
    {
        if (i < (PONTOS_MAPEADOS_BATERIA - 1))
            distancias[i] = abs(tensao_bateria - tensao_x_carga[i]);
        
        if (tensao_x_carga[i] == tensao_bateria)
        {
            carga_bateria_float = (float)cargas_mapeadas[i];
            carga_calculada = true;
            break;
        }
    }

    if ( carga_calculada == false)
    {
        /* Se a tensão da bateria não está nos níveis mapeados,
           calcula a carga com base na interpolação linear entre os dois níveis
           mapeados mais próximos */
        menor_distancia = distancias[0];
        idx_menor_distancia = 0;
        
        for (i=1; i<(PONTOS_MAPEADOS_BATERIA - 1); i++)
        {
            if ( distancias[i] < menor_distancia )
            {
                menor_distancia = distancias[i];
                idx_menor_distancia = i;                
            }
        }

        //tensão: eixo x
        //carga: eixo y
        //tensao mais prox da mapeada: x0
        //carga mais prox da mapeada: y0
        //tensão mapeada imediatamente acima: x1
        //carga mapeada imediatamente acima: y1
        //Coeficiente angular da reta que passa pelos dois níveis mapeados mais próximos: m = (y1-y0) / (x1 - x0)
        //equação de reta: y = m*(x-x0) + y0 -> y = m*tensoa_bateria -m*x0 + y0

        x0 = tensao_x_carga[idx_menor_distancia];
        y0 = cargas_mapeadas[idx_menor_distancia];
        x1 = tensao_x_carga[idx_menor_distancia + 1];
        y1 = cargas_mapeadas[idx_menor_distancia + 1];        
        m = ( (y1-y0) / (x1 - x0) );       
        carga_bateria_float = ((m*tensao_bateria) - (m*x0) + y0);        
    }

    /* Caso a bateria esteja totalmente carregada, ainterpolação seja feita nos dois últimos níveis mapeados. 
     * Nesse caso, se o ADC apresentar algum erro de leitura para cima, a carga calculada poderá ser ligeiramente
     * maior que 100%. Nesse caso, trava-se a carga em 100%.  
     */
    if (carga_bateria_float > 100.0)
        carga_bateria_float = 100.0;

    carga_bateria = (int)carga_bateria_float;
    return carga_bateria;
}

/* Função: mostra no display a carga da bateria
 * Parâmetros: nenhum
 * Retorno: nenhum
 */
void mostra_display_carga_bateria(void)
{  
    float tensao_bateria;
    int carga_bateria;
    int altura_indicador = 8;
    int largura_indicador = 20;
    char str_bat[5] = {0};

    /* Le a tensão e carga percentual da bateria */
    tensao_bateria = le_tensao_bateria();
    Serial.print("Tensao da bateria: ");
    Serial.println(tensao_bateria);
    carga_bateria = calculo_carga_bateria(tensao_bateria);
    Serial.print("Carga percentual da bateria: ");
    Serial.print(carga_bateria);
    Serial.println("%");
    
    /* Mostra no display a carga da bateria */
    sprintf(str_bat, "%d%%", carga_bateria);
    display.setRotation(1);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeSerif9pt7b);
    display.setCursor(5,95);
    display.println(str_bat); 
}

/* Função: desenha a ultima etiqueta válida recebida,
 *         ou uma etiqueta de teste caso a ultima etiqueta
 *         válida nao exista.
 * Parâmetros: nenhum
 * Retorno: nenhum
 */
void desenha_ultima_etiqueta_valida(void)
{
    switch (ultimo_tipo_etiqueta)
    {
        case TIPO_ETIQUETA_UNIDADE_OU_KG:
            Serial.println("Ultimo tipo de etiqueta: unidade ou kg");
            escreve_etiqueta_un_kg(ultima_etiqueta_un_ou_kg);
            break;

        case TIPO_ETIQUETA_OFERTA:
            Serial.println("Ultimo tipo de etiqueta: oferta");
            escreve_etiqueta_oferta(ultima_etiqueta_oferta);
            break;   

        default:
            Serial.println("Ultimo tipo de etiqueta: desconhecida. Escrevendo etiqueta de teste.");
            
            TEtiqueta_un_ou_kg etiqueta_teste;
            sprintf(etiqueta_teste.nome_produto, "Teste");
            sprintf(etiqueta_teste.preco_produto, "R$12,34");
            sprintf(etiqueta_teste.un_ou_kg, "AB");
            escreve_etiqueta_un_kg(etiqueta_teste);
            break;     
    }
}

void setup() 
{
    esp_sleep_wakeup_cause_t wakeup_reason;
    
    Serial.begin(SERIAL_DEBUG_BAUDRATE);
    Serial.println();
    Serial.println("Etiqueta eletronica - inicializada");

    /* Configura ADC para leitura de tensão da bateria */
    configura_adc_bateria();

    /* Inicializa comunicação com o display e-paper */
    SPI.begin(EPD_SCLK, EPD_MISO, EPD_MOSI, EPD_CS);
    display.init();
    
    wakeup_reason = esp_sleep_get_wakeup_cause();
  
    switch(wakeup_reason)
    {
      case ESP_SLEEP_WAKEUP_EXT0 : 
          Serial.println("Causa do Wakeup: interrupcao externa (EXT0)"); 
          break;
          
      case ESP_SLEEP_WAKEUP_TIMER : 
          Serial.println("Causa do Wakeup: timer"); 
          Serial.println("Redesenhando ultima tela valida..."); 

          /* Caso acorde por timer, redesenha última etiqueta valida
             (com indicador de bateria) 
          */
          desenha_ultima_etiqueta_valida();

          break;
          
      default : 
          Serial.printf("Causa do Wakeup: desconhecida (%d) \n",wakeup_reason); 
          break;
    }

    /* Configura como fontes de wake-up:
       - Estimulo noo GPIO33 (quando este estiver em nivel baixo) 
       - Timer
    */
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_33, 0);
    esp_sleep_enable_timer_wakeup(TEMPO_DE_SLEEP * FATOR_US_PARA_S);    

    /* Configura serial de comunicação via infrared */
    Serial2.begin(SERIAL_IR_BAUDRATE, 
                  SERIAL_IR_CONFIG, 
                  SERIAL_IR_GPIO_RX, 
                  SERIAL_IR_GPIO_TX);

    /* Inicializa temporização serial */
    timestamp_recepcao_serial = millis();
}

void loop() 
{
    char byte_recebido;
    
    /* Recebe byte por byte da serial de comunicação infrared e processa
       os dados recebidos */
    if (Serial2.available() > 0)
    {
        byte_recebido = Serial2.read();
        timestamp_recepcao_serial = millis();
        Serial.println("Byte recebido");

        if (maquina_estados_serial_IR(byte_recebido) == true)
        {
            /* Etiqueta completa recebida.
               Desenha etiqueta conforme dados recebidos. */
            desenha_etiqueta(frame_comunicacao.tipo_etiqueta);
            
            Serial.println("ESP32 entrando em sleep...");
            delay(500);
            esp_deep_sleep_start();
        }
    }

    /* Se a última transmissão serial ocorreu a mais tempo que o definido em TIMEOUT_SERIAL_INFRARED,
       Reseta a máquina de estados */
    if ( diferenca_tempo(timestamp_recepcao_serial) >= TIMEOUT_SERIAL_INFRARED )
    {
        estado = ESTADO_BYTE_1_SINCRONIA;

        if (maquina_estados_resetada == true)
        {
            Serial.println("Timeout de recepção - maquina de estado resetada");
            maquina_estados_resetada = false;

            Serial.println("ESP32 entrando em sleep...");
            delay(500);
            esp_deep_sleep_start();
        }    
    }
    else
    {
        maquina_estados_resetada = true;
    }
}
